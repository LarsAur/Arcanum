#include <test.hpp>
#include <utils.hpp>
#include <search.hpp>
#include <cstdint>
#include <chrono>

using namespace ChessEngine2;

static void s_findNumMovesAtDepth(int depth, ChessEngine2::Board *board, uint64_t *count, bool top = true);
static bool s_engineTest(uint32_t ms, std::string fen, Move bestMove, std::string id = "");
static uint64_t s_perftCaptures(std::string fen, uint64_t expected);
static uint64_t s_perftPosition(std::string fen, uint8_t ply, uint64_t expected);

static void s_findNumMovesAtDepth(int depth, ChessEngine2::Board *board, uint64_t *count, bool top)
{
    Move* legalMoves = board->getLegalMoves();
    uint8_t numLegalMoves = board->getNumLegalMoves();

    if(numLegalMoves == 0)
    {
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
            s_findNumMovesAtDepth(depth - 1, &newBoard, &_count, false);
            CE2_DEBUG(legalMoves[i] << ": " << _count);
            *count += _count;
        }
        else
        {
            s_findNumMovesAtDepth(depth - 1, &newBoard, count, false);
        }
    }
}

static bool s_engineTest(uint32_t ms, std::string fen, Move bestMove, std::string id)
{
    Searcher searcher;
    Board board(fen);
    Move foundMove = searcher.getBestMoveInTime(board, ms, 4);

    if(foundMove == bestMove)
    {
        CE2_SUCCESS("Success engine test with ("<< id << ") " << fen << " found best move " << foundMove)
        return true;
    }

    CE2_ERROR("Failed engine test with ("<< id << ") " << fen << " found best move " << foundMove << " not " << bestMove)
    return false;
}

static uint64_t s_perftPosition(std::string fen, uint8_t ply, uint64_t expected)
{
    uint64_t count = 0;
    Board board = Board(fen);
    CE2_DEBUG(std::endl << board.getBoardString())

    s_findNumMovesAtDepth(ply, &board, &count);
    
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

uint64_t s_perftCaptures(std::string fen, uint64_t expected)
{
    Board board = Board(fen);
    board.getLegalCaptureMoves();
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

// -- Test functions

void Test::perft()
{
    CE2_LOG("Running all perft")
    uint64_t sum = 0;
    auto start = std::chrono::high_resolution_clock::now();
    sum += s_perftPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 3195901860LL);
    sum += s_perftPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 6, 8031647685LL);
    sum += s_perftPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, 178633661LL);
    sum += s_perftPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 706045033LL);
    sum += s_perftPosition("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1 ", 6, 706045033LL);
    sum += s_perftPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, 89941194LL);
    sum += s_perftPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 6, 6923051137LL);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = end - start;
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);
    float deltaTime = micros.count() / 1000000.0f;

    CE2_LOG("Running all perft completed in " << micros.count() / 1000 << "ms. " << (sum / deltaTime) << " Nodes / Sec")
}

void Test::captureMoves()
{
    CE2_LOG("Running all capture moves")
    s_perftCaptures("k7/8/1r1b1n2/8/q2Q2p1/2P5/1q1p1p2/7K w - - 0 1", 7);
    s_perftCaptures("k7/8/1r1b1n2/5K2/q2Q2p1/2P5/1q1p1p2/8 w - - 0 1", 8);
    s_perftCaptures("8/8/3q1p2/2r3p1/4N3/2r3P1/3K1P2/8 w - - 0 1", 3);
    s_perftCaptures("8/8/3q1p2/2r3p1/4N3/2r3P1/3P1P2/7K w - - 0 1", 6);
    s_perftCaptures("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", 0);
    s_perftCaptures("k7/4b3/8/8/3QR1n1/8/4p3/K7 w - - 0 1", 3);
    s_perftCaptures("7k/1q6/8/5n2/4B3/8/2R5/Kb5p w - - 0 1", 3);
    s_perftCaptures("8/8/8/2bpb3/3K4/4b3/8/8 w - - 0 1", 3);
    CE2_LOG("Completed all capture moves")
}

