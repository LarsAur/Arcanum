#include <board.hpp>
#include <utils.hpp>
#include <cstdint>
#include <chrono>

namespace ChessEngine2
{
    void findNumMovesAtDepth(int depth, ChessEngine2::Board *board, uint64_t *count, bool top = true)
    {
        Move* legalMoves = board->getLegalMoves();
        uint8_t numLegalMoves = board->getNumLegalMoves();

        if(numLegalMoves == 0)
        {
            *count += 0;
            return;
        }
        
        if(depth == 1)
        {
            *count += numLegalMoves;
            return;
        }

        for(int i = 0; i < numLegalMoves; i++)
        {
            Board newBoard = Board(*board);
            newBoard.performMove(legalMoves[i]);

            if(top)
            {
                uint64_t _count = 0;
                findNumMovesAtDepth(depth - 1, &newBoard, &_count, false);
                // std::cout << ChessEngine2::getArithmeticNotation(it->from) << ChessEngine2::getArithmeticNotation(it->to) << ": " << _count << std::endl;
                *count += _count;
            }
            else
            {
                findNumMovesAtDepth(depth - 1, &newBoard, count, false);
            }
        }
    }

    uint64_t runPerft(std::string fen, uint8_t ply, uint64_t expected)
    {
        uint64_t count = 0;
        Board board = Board(fen);
        findNumMovesAtDepth(ply, &board, &count);
        
        if(count != expected)
        {
            CHESS_ENGINE2_ERR("Failed perft with " << fen << " at " << +ply << " depth. Expected: " << expected << " Got: " << count)
        }
        else
        {
            CHESS_ENGINE2_SUCCESS("Success perft with " << fen << " at " << +ply << " depth")
        }

        return count;
    }

    void runAllPerft()
    {
        CHESS_ENGINE2_LOG("Running all perft")
        uint64_t sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        sum += runPerft("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 3195901860LL);
        sum += runPerft("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 6, 8031647685LL);
        sum += runPerft("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, 178633661LL);
        sum += runPerft("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 706045033LL);
        sum += runPerft("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1 ", 6, 706045033LL);
        sum += runPerft("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, 89941194LL);
        sum += runPerft("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 6, 6923051137LL);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);
        float deltaTime = micros.count() / 1000000.0f;

        CHESS_ENGINE2_LOG("Running all perft completed in " << micros.count() / 1000 << "ms. " << (sum / deltaTime) << " Nodes / Sec")
    }

}