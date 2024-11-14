#include <test.hpp>
#include <utils.hpp>
#include <search.hpp>
#include <fen.hpp>
#include <perft.hpp>
#include <cstdint>
#include <chrono>

using namespace Arcanum;

static uint64_t s_perftCaptures(std::string fen, uint64_t expected);
static uint64_t s_perftPosition(std::string fen, uint8_t ply, uint64_t expected);

static uint64_t s_perftPosition(std::string fen, uint8_t ply, uint64_t expected)
{
    uint64_t count = 0;
    Board board = Board(fen);

    Test::findNumMovesAtDepth(board, ply, &count);

    if(count != expected)
    {
        ERROR("Failed perft with " << fen << " at " << unsigned(ply) << " depth. Expected: " << expected << " Got: " << count)
    }
    else
    {
        SUCCESS("Success perft with " << fen << " at " << unsigned(ply) << " depth")
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
        ERROR("Failed capture moves test with " << fen << " Expected: " << expected << " Got: " << count)
    }
    else
    {
        SUCCESS("Success capture moves test with " << fen)
    }

    return count;
}

// -- Test functions

void Test::perft()
{
    LOG("Running all perft")
    uint64_t sum = 0;
    auto start = std::chrono::high_resolution_clock::now();
    sum += s_perftPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 3195901860LL);
    sum += s_perftPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 6, 8031647685LL);
    sum += s_perftPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, 178633661LL);
    sum += s_perftPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 706045033LL);
    sum += s_perftPosition("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 6, 706045033LL);
    sum += s_perftPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, 89941194LL);
    sum += s_perftPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 6, 6923051137LL);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = end - start;
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);
    float deltaTime = micros.count() / 1000000.0f;

    LOG("Running all perft completed in " << micros.count() / 1000 << "ms. " << (sum / deltaTime) << " Nodes / Sec")
}

void Test::captureMoves()
{
    LOG("Running all capture moves")
    s_perftCaptures("k7/8/1r1b1n2/8/q2Q2p1/2P5/1q1p1p2/7K w - - 0 1", 7);
    s_perftCaptures("k7/8/1r1b1n2/5K2/q2Q2p1/2P5/1q1p1p2/8 w - - 0 1", 8);
    s_perftCaptures("k7/8/3q1p2/2r3p1/4N3/2r3P1/3K1P2/8 w - - 0 1", 3);
    s_perftCaptures("k7/8/3q1p2/2r3p1/4N3/2r3P1/3P1P2/7K w - - 0 1", 6);
    s_perftCaptures("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", 0);
    s_perftCaptures("k7/4b3/8/8/3QR1n1/8/4p3/K7 w - - 0 1", 3);
    s_perftCaptures("7k/1q6/8/5n2/4B3/8/2R5/Kb5p w - - 0 1", 3);
    s_perftCaptures("k7/8/8/2bpb3/3K4/4b3/8/8 w - - 0 1", 3);
    LOG("Completed all capture moves")
}