void Test::zobrist()
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

    // Capture pawn
    board1 = Board("rnbqkbnr/pp3ppp/8/2pP4/P7/8/1P1PPPPP/R1BQKBNR b - - 0 1");
    board1.performMove(Move(59, 35, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_QUEEN_MOVE));
    board2 = Board("rnb1kbnr/pp3ppp/8/2pq4/P7/8/1P1PPPPP/R1BQKBNR w - - 0 1");
    if(board1.getHash() != board2.getHash())
        CE2_ERROR("Capture pawn: Zobrist did not match")
    else if(board1.getPawnHash() != board2.getPawnHash())
        CE2_ERROR("Capture pawn: Pawn Zobrist did not match")
    else if(board1.getMaterialHash() != board2.getMaterialHash())
        CE2_ERROR("Capture pawn: Material Zobrist did not match")
    else
        CE2_SUCCESS("Capture pawn: Zobrist matched")

    // Enpassant
    board1 = Board("rnbqkbnr/1pp1pppp/8/p2pP3/8/8/PPPP1PPP/RNBQKBNR w - d6 0 1");
    board1.performMove(Move(36, 43, MOVE_INFO_PAWN_MOVE | MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT));
    board2 = Board("rnbqkbnr/1pp1pppp/3P4/p7/8/8/PPPP1PPP/RNBQKBNR b - - 0 1");
    if(board1.getHash() != board2.getHash())
        CE2_ERROR("Enpassant: Zobrist did not match")
    else if(board1.getPawnHash() != board2.getPawnHash())
        CE2_ERROR("Enpassant: Pawn Zobrist did not match")
    else if(board1.getMaterialHash() != board2.getMaterialHash())
        CE2_ERROR("Enpassant: Material Zobrist did not match")
    else
        CE2_SUCCESS("Enpassant: Zobrist matched")

    CE2_LOG("Completed all Zobrist tests")
}

// -- Perf functions

void Perf::search()
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

// Bratko-Kopec Test
// From: https://www.chessprogramming.org/Bratko-Kopec_Test
void Perf::engineTest()
{
    const int total = 24;
    int correct = 0;
    correct += s_engineTest(5000, "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", Move(43, 3), "BK.01");
    correct += s_engineTest(5000, "3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 1", Move(27, 35), "BK.02");
    correct += s_engineTest(5000, "2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - 0 1", Move(45, 37), "BK.03");
    correct += s_engineTest(5000, "rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq - 0 1", Move(36, 44), "BK.04");
    correct += s_engineTest(5000, "r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 1", Move(8, 24), "BK.05");
    correct += s_engineTest(5000, "2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - - 0 1", Move(38, 46), "BK.06");
    correct += s_engineTest(5000, "1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - - 0 1", Move(39, 45), "BK.07");
    correct += s_engineTest(5000, "4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - - 0 1", Move(29, 37), "BK.08");
    correct += s_engineTest(5000, "2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - - 0 1", Move(29, 37), "BK.09");
    correct += s_engineTest(5000, "3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - - 0 1", Move(42, 36), "BK.10");
    correct += s_engineTest(5000, "2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - - 0 1", Move(13, 29), "BK.11");
    correct += s_engineTest(5000, "r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - - 0 1", Move(51, 37), "BK.12");
    correct += s_engineTest(5000, "r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - - 0 1", Move(9, 25), "BK.13");
    correct += s_engineTest(5000, "rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - - 0 1", Move(3, 4), "BK.14");
    correct += s_engineTest(5000, "2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - - 0 1", Move(30, 54), "BK.15");
    correct += s_engineTest(5000, "r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq - 0 1", Move(11, 28), "BK.16");
    correct += s_engineTest(5000, "r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - - 0 1", Move(55, 39), "BK.17");
    correct += s_engineTest(5000, "r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - - 0 1", Move(34, 17), "BK.18");
    correct += s_engineTest(5000, "3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - - 0 1", Move(60, 28), "BK.19");
    correct += s_engineTest(5000, "r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - - 0 1", Move(22, 30), "BK.20");
    correct += s_engineTest(5000, "3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - - 0 1", Move(37, 47), "BK.21");
    correct += s_engineTest(5000, "2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1", Move(49, 28), "BK.22");
    correct += s_engineTest(5000, "r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq - 0 1", Move(53, 45), "BK.23");
    correct += s_engineTest(5000, "r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - - 0 1", Move(13, 29), "BK.24");

    CE2_LOG("Score: " << correct << " / " << total);
}