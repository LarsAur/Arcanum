#include <zobrist.hpp>
#include <board.hpp>
#include <utils.hpp>
#include <fen.hpp>
#include <random>

using namespace Arcanum;

Zobrist Zobrist::zobrist = Zobrist();

Zobrist::Zobrist()
{
    // TODO: Should castle rights be a part of the hash
    std::mt19937 generator(0);
    std::uniform_int_distribution<uint64_t> distribution(0, UINT64_MAX);

    for(int i = 0; i < 6; i++)
    {
        for(int j = 0; j < 2; j++)
        {
            for(int k = 0; k < 64; k++)
            {
                m_tables[i][j][k] = distribution(generator);
            }
        }
    }

    for(int i = 0; i < 64; i++)
    {
        m_enPassantTable[i] = distribution(generator);
    }
    m_enPassantTable[Square::NONE] = 0LL;

    m_blackToMove = distribution(generator);
}

Zobrist::~Zobrist()
{

}

inline void Zobrist::m_addAllPieces(hash_t &hash, hash_t &materialHash, bitboard_t bitboard, uint8_t pieceType, Color pieceColor)
{
    int i = 0;
    while (bitboard)
    {
        int pieceIdx = popLS1B(&bitboard);
        materialHash ^= m_tables[pieceType][pieceColor][i];
        hash ^= m_tables[pieceType][pieceColor][pieceIdx];
        i++;
    }
}

void Zobrist::getHashs(const Board &board, hash_t &hash, hash_t &pawnHash, hash_t &materialHash)
{
    hash = 0LL;
    pawnHash = 0LL;
    materialHash = 0LL;

    // Pawns
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::PAWN][Color::WHITE], Piece::PAWN, Color::WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::PAWN][Color::BLACK], Piece::PAWN, Color::BLACK);
    pawnHash = hash;
    // Rooks
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::ROOK][Color::WHITE], Piece::ROOK, Color::WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::ROOK][Color::BLACK], Piece::ROOK, Color::BLACK);
    // Knights
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::KNIGHT][Color::WHITE], Piece::KNIGHT, Color::WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::KNIGHT][Color::BLACK], Piece::KNIGHT, Color::BLACK);
    // Bishops
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::BISHOP][Color::WHITE], Piece::BISHOP, Color::WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::BISHOP][Color::BLACK], Piece::BISHOP, Color::BLACK);
    // Queens
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::QUEEN][Color::WHITE], Piece::QUEEN, Color::WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::QUEEN][Color::BLACK], Piece::QUEEN, Color::BLACK);
    // Kings
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::KING][Color::WHITE], Piece::KING, Color::WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[Piece::KING][Color::BLACK], Piece::KING, Color::BLACK);

    if(board.m_turn == BLACK)
    {
        hash ^= m_blackToMove;
    }

    hash     ^= m_enPassantTable[board.m_enPassantSquare];
    pawnHash ^= m_enPassantTable[board.m_enPassantSquare];
}

