#include <board.hpp>
#include <utils.hpp>
#include <search.hpp>
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
            // *count += 0;
            return;
        }
        
        if(depth == 1)
        {
            *count += numLegalMoves;
            return;
        }

        board->generateCaptureInfo();
        for(int i = 0; i < numLegalMoves; i++)
        {
            Board newBoard = Board(*board);
            newBoard.performMove(legalMoves[i]);

            if(top)
            {
                uint64_t _count = 0;
                findNumMovesAtDepth(depth - 1, &newBoard, &_count, false);
                CE2_LOG(ChessEngine2::getArithmeticNotation(legalMoves[i].from) << ChessEngine2::getArithmeticNotation(legalMoves[i].to) << " " << unsigned(legalMoves[i].moveInfo) << ": " << _count);
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
        CE2_LOG(std::endl << board.getBoardString())

        findNumMovesAtDepth(ply, &board, &count);
        
        if(count != expected)
        {
            CE2_ERROR("Failed perft with " << fen << " at " << unsigned(ply) << " depth. Expected: " << expected << " Got: " << count)
        }
        else
        {
            CE2_SUCCESS("Success perft with " << fen << " at " << unsigned(ply) << " depth")
        }

        return count;
    }

    void runAllPerft()
    {
        CE2_LOG("Running all perft")
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

        CE2_LOG("Running all perft completed in " << micros.count() / 1000 << "ms. " << (sum / deltaTime) << " Nodes / Sec")
    }

    uint64_t runCaptureMoves(std::string fen, uint64_t expected)
    {
        Board board = Board(fen);
        board.getLegalCaptureAndCheckMoves();
        uint64_t count = board.getNumLegalMoves();

        if(count != expected)
        {
            CE2_ERROR("Failed capture moves test with " << fen << " Expected: " << expected << " Got: " << count)
        }
        else
        {
            CE2_SUCCESS("Success capture moves test with " << fen)
        }

        return count;
    }

    void runAllCaptureMoves()
    {
        CE2_LOG("Running all capture moves")
        runCaptureMoves("k7/8/1r1b1n2/8/q2Q2p1/2P5/1q1p1p2/7K w - - 0 1", 7);
        runCaptureMoves("k7/8/1r1b1n2/5K2/q2Q2p1/2P5/1q1p1p2/8 w - - 0 1", 8);
        runCaptureMoves("8/8/3q1p2/2r3p1/4N3/2r3P1/3K1P2/8 w - - 0 1", 3);
        runCaptureMoves("8/8/3q1p2/2r3p1/4N3/2r3P1/3P1P2/7K w - - 0 1", 6);
        runCaptureMoves("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", 0);
        runCaptureMoves("k7/4b3/8/8/3QR1n1/8/4p3/K7 w - - 0 1", 3);
        runCaptureMoves("7k/1q6/8/5n2/4B3/8/2R5/Kb5p w - - 0 1", 3);
        runCaptureMoves("8/8/8/2bpb3/3K4/4b3/8/8 w - - 0 1", 3);
        CE2_LOG("Completed all capture moves")
    }

    void runAllZobristTests()
    {
        CE2_LOG("Testing Zobrist")

        // Move Rook
        Board board1 = Board("k5r1/8/8/8/8/8/8/1R5K w - - 0 1");
        board1.performMove(Move(1, 2, MOVE_INFO_ROOK_MOVE));
        Board board2 = Board("k5r1/8/8/8/8/8/8/2R4K b - - 0 1");
        if(board1.getHash() != board2.getHash())
            CE2_ERROR("ROOK: Zobrist did not match")
        else if(board1.getPawnHash() != board2.getPawnHash())
            CE2_ERROR("ROOK: Pawn Zobrist did not match")
        else if(board1.getMaterialHash() != board2.getMaterialHash())
            CE2_ERROR("ROOK: Material Zobrist did not match")
        else
            CE2_SUCCESS("ROOK: Zobrist matched")

        // Capture Rook
        board1 = Board("k7/8/8/8/8/8/8/1Rr4K w - - 0 1");
        board1.performMove(Move(1, 2, MOVE_INFO_ROOK_MOVE | MOVE_INFO_CAPTURE_ROOK));
        board2 = Board("k7/8/8/8/8/8/8/2R4K b - - 0 1");
        if(board1.getHash() != board2.getHash())
            CE2_ERROR("Capture rook: Zobrist did not match")
        else if(board1.getPawnHash() != board2.getPawnHash())
            CE2_ERROR("Capture rook: Pawn Zobrist did not match" << board1.getPawnHash() << "  " << board2.getPawnHash())
        else if(board1.getMaterialHash() != board2.getMaterialHash())
            CE2_ERROR("Capture rook: Material Zobrist did not match")
        else
            CE2_SUCCESS("Capture rook: Zobrist matched")

        // Move back and forth
        board1 = Board("r2qkbnr/pppppppp/8/8/8/8/PPPPPPPP/R2QK2R w - - 0 1");
        board2 = Board("r2qkbnr/pppppppp/8/8/8/8/PPPPPPPP/R2QK2R w - - 0 1");
        board1.performMove(Move(3, 2, MOVE_INFO_QUEEN_MOVE));
        board1.performMove(Move(59, 58, MOVE_INFO_QUEEN_MOVE));
        board1.performMove(Move(2, 3, MOVE_INFO_QUEEN_MOVE));
        board1.performMove(Move(58, 59, MOVE_INFO_QUEEN_MOVE));
        if(board1.getHash() != board2.getHash())
            CE2_ERROR("Repeat: Zobrist did not match")
        else if(board1.getPawnHash() != board2.getPawnHash())
            CE2_ERROR("Repeat: Pawn Zobrist did not match")
        else if(board1.getMaterialHash() != board2.getMaterialHash())
            CE2_ERROR("Repeat: Material Zobrist did not match")
        else
            CE2_SUCCESS("Repeat: Zobrist matched")

        // Recreate board
        board1 = Board("r2qkbnr/pppppppp/8/8/8/8/PPPPPPPP/R2QK2R w - - 0 1");
        board2 = Board(board1);
        if(board1.getHash() != board2.getHash())
            CE2_ERROR("Recreate: Zobrist did not match")
        else if(board1.getPawnHash() != board2.getPawnHash())
            CE2_ERROR("Recreate: Pawn Zobrist did not match")
        else if(board1.getMaterialHash() != board2.getMaterialHash())
            CE2_ERROR("Recreate: Material Zobrist did not match")
        else
            CE2_SUCCESS("Recreate: Zobrist matched")

        CE2_LOG("Completed all Zobrist tests")
    }

    void runSearchPerformanceTest()
    {
        CE2_LOG("Starting search performance test")

        Searcher whiteSearcher = Searcher();
        Searcher blackSearcher = Searcher();
        Board board = Board(ChessEngine2::startFEN);
        board.addBoardToHistory();

        auto start = std::chrono::high_resolution_clock::now();
        // Search for 10 moves
        for(int i = 0; i < 10; i++)
        {
            CE2_DEBUG("PERF: " << i << "/" << 10)
            Move whiteMove = whiteSearcher.getBestMove(board, 6, 4);
            board.performMove(whiteMove);
            board.addBoardToHistory();
            Move blackMove = blackSearcher.getBestMove(board, 6, 4);
            board.performMove(blackMove);
            board.addBoardToHistory();
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);

        CE2_LOG("Completed search performance in " << micros.count() / 1000 << "ms")
    }
}