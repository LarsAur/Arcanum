#include <syzygy.hpp>

using namespace Arcanum;

Syzygy::WDLResult Syzygy::TBProbeDTZ(Board& board, Move* moves, uint8_t& numMoves)
{
    constexpr static uint32_t PyrrhicToArcanumPromotion[5] = {
        0,
        Arcanum::PROMOTE_QUEEN,
        Arcanum::PROMOTE_ROOK,
        Arcanum::PROMOTE_BISHOP,
        Arcanum::PROMOTE_KNIGHT,
    };

    if(board.getNumPieces() > TB_LARGEST || board.getCastleRights())
    {
        return Syzygy::WDLResult::FAILED;
    }

    unsigned results[MaxMoveCount];

    unsigned result = tb_probe_root(
        board.getColoredPieces(Color::WHITE),
        board.getColoredPieces(Color::BLACK),
        board.getTypedPieces(Piece::KING,    Color::WHITE) | board.getTypedPieces(Piece::KING,   Color::BLACK),
        board.getTypedPieces(Piece::QUEEN,   Color::WHITE) | board.getTypedPieces(Piece::QUEEN,  Color::BLACK),
        board.getTypedPieces(Piece::ROOK,    Color::WHITE) | board.getTypedPieces(Piece::ROOK,   Color::BLACK),
        board.getTypedPieces(Piece::BISHOP,  Color::WHITE) | board.getTypedPieces(Piece::BISHOP, Color::BLACK),
        board.getTypedPieces(Piece::KNIGHT,  Color::WHITE) | board.getTypedPieces(Piece::KNIGHT, Color::BLACK),
        board.getTypedPieces(Piece::PAWN,    Color::WHITE) | board.getTypedPieces(Piece::PAWN,   Color::BLACK),
        board.getHalfMoves(),
        board.getEnpassantSquare() == 64 ? 0 : board.getEnpassantSquare(),
        board.getTurn()^1, results
    );

    // Probe failed, or we are already in a finished position.
    if(result == TB_RESULT_FAILED)
    {
        return Syzygy::WDLResult::FAILED;
    }

    // Find a move with the same WDL value as the root position
    numMoves = 0;
    for (int i = 0; i < MaxMoveCount && results[i] != TB_RESULT_FAILED; i++)
    {
        if (TB_GET_WDL(results[i]) == TB_GET_WDL(result))
        {
            square_t to    = TB_GET_TO(results[i]);
            square_t from  = TB_GET_FROM(results[i]);
            uint32_t promo = PyrrhicToArcanumPromotion[TB_GET_PROMOTES(results[i])];
            moves[numMoves++] = board.generateMoveWithInfo(from, to, promo);
        }
    }

    // Check if any maching moves are found
    // Assuming some moves could fail probing
    if(numMoves == 0)
    {
        return Syzygy::WDLResult::FAILED;
    }

    switch (TB_GET_WDL(result))
    {
    case TB_LOSS:
        return Syzygy::WDLResult::LOSS;
    case TB_WIN:
        return Syzygy::WDLResult::WIN;
    default:
        // This covers TB_DRAW, TB_BLESSED_LOSS and TB_CURSED_WIN
        return Syzygy::WDLResult::DRAW;
    }
}

Syzygy::WDLResult Syzygy::TBProbeWDL(const Board &board)
{
    if(board.getNumPieces() > TB_LARGEST || board.getCastleRights() || board.getHalfMoves() != 0)
    {
        return Syzygy::WDLResult::FAILED;
    }

    uint32_t result = tb_probe_wdl(
        board.getColoredPieces(Color::WHITE),
        board.getColoredPieces(Color::BLACK),
        board.getTypedPieces(Piece::KING,    Color::WHITE) | board.getTypedPieces(Piece::KING,   Color::BLACK),
        board.getTypedPieces(Piece::QUEEN,   Color::WHITE) | board.getTypedPieces(Piece::QUEEN,  Color::BLACK),
        board.getTypedPieces(Piece::ROOK,    Color::WHITE) | board.getTypedPieces(Piece::ROOK,   Color::BLACK),
        board.getTypedPieces(Piece::BISHOP,  Color::WHITE) | board.getTypedPieces(Piece::BISHOP, Color::BLACK),
        board.getTypedPieces(Piece::KNIGHT,  Color::WHITE) | board.getTypedPieces(Piece::KNIGHT, Color::BLACK),
        board.getTypedPieces(Piece::PAWN,    Color::WHITE) | board.getTypedPieces(Piece::PAWN,   Color::BLACK),
        board.getEnpassantSquare() == 64 ? 0 : board.getEnpassantSquare(),
        board.getTurn()^1
    );

    switch (result)
    {
    case TB_RESULT_FAILED:
        return Syzygy::WDLResult::FAILED;
    case TB_LOSS:
        return Syzygy::WDLResult::LOSS;
    case TB_WIN:
        return Syzygy::WDLResult::WIN;
    default:
        // This covers TB_DRAW, TB_BLESSED_LOSS and TB_CURSED_WIN
        return Syzygy::WDLResult::DRAW;
    }
}

bool Syzygy::TBInit(std::string path)
{
    bool initialized = tb_init(path.c_str());

    if(!initialized)
    {
        WARNING("Failed to initialize syzygy: " << path)
    }
    else
    {
        DEBUG("Initialized syzygy: " << path)
        DEBUG("Syzygy largest piece set: " << TB_LARGEST)
    }

    return initialized;
}

void Syzygy::TBFree()
{
    tb_free();
}