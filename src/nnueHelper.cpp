#include <nnueHelper.hpp>
#include <chessutils.hpp>

static int lsb64(uint64_t v)
{
    return _tzcnt_u64(v);
}

static int popLsb64(uint64_t* v)
{
    int popIdx = _tzcnt_u64(*v);
    *v = _blsr_u64(*v);
    return popIdx;
}

void NN::loadNNUE(std::string filename)
{
    LOG("Loading NNUE: " << filename)
    nnue_init(filename.c_str());
}

int NN::nnueEvaluateBoard(Arcanum::Board& board)
{
    int player = board.getTurn(); // White == 0, Black == 1
    int pieces[33];
    int squares[33];

    // Set kings
    // White and black king have to be index 0 and 1
    squares[0] = lsb64(board.getTypedPieces(Arcanum::Piece::W_KING, Arcanum::Color::WHITE));
    squares[1] = lsb64(board.getTypedPieces(Arcanum::Piece::W_KING, Arcanum::Color::BLACK));
    pieces[0] = wking;
    pieces[1] = bking;

    int idxCounter = 1;
    // Set remaining pieces

    // Pawns
    Arcanum::bitboard_t whitePawns = board.getTypedPieces(Arcanum::Piece::W_PAWN, Arcanum::Color::WHITE);
    while (whitePawns) { squares[++idxCounter] = popLsb64(&whitePawns); pieces[idxCounter] = wpawn; }
    Arcanum::bitboard_t blackPawns = board.getTypedPieces(Arcanum::Piece::W_PAWN, Arcanum::Color::BLACK);
    while (blackPawns) { squares[++idxCounter] = popLsb64(&blackPawns); pieces[idxCounter] = bpawn; }
    
    // Rooks
    Arcanum::bitboard_t whiteRooks = board.getTypedPieces(Arcanum::Piece::W_ROOK, Arcanum::Color::WHITE);
    while (whiteRooks) { squares[++idxCounter] = popLsb64(&whiteRooks); pieces[idxCounter] = wrook; }
    Arcanum::bitboard_t blackRooks = board.getTypedPieces(Arcanum::Piece::W_ROOK, Arcanum::Color::BLACK);
    while (blackRooks) { squares[++idxCounter] = popLsb64(&blackRooks); pieces[idxCounter] = brook; }

    // Knights
    Arcanum::bitboard_t whiteKnights = board.getTypedPieces(Arcanum::Piece::W_KNIGHT, Arcanum::Color::WHITE);
    while (whiteKnights) { squares[++idxCounter] = popLsb64(&whiteKnights); pieces[idxCounter] = wknight; }
    Arcanum::bitboard_t blackKnights = board.getTypedPieces(Arcanum::Piece::W_KNIGHT, Arcanum::Color::BLACK);
    while (blackKnights) { squares[++idxCounter] = popLsb64(&blackKnights); pieces[idxCounter] = bknight; }

    // Bishops
    Arcanum::bitboard_t whiteBishops = board.getTypedPieces(Arcanum::Piece::W_BISHOP, Arcanum::Color::WHITE);
    while (whiteBishops) { squares[++idxCounter] = popLsb64(&whiteBishops); pieces[idxCounter] = wbishop; }
    Arcanum::bitboard_t blackBishops = board.getTypedPieces(Arcanum::Piece::W_BISHOP, Arcanum::Color::BLACK);
    while (blackBishops) { squares[++idxCounter] = popLsb64(&blackBishops); pieces[idxCounter] = bbishop; }

    // Queens
    Arcanum::bitboard_t whiteQueens = board.getTypedPieces(Arcanum::Piece::W_QUEEN, Arcanum::Color::WHITE);
    while (whiteQueens) { squares[++idxCounter] = popLsb64(&whiteQueens); pieces[idxCounter] = wqueen; }
    Arcanum::bitboard_t blackQueens = board.getTypedPieces(Arcanum::Piece::W_QUEEN, Arcanum::Color::BLACK);
    while (blackQueens) { squares[++idxCounter] = popLsb64(&blackQueens); pieces[idxCounter] = bqueen; }

    // Set the last index to be zero
    pieces[++idxCounter] = 0;

    return nnue_evaluate(player, pieces, squares);
}