
#include <board.hpp>
#include <random>
#include <utils.hpp>

using namespace Arcanum;

Zobrist::Zobrist()
{
    // TODO: Should castle rights be a part of the hash
    std::default_random_engine generator;
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
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_PAWN][WHITE], W_PAWN, WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_PAWN][BLACK], W_PAWN, BLACK);
    pawnHash = hash;
    // Rooks
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_ROOK][WHITE], W_ROOK, WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_ROOK][BLACK], W_ROOK, BLACK);
    // Knights
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_KNIGHT][WHITE], W_KNIGHT, WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_KNIGHT][BLACK], W_KNIGHT, BLACK);
    // Bishops
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_BISHOP][WHITE], W_BISHOP, WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_BISHOP][BLACK], W_BISHOP, BLACK);
    // Queens
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_QUEEN][WHITE], W_QUEEN, WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_QUEEN][BLACK], W_QUEEN, BLACK);
    // Kings
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_KING][WHITE], W_KING, WHITE);
    m_addAllPieces(hash, materialHash, board.m_bbTypedPieces[W_KING][BLACK], W_KING, BLACK);

    if(board.m_turn == BLACK)
    {
        hash ^= m_blackToMove;
    }

    if(board.m_enPassantSquare != 64)
    {
        hash ^= m_enPassantTable[board.m_enPassantSquare];
        pawnHash ^= m_enPassantTable[board.m_enPassantSquare];
    }
}

void Zobrist::getUpdatedHashs(const Board &board, Move move, uint8_t oldEnPassantSquare, uint8_t newEnPassantSquare, hash_t &hash, hash_t &pawnHash, hash_t &materialHash)
{
    // TODO: Include castle opertunities
    // XOR in and out the moved piece corresponding to its location
    if(move.moveInfo & MOVE_INFO_PROMOTE_MASK)
    {
        Piece promoteType = Piece(LS1B(move.moveInfo & MOVE_INFO_PROMOTE_MASK) - 11);
        uint8_t pawnCount = CNTSBITS(board.m_bbTypedPieces[W_PAWN][board.m_turn]);
        uint8_t promoteCount = CNTSBITS(board.m_bbTypedPieces[promoteType][board.m_turn]) - 1;
        hash ^= m_tables[W_PAWN][board.m_turn][move.from] ^ m_tables[promoteType][board.m_turn][move.to];
        pawnHash ^= m_tables[W_PAWN][board.m_turn][move.from];
        materialHash ^= m_tables[promoteType][board.m_turn][promoteCount] ^ m_tables[W_PAWN][board.m_turn][pawnCount];
    }
    else
    {
        uint8_t pieceIndex = LS1B(move.moveInfo & MOVE_INFO_MOVE_MASK);
        hash_t moveHash = m_tables[pieceIndex][board.m_turn][move.from] ^ m_tables[pieceIndex][board.m_turn][move.to];
        hash ^= moveHash;
        pawnHash ^= moveHash * (move.moveInfo & MOVE_INFO_PAWN_MOVE); // Multiplication works as MOVE_INFO_PAWN_MOVE == 1
    }

    // Handle the moved rook when castling
    if(move.moveInfo & MOVE_INFO_CASTLE_MASK)
    {
        if(move.moveInfo & MOVE_INFO_CASTLE_WHITE_QUEEN)
        {
            hash ^= m_tables[W_ROOK][WHITE][0] ^  m_tables[W_ROOK][WHITE][3];
        } 
        else if(move.moveInfo & MOVE_INFO_CASTLE_WHITE_KING)
        {
            hash ^= m_tables[W_ROOK][WHITE][7] ^  m_tables[W_ROOK][WHITE][5];
        }
        else if(move.moveInfo & MOVE_INFO_CASTLE_BLACK_QUEEN)
        {
            hash ^= m_tables[W_ROOK][BLACK][56] ^ m_tables[W_ROOK][BLACK][59];
        }
        else if(move.moveInfo & MOVE_INFO_CASTLE_BLACK_KING)
        {
            hash ^= m_tables[W_ROOK][BLACK][63] ^ m_tables[W_ROOK][BLACK][61];
        }
    }

    // XOR out the captured piece
    uint32_t capturedBitmask = move.moveInfo & MOVE_INFO_CAPTURE_MASK;
    if(capturedBitmask != 0)
    {
        Color opponent = Color(board.m_turn^1);

        if(move.moveInfo & MOVE_INFO_ENPASSANT)
        {
            uint8_t oldEnPassantTarget = oldEnPassantSquare > 32 ? oldEnPassantSquare - 8 : oldEnPassantSquare + 8;
            uint8_t count = CNTSBITS(board.m_bbTypedPieces[W_PAWN][opponent]);
            hash ^= m_tables[W_PAWN][opponent][oldEnPassantTarget];
            pawnHash ^= m_tables[W_PAWN][opponent][oldEnPassantTarget];
            materialHash ^= m_tables[W_PAWN][opponent][count];
        }
        else if(move.moveInfo & MOVE_INFO_CAPTURE_PAWN)
        {
            uint8_t count = CNTSBITS(board.m_bbTypedPieces[W_PAWN][opponent]);
            hash ^= m_tables[W_PAWN][opponent][move.to];
            pawnHash ^= m_tables[W_PAWN][opponent][move.to];
            materialHash ^= m_tables[W_PAWN][opponent][count];
        }
        else
        {
            uint8_t capturedIndex = LS1B(capturedBitmask) - 16;
            uint8_t count = CNTSBITS(board.m_bbTypedPieces[capturedIndex][opponent]);
            hash ^= m_tables[capturedIndex][opponent][move.to];
            materialHash ^= m_tables[capturedIndex][opponent][count];
        }
    }
    
    hash_t enPassantHash = (m_enPassantTable[oldEnPassantSquare] * (oldEnPassantSquare != 64)) ^ (m_enPassantTable[newEnPassantSquare] * (newEnPassantSquare != 64));
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
        DEBUG(board.getBoardString())
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