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
        Zobrist::getHashes(newBoard, hash, pawnHash, materialHash);

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

    // Verify that the hash of a board with/without enpassant has different hash only when enpassant is legal

    // Enpassant is legal
    Board boardEp1 = Board("rnbqkbnr/1pp1pppp/p7/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    Board boardEp2 = Board("rnbqkbnr/1pp1pppp/p7/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3");
    if(boardEp1.getHash() == boardEp2.getHash())
    {
        FAIL("Boards with and without enpassant have the same hash even though enpassant is legal")
        return false;
    }
    else
    {
        SUCCESS("Boards with and without enpassant have different hashes when enpassant is legal")
    }

    // Enpassant is not legal because there are no attackers
    boardEp1 = Board("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    boardEp2 = Board("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    if(boardEp1.getHash() != boardEp2.getHash())
    {
        FAIL("Boards with and without enpassant have different hash when enpassant is not legal due to no attackers")
        return false;
    }
    else
    {
        SUCCESS("Boards with and without enpassant have the same hash when enpassant is not legal due to no attackers")
    }

    // Enpassant is not legal because it would cause a discovered check
    boardEp1 = Board("1nbqkbnr/1pp1pppp/8/r1PpK3/8/p4P2/PP1PP1PP/RNBQ1BNR w k d6 0 8");
    boardEp2 = Board("1nbqkbnr/1pp1pppp/8/r1PpK3/8/p4P2/PP1PP1PP/RNBQ1BNR w k - 0 8");
    if(boardEp1.getHash() != boardEp2.getHash())
    {
        FAIL("Boards with and without enpassant have different hash when enpassant is not legal due to self checking")
        return false;
    }
    else
    {
        SUCCESS("Boards with and without enpassant have the same hash when enpassant is not legal due to self checking")
    }

    // Enpassant is not allowed because it does not stop the king from being in check
    boardEp1 = Board("rnbqkbnr/2p1pppp/8/1p1pPK2/8/p7/PPPP1PPP/RNBQ1BNR w kq d6 0 8");
    boardEp2 = Board("rnbqkbnr/2p1pppp/8/1p1pPK2/8/p7/PPPP1PPP/RNBQ1BNR w kq - 0 8");
    if(boardEp1.getHash() != boardEp2.getHash())
    {
        FAIL("Boards with and without enpassant have different hash when enpassant is not legal due to not stopping check")
        return false;
    }
    else
    {
        SUCCESS("Boards with and without enpassant have the same hash when enpassant is not legal due to not stopping check")
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