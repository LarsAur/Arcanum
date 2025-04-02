#include <tuning/fengen.hpp>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <search.hpp>
#include <syzygy.hpp>
#include <fen.hpp>
#include <timer.hpp>

using namespace Arcanum;

void Fengen::m_setupMaterialDraws()
{
    // Setup a table of known endgame draws based on material
    // This is based on: https://www.chess.com/terms/draw-chess
    Board kings = Board("K1k5/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsWBishop = Board("K1k1B3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsBBishop = Board("K1k1b3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsWKnight = Board("K1k1N3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsBKnight = Board("K1k1n3/8/8/8/8/8/8/8 w - - 0 1");
    m_materialDraws.push_back(kings.getMaterialHash());
    m_materialDraws.push_back(kingsWBishop.getMaterialHash());
    m_materialDraws.push_back(kingsBBishop.getMaterialHash());
    m_materialDraws.push_back(kingsWKnight.getMaterialHash());
    m_materialDraws.push_back(kingsBKnight.getMaterialHash());
}

bool Fengen::m_isFinished(Board& board, Searcher& searcher, GameResult& result)
{
    auto history = searcher.getHistory();
    if(history.at(board.getHash()) > 2)
    {
        result = DRAW;
        return true;
    }

    if(board.getNumPieces() <= 3)
    {
        for(auto it = m_materialDraws.begin(); it != m_materialDraws.end(); it++)
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
            result = (board.getTurn() == WHITE) ? GameResult::BLACK_WIN : GameResult::WHITE_WIN;
        }
        else
        {
            result = GameResult::DRAW;
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

void Fengen::start(std::string startPosPath, std::string outputPath, size_t numFens, uint8_t numThreads, uint32_t depth, uint32_t movetime, uint32_t nodes)
{
    std::vector<std::thread> threads;
    std::mutex readLock;
    std::mutex writeLock;
    size_t fenCount = 0LL;
    Timer msTimer = Timer();

    // Set search parameters
    SearchParameters searchParams = SearchParameters();

    if(movetime > 0)
    {
        searchParams.useTime = true;
        searchParams.msTime = movetime;
    }

    if(depth > 0)
    {
        searchParams.useDepth = true;
        searchParams.depth = depth;
    }

    if(nodes > 0)
    {
        searchParams.useNodes = true;
        searchParams.nodes = nodes;
    }

    msTimer.start();

    std::ifstream posStream = std::ifstream(startPosPath);

    if(!posStream.is_open())
    {
        ERROR("Unable to open " << startPosPath)
        return;
    }

    DataStorer encoder = DataStorer();
    if(!encoder.open(outputPath))
    {
        ERROR("Unable to open " << outputPath)
        posStream.close();
        return;
    }

    auto fn = [&](uint8_t id)
    {
        std::string startfen;
        std::array<Move, DataEncoder::MaxGameLength> moves;
        std::array<eval_t, DataEncoder::MaxGameLength> scores;
        uint32_t numMoves;
        GameResult result;

        Searcher searchers[2] = {Searcher(false), Searcher(false)};
        while (true)
        {
            readLock.lock();

            if(posStream.eof() || fenCount > numFens)
            {
                readLock.unlock();
                break;
            }

            std::getline(posStream, startfen);

            readLock.unlock();

            startfen.insert(startfen.size() - 1, "0 1");

            Board board = Board(startfen);
            searchers[0].addBoardToHistory(board);
            searchers[1].addBoardToHistory(board);

            numMoves = 0;

            // If there are too many moves, the game will be adjudicated to a draw
            while (!m_isFinished(board, searchers[board.getTurn()], result) && (numMoves < DataEncoder::MaxGameLength))
            {
                SearchResult searchResult;
                Move move = searchers[board.getTurn()].search(board, searchParams, &searchResult);

                // The scores are from the current perspective
                scores[numMoves] = searchResult.eval;
                moves[numMoves] = move;
                numMoves++;

                board.performMove(move);
                searchers[0].addBoardToHistory(board);
                searchers[1].addBoardToHistory(board);

                if(Evaluator::isMateScore(searchResult.eval))
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

            encoder.addGame(startfen, moves, scores, numMoves, result);

            fenCount += numMoves + 1; // Num moves + startfen
            if((fenCount % 1000) < ((fenCount - numMoves - 1) % 1000)  )
            {
                LOG(fenCount << " fens\t" << 1000000.0f / msTimer.getMs() << " fens/sec\t" << 100 * fenCount / numFens << "%")
                msTimer.start();
            }
            writeLock.unlock();

            searchers[0].clearHistory();
            searchers[1].clearHistory();
            searchers[0].clear();
            searchers[1].clear();
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

    encoder.close();
    LOG("Finished generating FENs")
}