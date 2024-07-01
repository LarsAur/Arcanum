#include <tuning/fengen.hpp>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <search.hpp>
#include <syzygy.hpp>
#include <fen.hpp>

using namespace Arcanum;

enum Result
{
    BLACK_WIN = -1,
    DRAW = 0,
    WHITE_WIN = 1,
};

std::vector<hash_t> s_materialDraws;

void m_setupMaterialDraws()
{
    // Setup a table of known endgame draws based on material
    // This is based on: https://www.chess.com/terms/draw-chess
    Board kings = Board("K1k5/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsWBishop = Board("K1k1B3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsBBishop = Board("K1k1b3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsWKnight = Board("K1k1N3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsBKnight = Board("K1k1n3/8/8/8/8/8/8/8 w - - 0 1");
    s_materialDraws.push_back(kings.getMaterialHash());
    s_materialDraws.push_back(kingsWBishop.getMaterialHash());
    s_materialDraws.push_back(kingsBBishop.getMaterialHash());
    s_materialDraws.push_back(kingsWKnight.getMaterialHash());
    s_materialDraws.push_back(kingsBKnight.getMaterialHash());
}

bool m_isFinished(Board& board, Searcher& searcher, Result& result)
{
    auto history = searcher.getHistory();
    if(history.at(board.getHash()) > 2)
    {
        result = DRAW;
        return true;
    }

    if(board.getNumPiecesLeft() <= 3)
    {
        for(auto it = s_materialDraws.begin(); it != s_materialDraws.end(); it++)
        {
            if(*it == board.getMaterialHash())
            {
                result = DRAW;
                return true;
            }
        }
    }

    board.getLegalMoves();
    if(board.getNumLegalMoves() == 0)
    {
        if(board.isChecked())
        {
            result = (board.getTurn() == WHITE) ? BLACK_WIN : WHITE_WIN;
        }
        else
        {
            result = DRAW;
        }

        return true;
    }

    // Check for 50 move rule
    if(board.getHalfMoves() >= 100)
    {
        result = DRAW;
        return true;
    }

    return false;
}

void Tuning::fengen(std::string startPosPath, std::string outputPath, size_t numFens, uint8_t numThreads, uint32_t depth)
{
    std::vector<std::thread> threads;
    std::mutex readLock;
    std::mutex writeLock;
    size_t fenCount = 0LL;

    std::ifstream posStream = std::ifstream(startPosPath);

    if(!posStream.is_open())
    {
        ERROR("Unable to open " << startPosPath)
        return;
    }

    std::ofstream outStream = std::ofstream(outputPath, std::ios::app);

    if(!outStream.is_open())
    {
        ERROR("Unable to open " << outputPath)
        posStream.close();
        return;
    }

    auto fn = [&](uint8_t id)
    {
        std::vector<std::string> fens;
        std::vector<eval_t> evals;
        std::string startPostion;
        Searcher searcher = Searcher();
        searcher.setVerbose(false);

        while (true)
        {
            readLock.lock();

            if(posStream.eof() || fenCount > numFens)
            {
                readLock.unlock();
                break;
            }

            std::getline(posStream, startPostion);

            readLock.unlock();

            startPostion.insert(startPostion.size() - 1, "0 1");

            Board startboard = Board(startPostion);
            Move* moves = startboard.getLegalMoves();
            uint8_t numMoves = startboard.getNumLegalMoves();
            startboard.generateCaptureInfo();

            for(uint8_t i = 0; i < numMoves; i++)
            {
                Board board = Board(startboard);
                board.performMove(moves[i]);
                searcher.addBoardToHistory(board);

                Result result;
                while (!m_isFinished(board, searcher, result))
                {
                    SearchResult searchResult;
                    Move move = searcher.getBestMove(board, depth, &searchResult);

                    // Output the evaluation from the white perspective
                    if(board.getTurn() == WHITE)
                        evals.push_back(searchResult.eval);
                    else
                        evals.push_back(-searchResult.eval);


                    board.performMove(move);
                    searcher.addBoardToHistory(board);
                    fens.push_back(FEN::getFEN(board));

                    if(Evaluator::isTbCheckMateScore(searchResult.eval) || Evaluator::isCheckMateScore(searchResult.eval))
                    {
                        result = searchResult.eval > 0 ? (board.getTurn() == WHITE ? BLACK_WIN : WHITE_WIN) : (board.getTurn() == WHITE ? WHITE_WIN : BLACK_WIN);
                        break;
                    }
                    // If no mate is found and there are few enough pieces, it must be a draw
                    else if(TB_LARGEST >= (int) CNTSBITS(board.getColoredPieces(WHITE) | board.getColoredPieces(BLACK)))
                    {
                        result = DRAW;
                        break;
                    }
                }

                writeLock.lock();

                std::string strRes = std::to_string(result);

                for(size_t i = 0; i < fens.size(); i++)
                {
                    // Write result
                    outStream.write(strRes.c_str(), strRes.size());
                    outStream.write("\n", 1);
                    // Write eval
                    std::string strEval = std::to_string(evals.at(i));
                    outStream.write(strEval.c_str(), strEval.size());
                    outStream.write("\n", 1);
                    // Write FEN
                    outStream.write(fens.at(i).c_str(), fens.at(i).size());
                    outStream.write("\n", 1);
                }

                outStream.flush();

                fenCount += fens.size();
                if(fenCount % 100 == 0)
                {
                    DEBUG(fenCount)
                }
                writeLock.unlock();

                searcher.clearHistory();
                searcher.clear();
                fens.clear();
                evals.clear();
            }
        }
    };

    for(uint32_t i = 0; i < numThreads; i++)
    {
        threads.push_back(std::thread(fn, i));
    }

    for(uint32_t i = 0; i < numThreads; i++)
    {
        threads.at(i).join();
    }
}