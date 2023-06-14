
#include <board.hpp>
#include <random>
#include <utils.hpp>

using namespace ChessEngine2;

Zobrist::Zobrist()
{
    // TODO: Should castle rights be a part of the hash
    std::default_random_engine generator;
    std::uniform_int_distribution<uint64_t> distribution(0, UINT64_MAX);

    for(int i = 0; i < 12; i++)
    {
        for(int j = 0; j < 64; j++)
        {
            m_tables[i][j] = distribution(generator);
        }
    }
    m_blackToMove = distribution(generator);
}

Zobrist::~Zobrist()
{

}

inline hash_t Zobrist::m_addAllPieces(hash_t hash, bitboard_t bitboard, int tableIdx)
{
    while (bitboard)
    {
        int pieceIdx = popLS1B(&bitboard);
        hash ^= m_tables[tableIdx][pieceIdx];
    }

    return hash;
}

hash_t Zobrist::getHash(const Board &board)
{
    hash_t hash = 0LL;

    // Pawns
    hash = m_addAllPieces(hash, board.m_bbPawns[WHITE], ChessEngine2::ZOBRIST_WHITE_PAWN_TABLE_IDX);
    hash = m_addAllPieces(hash, board.m_bbPawns[BLACK], ChessEngine2::ZOBRIST_BLACK_PAWN_TABLE_IDX);
    // Rooks
    hash = m_addAllPieces(hash, board.m_bbRooks[WHITE], ChessEngine2::ZOBRIST_WHITE_ROOK_TABLE_IDX);
    hash = m_addAllPieces(hash, board.m_bbRooks[BLACK], ChessEngine2::ZOBRIST_BLACK_ROOK_TABLE_IDX);
    // Knights
    hash = m_addAllPieces(hash, board.m_bbKnights[WHITE], ChessEngine2::ZOBRIST_WHITE_KNIGHT_TABLE_IDX);
    hash = m_addAllPieces(hash, board.m_bbKnights[BLACK], ChessEngine2::ZOBRIST_BLACK_KNIGHT_TABLE_IDX);
    // Bishops
    hash = m_addAllPieces(hash, board.m_bbBishops[WHITE], ChessEngine2::ZOBRIST_WHITE_BISHOP_TABLE_IDX);
    hash = m_addAllPieces(hash, board.m_bbBishops[BLACK], ChessEngine2::ZOBRIST_BLACK_BISHOP_TABLE_IDX);
    // Queens
    hash = m_addAllPieces(hash, board.m_bbQueens[WHITE], ChessEngine2::ZOBRIST_WHITE_QUEEN_TABLE_IDX);
    hash = m_addAllPieces(hash, board.m_bbQueens[BLACK], ChessEngine2::ZOBRIST_BLACK_QUEEN_TABLE_IDX);
    // Kings
    hash = m_addAllPieces(hash, board.m_bbKing[WHITE], ChessEngine2::ZOBRIST_WHITE_KING_TABLE_IDX);
    hash = m_addAllPieces(hash, board.m_bbKing[BLACK], ChessEngine2::ZOBRIST_BLACK_KING_TABLE_IDX);

    if(board.m_turn == BLACK)
    {
        hash ^= m_blackToMove;
    }

    return hash;
}

hash_t Zobrist::getUpdatedHash(const Board &board, Move move)
{
    hash_t hash = board.m_hash;
    // XOR out and in the moved piece
    uint8_t tableIndex = LS1B(move.moveInfo & (
        MOVE_INFO_PAWN_MOVE  
        | MOVE_INFO_ROOK_MOVE  
        | MOVE_INFO_KNIGHT_MOVE
        | MOVE_INFO_BISHOP_MOVE
        | MOVE_INFO_QUEEN_MOVE
        | MOVE_INFO_KING_MOVE
    ));

    hash ^= m_tables[tableIndex + board.m_turn][move.from];
    hash ^= m_tables[tableIndex + board.m_turn][move.to];

    uint32_t captureBitmask = move.moveInfo & (
        MOVE_INFO_CAPTURE_PAWN  
        | MOVE_INFO_CAPTURE_ROOK  
        | MOVE_INFO_CAPTURE_KNIGHT
        | MOVE_INFO_CAPTURE_QUEEN
        | MOVE_INFO_CAPTURE_BISHOP
    );

    if(captureBitmask != 0)
    {
        tableIndex = LS1B(captureBitmask) - 16;

        Color oponent = Color(board.m_turn^1);
        if(move.moveInfo & MOVE_INFO_ENPASSANT)
        {
            hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_PAWN_TABLE_IDX + oponent][board.m_enPassantTarget];
        }
        else
        {
            hash ^= m_tables[tableIndex + oponent][move.to]; 
        }
    }

    hash ^= m_blackToMove;

    return hash;
}