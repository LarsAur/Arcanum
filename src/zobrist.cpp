
#include <board.hpp>
#include <random>
#include <utils.hpp>

using namespace ChessEngine2;

Zobrist::Zobrist()
{
    // TODO: Should castle rights be a part of the hash
    srand(0xC00L);
    for(int i = 0; i < 12; i++)
    {
        for(int j = 0; j < 64; j++)
        {
            m_tables[i][j] = (((uint64_t) rand()) << 32) | rand();
        }
    }
    m_blackToMove = (((uint64_t) rand()) << 32) | rand();
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
    if(move.moveInfo & MOVE_INFO_PAWN_MOVE)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_PAWN_TABLE_IDX + board.m_turn][move.from];
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_PAWN_TABLE_IDX + board.m_turn][move.to];
    }
    else if(move.moveInfo & MOVE_INFO_ROOK_MOVE)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_ROOK_TABLE_IDX + board.m_turn][move.from];
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_ROOK_TABLE_IDX + board.m_turn][move.to];
    }
    else if(move.moveInfo & MOVE_INFO_KNIGHT_MOVE)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_KNIGHT_TABLE_IDX + board.m_turn][move.from];
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_KNIGHT_TABLE_IDX + board.m_turn][move.to];
    }
    else if(move.moveInfo & MOVE_INFO_BISHOP_MOVE)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_BISHOP_TABLE_IDX + board.m_turn][move.from];
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_BISHOP_TABLE_IDX + board.m_turn][move.to];
    }
    else if(move.moveInfo & MOVE_INFO_QUEEN_MOVE)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_QUEEN_TABLE_IDX + board.m_turn][move.from];
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_QUEEN_TABLE_IDX + board.m_turn][move.to];
    }
    else if(move.moveInfo & MOVE_INFO_KING_MOVE)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_KING_TABLE_IDX + board.m_turn][move.from];
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_KING_TABLE_IDX + board.m_turn][move.to];
    }
    else
    {
        CHESS_ENGINE2_ERR("Missing piece type")
    }


    Color oponent = Color(board.m_turn^1);
    if(move.moveInfo & MOVE_INFO_CAPTURE_PAWN)
    {
        if(move.moveInfo & MOVE_INFO_ENPASSANT)
        {
            hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_PAWN_TABLE_IDX + oponent][board.m_enPassantTarget];
        }
        else
        {
            hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_PAWN_TABLE_IDX + oponent][move.to];
        }
    }
    else if(move.moveInfo & MOVE_INFO_CAPTURE_ROOK)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_ROOK_TABLE_IDX + oponent][move.to];
    }
    else if(move.moveInfo & MOVE_INFO_CAPTURE_KNIGHT)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_KNIGHT_TABLE_IDX + oponent][move.to];
    }
    else if(move.moveInfo & MOVE_INFO_CAPTURE_BISHOP)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_BISHOP_TABLE_IDX + oponent][move.to];
    }
    else if(move.moveInfo & MOVE_INFO_CAPTURE_QUEEN)
    {
        hash ^= m_tables[ChessEngine2::ZOBRIST_WHITE_QUEEN_TABLE_IDX + oponent][move.to];
    }

    hash ^= m_blackToMove;

    return hash;
}