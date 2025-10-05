#include <tests/test.hpp>
#include <zobrist.hpp>

using namespace Arcanum;

static void playAllMovesAndCheckZobrist(Board& board, uint32_t depth, bool* failed)
{
    Move* legalMoves = board.getLegalMoves();
    uint8_t numLegalMoves = board.getNumLegalMoves();

    if((numLegalMoves == 0) || (depth == 0))
    {
        return;
    }

    board.generateCaptureInfo();
    for(int i = 0; i < numLegalMoves; i++)
    {
        Board newBoard = Board(board);
        newBoard.performMove(legalMoves[i]);

        hash_t hash, pawnHash, materialHash;
        Zobrist::zobrist.getHashs(newBoard, hash, pawnHash, materialHash);

        if(hash != newBoard.getHash())
        {
            FAIL("Zobrist did not change after move: " << legalMoves[i] << " From board: " << board.fen() << " To board: " << newBoard.fen())
            *failed |= true;
        }

        if(pawnHash != newBoard.getPawnHash())
        {
            FAIL("Pawn Zobrist did not change after move: " << legalMoves[i] << " From board: " << board.fen() << " To board: " << newBoard.fen())
            *failed |= true;
        }

        if(materialHash != newBoard.getMaterialHash())
        {
            FAIL("Material Zobrist did not change after move: " << legalMoves[i] << " From board: " << board.fen() << " To board: " << newBoard.fen())
            *failed |= true;
        }

        if(!*failed)
        {
            playAllMovesAndCheckZobrist(newBoard, depth - 1, failed);
        }
        else
        {
            return; // Exit early on failure
        }
    }
}

bool Test::runZobristTest()
{
    bool failed = false;

    /////////////////// Test Hash Differences ///////////////////

    // Verify that the hash of a board with and without castling rights differ
    Board boardWithCastling = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Board boardWithoutCastling = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1");
    if(boardWithCastling.getHash() == boardWithoutCastling.getHash())
    {
        FAIL("Boards with and without castling rights have the same hash")
        return false;
    }
    else
    {
        SUCCESS("Boards with and without castling rights have different hashes")
    }

    // Verify that the hash of a board with and without enpassant differ
    Board boardWithEnpassant = Board("rnbqkbnr/pppp1ppp/8/8/4PpP1/8/PPPP3P/RNBQKBNR b KQkq g3 0 3");
    Board boardWithoutEnpassant = Board("rnbqkbnr/pppp1ppp/8/8/4PpP1/8/PPPP3P/RNBQKBNR b KQkq - 0 3");
    if(boardWithEnpassant.getHash() == boardWithoutEnpassant.getHash())
    {
        FAIL("Boards with and without enpassant have the same hash")
        return false;
    }
    else
    {
        SUCCESS("Boards with and without enpassant have different hashes")
    }

    // Verify that the hash of a board with different turns differ
    Board boardTurnWhite = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Board boardTurnBlack = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    if(boardTurnWhite.getHash() == boardTurnBlack.getHash())
    {
        FAIL("Boards with different turns have the same hash")
        return false;
    }
    else
    {
        SUCCESS("Boards with different turns have different hashes")
    }

    /////////////////// Recursive check of zobrist update vs zobrist initialization /////////////////

    // Test initial position
    Board boardStart = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    playAllMovesAndCheckZobrist(boardStart, 6, &failed);
    if(failed)
    {
        FAIL("Failed initial position")
        return false;
    }
    else
    {
        SUCCESS("Completed initial position")
    }

    // Test initial position without castling rights
    Board boardNoCastling = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1");
    playAllMovesAndCheckZobrist(boardNoCastling, 5, &failed);
    if(failed)
    {
        FAIL("Failed initial position without castling rights")
        return false;
    }
    else
    {
        SUCCESS("Completed initial position without castling rights")
    }

    // Test position with enpassant and black to move
    Board boardEnpassant = Board("rnbqkbnr/pppp1ppp/8/8/4PpP1/8/PPPP3P/RNBQKBNR b KQkq g3 0 3");
    playAllMovesAndCheckZobrist(boardEnpassant, 5, &failed);
    if(failed)
    {
        FAIL("Failed position with enpassant")
        return false;
    }
    else
    {
        SUCCESS("Completed position with enpassant")
    }

    // Test position with promotions for both colors
    Board boardPromote = Board("rnbqkbnr/ppppp2P/8/8/8/2P5/PP1pK1PP/RNBQ1BNR b kq - 1 8");
    playAllMovesAndCheckZobrist(boardPromote, 5, &failed);
    if(failed)
    {
        FAIL("Failed position with promotions")
        return false;
    }
    else
    {
        SUCCESS("Completed position with promotions")
    }

    return true;
}