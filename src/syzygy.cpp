#include <syzygy.hpp>

using namespace Arcanum;

bool matchesPyrrhicMove(Move move, unsigned pyrrhicMove) {

    static uint32_t arcanumPromotions[5] = {
        0,
        Arcanum::PROMOTE_QUEEN,
        Arcanum::PROMOTE_ROOK,
        Arcanum::PROMOTE_BISHOP,
        Arcanum::PROMOTE_KNIGHT,
    };

    unsigned to    = TB_GET_TO(pyrrhicMove);
    unsigned from  = TB_GET_FROM(pyrrhicMove);
    unsigned promo = TB_GET_PROMOTES(pyrrhicMove);
    return (move.from == from) && (move.to == to) && (PROMOTED_PIECE(move.moveInfo) == arcanumPromotions[promo]);
}

bool Arcanum::TBProbeDTZ(Board& board, Move* moves, uint8_t& numMoves, uint8_t& wdl)
{
    if(board.getNumPiecesLeft() > TB_LARGEST || board.getCastleRights())
    {
        return false;
    }

    unsigned results[218];

    Move* legalMoves = board.getLegalMoves();
    uint8_t numLegalMoves = board.getNumLegalMoves();
    board.generateCaptureInfo();

    unsigned result = tb_probe_root(
        board.getColoredPieces(WHITE),
        board.getColoredPieces(BLACK),
        board.getTypedPieces(W_KING,    WHITE) | board.getTypedPieces(W_KING,   BLACK),
        board.getTypedPieces(W_QUEEN,   WHITE) | board.getTypedPieces(W_QUEEN,  BLACK),
        board.getTypedPieces(W_ROOK,    WHITE) | board.getTypedPieces(W_ROOK,   BLACK),
        board.getTypedPieces(W_BISHOP,  WHITE) | board.getTypedPieces(W_BISHOP, BLACK),
        board.getTypedPieces(W_KNIGHT,  WHITE) | board.getTypedPieces(W_KNIGHT, BLACK),
        board.getTypedPieces(W_PAWN,    WHITE) | board.getTypedPieces(W_PAWN,   BLACK),
        board.getHalfMoves(),
        board.getEnpassantSquare() == 64 ? 0 : board.getEnpassantSquare(),
        board.getTurn()^1, results
    );

    // Probe failed, or we are already in a finished position.
    if(result == TB_RESULT_FAILED)
        return false;


    // Find a move with the
    numMoves = 0;
    for (int i = 0; i < 218 && results[i] != TB_RESULT_FAILED; i++)
    {
        if (TB_GET_WDL(results[i]) == TB_GET_WDL(result))
        {
            for(uint8_t j = 0; j < numLegalMoves; j++)
            {
                if(matchesPyrrhicMove(legalMoves[j], results[i]))
                {
                    moves[numMoves++] = legalMoves[j];
                    break;
                }
            }
        }
    }

    wdl = TB_GET_WDL(result);

    return numMoves > 0;
}

uint32_t Arcanum::TBProbeWDL(const Board &board)
{
    return tb_probe_wdl(
        board.getColoredPieces(WHITE),
        board.getColoredPieces(BLACK),
        board.getTypedPieces(W_KING,    WHITE) | board.getTypedPieces(W_KING,   BLACK),
        board.getTypedPieces(W_QUEEN,   WHITE) | board.getTypedPieces(W_QUEEN,  BLACK),
        board.getTypedPieces(W_ROOK,    WHITE) | board.getTypedPieces(W_ROOK,   BLACK),
        board.getTypedPieces(W_BISHOP,  WHITE) | board.getTypedPieces(W_BISHOP, BLACK),
        board.getTypedPieces(W_KNIGHT,  WHITE) | board.getTypedPieces(W_KNIGHT, BLACK),
        board.getTypedPieces(W_PAWN,    WHITE) | board.getTypedPieces(W_PAWN,   BLACK),
        board.getEnpassantSquare() == 64 ? 0 : board.getEnpassantSquare(),
        board.getTurn()^1
    );
}

bool Arcanum::TBInit(std::string path)
{
    bool initialized = tb_init(path.c_str());

    if(!initialized)
    {
        WARNING("Failed to initialize syzygy: " << path)
    }
    else
    {
        LOG("Initialized syzygy: " << path)
        LOG("Syzygy largest piece set: " << TB_LARGEST)
    }

    return initialized;
}

void Arcanum::TBFree()
{
    tb_free();
}