void Zobrist::getUpdatedHashs(const Board &board, Move move, square_t oldEnPassantSquare, square_t newEnPassantSquare, hash_t &hash, hash_t &pawnHash, hash_t &materialHash)
{
    // TODO: Include castle opertunities
    // XOR in and out the moved piece corresponding to its location
    if(move.isPromotion())
    {
        Piece promoteType = move.promotedPiece();
        uint8_t pawnCount = CNTSBITS(board.m_bbTypedPieces[Piece::PAWN][board.m_turn]);
        uint8_t promoteCount = CNTSBITS(board.m_bbTypedPieces[promoteType][board.m_turn]) - 1;
        hash ^= m_tables[Piece::PAWN][board.m_turn][move.from] ^ m_tables[promoteType][board.m_turn][move.to];
        pawnHash ^= m_tables[Piece::PAWN][board.m_turn][move.from];
        materialHash ^= m_tables[promoteType][board.m_turn][promoteCount] ^ m_tables[Piece::PAWN][board.m_turn][pawnCount];
    }
    else
    {
        uint8_t pieceIndex = move.movedPiece();
        hash_t moveHash = m_tables[pieceIndex][board.m_turn][move.from] ^ m_tables[pieceIndex][board.m_turn][move.to];
        hash ^= moveHash;
        pawnHash ^= moveHash * (move.moveInfo & MoveInfoBit::PAWN_MOVE); // Multiplication works as MoveInfoBit::PAWN_MOVE == 1
    }

    // Handle the moved rook when castling
    if(move.isCastle())
    {
        const CastleIndex castleIndex = move.castleIndex();
        const square_t rookFrom = Move::CastleRookFrom[castleIndex];
        const square_t rookTo = Move::CastleRookTo[castleIndex];
        hash ^= m_tables[Piece::ROOK][board.m_turn][rookTo] ^ m_tables[Piece::ROOK][board.m_turn][rookFrom];
    }

    // XOR out the captured piece
    if(move.isCapture())
    {
        Color opponent = Color(board.m_turn^1);

        if(move.moveInfo & MoveInfoBit::ENPASSANT)
        {
            Arcanum::square_t oldEnPassantTarget = oldEnPassantSquare > 32 ? oldEnPassantSquare - 8 : oldEnPassantSquare + 8;
            uint8_t count = CNTSBITS(board.m_bbTypedPieces[Piece::PAWN][opponent]);
            hash ^= m_tables[Piece::PAWN][opponent][oldEnPassantTarget];
            pawnHash ^= m_tables[Piece::PAWN][opponent][oldEnPassantTarget];
            materialHash ^= m_tables[Piece::PAWN][opponent][count];
        }
        else if(move.moveInfo & MoveInfoBit::CAPTURE_PAWN)
        {
            uint8_t count = CNTSBITS(board.m_bbTypedPieces[Piece::PAWN][opponent]);
            hash ^= m_tables[Piece::PAWN][opponent][move.to];
            pawnHash ^= m_tables[Piece::PAWN][opponent][move.to];
            materialHash ^= m_tables[Piece::PAWN][opponent][count];
        }
        else
        {
            uint8_t capturedIndex = move.capturedPiece();
            uint8_t count = CNTSBITS(board.m_bbTypedPieces[capturedIndex][opponent]);
            hash ^= m_tables[capturedIndex][opponent][move.to];
            materialHash ^= m_tables[capturedIndex][opponent][count];
        }
    }

    hash_t enPassantHash = m_enPassantTable[oldEnPassantSquare] ^ m_enPassantTable[newEnPassantSquare];
    pawnHash ^= enPassantHash;
    hash ^= enPassantHash;
    hash ^= m_blackToMove;

    #ifdef VERIFY_HASH
    // Verify hash
    hash_t h, ph, mh;
    Board b(board);
    b.m_turn = Color(b.m_turn^1);
    getHashs(b, h, ph, mh);
    if(h != hash || ph != pawnHash || mh != materialHash)
    {
        DEBUG(FEN::toString(board))
        DEBUG(move << " " << move.moveInfo)
        if(hash != h)
        ERROR("Hash: " << hash << " != " << h << " (Correct)")
        if(pawnHash != ph)
        ERROR("Pawn Hash: " << pawnHash << " != " << ph << " (Correct)")
        if(materialHash != mh)
        ERROR("Material Hash: " << materialHash << " != " << mh << " (Correct)")
        exit(EXIT_FAILURE);
    }
    #endif
}

void Zobrist::updateHashsAfterNullMove(hash_t& hash, hash_t& pawnHash, square_t oldEnPassantSquare)
{
    hash_t enPassantHash = m_enPassantTable[oldEnPassantSquare];
    pawnHash ^= enPassantHash;
    hash ^= enPassantHash;
    hash ^= m_blackToMove;
}