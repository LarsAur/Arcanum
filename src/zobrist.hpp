#pragma once

#include <types.hpp>
#include <board.hpp>

namespace Arcanum
{
    class Zobrist
    {
        private:
            static hash_t m_tables[6][2][64];
            static hash_t m_enPassantTable[65]; // Only 16 is actually used, index 64 is used to not read out of bounds
            static hash_t m_castleRights[16];
            static hash_t m_blackToMove;

            static void m_addAllPieces(hash_t &hash, hash_t &materialHash, bitboard_t bitboard, uint8_t pieceType, Color pieceColor);
        public:
            static void init();
            static void getHashes(const Board &board, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
            static void getUpdatedHashes(const Board &board, Move move, square_t oldEnPassantSquare, square_t newEnPassantSquare, uint8_t oldCastleRights, uint8_t newCastleRights, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
            static void updateHashesAfterNullMove(hash_t& hash, hash_t& pawnHash, square_t oldEnPassantSquare);
    };
}