void Test::zobrist()
{
    LOG("Testing Zobrist")

    // Move Rook
    Board board1 = Board("k5r1/8/8/8/8/8/8/1R5K w - - 0 1");
    board1.performMove(Move(1, 2, MoveInfoBit::ROOK_MOVE));
    Board board2 = Board("k5r1/8/8/8/8/8/8/2R4K b - - 0 1");
    if(board1.getHash() != board2.getHash())
        ERROR("ROOK: Zobrist did not match")
    else if(board1.getPawnHash() != board2.getPawnHash())
        ERROR("ROOK: Pawn Zobrist did not match")
    else if(board1.getMaterialHash() != board2.getMaterialHash())
        ERROR("ROOK: Material Zobrist did not match")
    else
        SUCCESS("ROOK: Zobrist matched")

    // Capture Rook
    board1 = Board("k7/8/8/8/8/8/8/1Rr4K w - - 0 1");
    board1.performMove(Move(1, 2, MoveInfoBit::ROOK_MOVE | MoveInfoBit::CAPTURE_ROOK));
    board2 = Board("k7/8/8/8/8/8/8/2R4K b - - 0 1");
    if(board1.getHash() != board2.getHash())
        ERROR("Capture rook: Zobrist did not match")
    else if(board1.getPawnHash() != board2.getPawnHash())
        ERROR("Capture rook: Pawn Zobrist did not match" << board1.getPawnHash() << "  " << board2.getPawnHash())
    else if(board1.getMaterialHash() != board2.getMaterialHash())
        ERROR("Capture rook: Material Zobrist did not match")
    else
        SUCCESS("Capture rook: Zobrist matched")

    // Move back and forth
    board1 = Board("r2qkbnr/pppppppp/8/8/8/8/PPPPPPPP/R2QK2R w - - 0 1");
    board2 = Board("r2qkbnr/pppppppp/8/8/8/8/PPPPPPPP/R2QK2R w - - 0 1");
    board1.performMove(Move(3, 2, MoveInfoBit::QUEEN_MOVE));
    board1.performMove(Move(59, 58, MoveInfoBit::QUEEN_MOVE));
    board1.performMove(Move(2, 3, MoveInfoBit::QUEEN_MOVE));
    board1.performMove(Move(58, 59, MoveInfoBit::QUEEN_MOVE));
    if(board1.getHash() != board2.getHash())
        ERROR("Repeat: Zobrist did not match")
    else if(board1.getPawnHash() != board2.getPawnHash())
        ERROR("Repeat: Pawn Zobrist did not match")
    else if(board1.getMaterialHash() != board2.getMaterialHash())
        ERROR("Repeat: Material Zobrist did not match")
    else
        SUCCESS("Repeat: Zobrist matched")

    // Recreate board
    board1 = Board("r2qkbnr/pppppppp/8/8/8/8/PPPPPPPP/R2QK2R w - - 0 1");
    board2 = Board(board1);
    if(board1.getHash() != board2.getHash())
        ERROR("Recreate: Zobrist did not match")
    else if(board1.getPawnHash() != board2.getPawnHash())
        ERROR("Recreate: Pawn Zobrist did not match")
    else if(board1.getMaterialHash() != board2.getMaterialHash())
        ERROR("Recreate: Material Zobrist did not match")
    else
        SUCCESS("Recreate: Zobrist matched")

    // Capture pawn
    board1 = Board("rnbqkbnr/pp3ppp/8/2pP4/P7/8/1P1PPPPP/R1BQKBNR b - - 0 1");
    board1.performMove(Move(59, 35, MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::QUEEN_MOVE));
    board2 = Board("rnb1kbnr/pp3ppp/8/2pq4/P7/8/1P1PPPPP/R1BQKBNR w - - 0 1");
    if(board1.getHash() != board2.getHash())
        ERROR("Capture pawn: Zobrist did not match")
    else if(board1.getPawnHash() != board2.getPawnHash())
        ERROR("Capture pawn: Pawn Zobrist did not match")
    else if(board1.getMaterialHash() != board2.getMaterialHash())
        ERROR("Capture pawn: Material Zobrist did not match")
    else
        SUCCESS("Capture pawn: Zobrist matched")

    // Enpassant
    board1 = Board("rnbqkbnr/1pp1pppp/8/p2pP3/8/8/PPPP1PPP/RNBQKBNR w - d6 0 1");
    board1.performMove(Move(36, 43, MoveInfoBit::PAWN_MOVE | MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT));
    board2 = Board("rnbqkbnr/1pp1pppp/3P4/p7/8/8/PPPP1PPP/RNBQKBNR b - - 0 1");
    if(board1.getHash() != board2.getHash())
        ERROR("Enpassant: Zobrist did not match")
    else if(board1.getPawnHash() != board2.getPawnHash())
        ERROR("Enpassant: Pawn Zobrist did not match")
    else if(board1.getMaterialHash() != board2.getMaterialHash())
        ERROR("Enpassant: Material Zobrist did not match")
    else
        SUCCESS("Enpassant: Zobrist matched")

    LOG("Completed all Zobrist tests")
}

void Test::draw()
{
    LOG("Starting draw test")
    Searcher wsearcher = Searcher();

    // Test if search will find its way around 3-fold repetition checkmate
    Board repeat = Board("k7/1p1p1p2/pPpPpPp1/P1P1P1P1/7R/8/8/K7 b - - 0 1");
    wsearcher.addBoardToHistory(repeat);
    wsearcher.addBoardToHistory(repeat);
    Board board = Board("k7/1p1p1p2/pPpPpPp1/P1P1P1P1/R7/8/8/K7 w - - 0 1");
    wsearcher.addBoardToHistory(board);

    board.performMove(wsearcher.getBestMoveInTime(board, 200));
    wsearcher.addBoardToHistory(board);
    if(board.getHash() == repeat.getHash())
    {
        ERROR("Repeated position: k7/1p1p1p2/pPpPpPp1/P1P1P1P1/7R/8/8/K7 b - - 0 1")
    }
    board.performMove(Move(56, 57, MoveInfoBit::KING_MOVE));
    wsearcher.addBoardToHistory(board);
    board.performMove(wsearcher.getBestMoveInTime(board, 200));
    wsearcher.addBoardToHistory(board);
    board.performMove(Move(57, 56, MoveInfoBit::KING_MOVE));
    wsearcher.addBoardToHistory(board);
    board.performMove(wsearcher.getBestMoveInTime(board, 200));
    wsearcher.addBoardToHistory(board);
    if(board.getHash() == repeat.getHash())
    {
        ERROR("Repeated position: k7/1p1p1p2/pPpPpPp1/P1P1P1P1/7R/8/8/K7 b - - 0 1")
    }
    board.getLegalMoves();
    if(board.getNumLegalMoves() == 0 && board.isChecked())
    {
        SUCCESS("Found checkmate to avoid stalemate from  k7/1p1p1p2/pPpPpPp1/P1P1P1P1/7R/8/8/K7 b - - 0 1")
    }
    else
    {
        ERROR("Did not find checkmate to avoid stalemate from k7/1p1p1p2/pPpPpPp1/P1P1P1P1/7R/8/8/K7 b - - 0 1")
    }
}
