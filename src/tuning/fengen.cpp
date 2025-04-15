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

bool Fengen::m_isFinished(Board& board, Searcher& searcher, GameResult& result) const
{
    auto history = searcher.getHistory();
    if(history.at(board.getHash()) > 2)
    {
        result = DRAW;
        return true;
    }

    if(board.isMaterialDraw())
    {
        result = DRAW;
        return true;
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

bool Fengen::m_isAdjudicated(Board& board, uint32_t ply, std::array<eval_t, DataEncoder::MaxGameLength>& scores, GameResult& result) const
{
    constexpr uint32_t DrawPly   = 40;
    constexpr uint32_t DrawScore = 10;
    constexpr uint32_t DrawScoreRepeats = 8; // 4 times from each side

    // Adjudicate the game as a draw if it is too long
    if(ply >= DataEncoder::MaxGameLength - 1)
    {
        result = GameResult::DRAW;
        return true;
    }

    // Adjudicate the game if the score is close to zero for long enough
    // This will also catch draws from tablebase
    bool isDraw = false;
    if(DrawPly > 40)
    {
        isDraw = true;
        for(uint32_t i = ply - DrawScoreRepeats - 1; i < ply; i++)
        {
            if(std::abs(scores[i]) > DrawScore)
            {
                isDraw = false;
                break;
            }
        }
    }

    if(isDraw)
    {
        result = GameResult::DRAW;
    }

    return isDraw;
}

void Fengen::start(FengenParameters params)
{
    std::vector<std::thread> threads;
    std::mutex readLock;
    std::mutex writeLock;
    size_t fenCount = 0LL;
    Timer msTimer = Timer();

    // Set search parameters
    SearchParameters searchParams = SearchParameters();

    searchParams.useTime    = params.movetime > 0;
    searchParams.msTime     = params.movetime;
    searchParams.useDepth   = params.depth > 0;
    searchParams.depth      = params.depth;
    searchParams.useNodes   = params.nodes > 0;
    searchParams.nodes      = params.nodes;

    msTimer.start();

    std::ifstream posStream = std::ifstream(params.startposPath);

    if(!posStream.is_open())
    {
        ERROR("Unable to open " << params.startposPath)
        return;
    }

    DataStorer encoder = DataStorer();
    if(!encoder.open(params.outputPath))
    {
        ERROR("Unable to open " << params.outputPath)
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

        readLock.lock();
        Searcher searchers[2] = {Searcher(false), Searcher(false)};
        readLock.unlock();

        searchers[0].resizeTT(8);
        searchers[1].resizeTT(8);

        while (true)
        {
            readLock.lock();

            posStream.peek();
            if(posStream.eof() || fenCount > params.numFens)
            {
                readLock.unlock();
                break;
            }

            std::getline(posStream, startfen);

            readLock.unlock();

            // Parse the board in relaxed mode and get the fen from the board
            // this is in case the edp does not provide move-clocks
            Board board = Board(startfen, false);
            startfen = board.fen();

            searchers[0].addBoardToHistory(board);
            searchers[1].addBoardToHistory(board);

            numMoves = 0;

            // If there are too many moves, the game will be adjudicated to a draw
            while (!m_isFinished(board, searchers[board.getTurn()], result) && !m_isAdjudicated(board, numMoves, scores, result))
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

                // If a real mate is found, we can terminate the search.
                // The positions in the mating line will not be added to the games
                if(Evaluator::isRealMateScore(searchResult.eval))
                {
                    result = searchResult.eval > 0 ? (board.getTurn() == WHITE ? BLACK_WIN : WHITE_WIN) : (board.getTurn() == WHITE ? WHITE_WIN : BLACK_WIN);
                    break;
                }
            }

            writeLock.lock();

            encoder.addGame(startfen, moves, scores, numMoves, result);

            fenCount += numMoves + 1; // Num moves + startfen
            if((fenCount % 1000) < ((fenCount - numMoves - 1) % 1000)  )
            {
                LOG(fenCount << " fens\t" << 1000000.0f / msTimer.getMs() << " fens/sec\t" << 100 * fenCount / params.numFens << "%")
                msTimer.start();
            }
            writeLock.unlock();

            searchers[0].clearHistory();
            searchers[1].clearHistory();
            searchers[0].clear();
            searchers[1].clear();
        }
    };

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.push_back(std::thread(fn, i));
    }

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.at(i).join();
    }

    encoder.close();
    LOG("Finished generating FENs")
}