#pragma once

#include <types.hpp>
#include <board.hpp>

namespace Arcanum
{
    class Zobrist
    {
        private:
            hash_t m_tables[6][2][64];
            hash_t m_enPassantTable[65]; // Only 16 is actually used, index 64 is used to not read out of bounds
            hash_t m_blackToMove;

            void m_addAllPieces(hash_t &hash, hash_t &materialHash, bitboard_t bitboard, uint8_t pieceType, Color pieceColor);
        public:
            Zobrist();
            ~Zobrist();

            void getHashs(const Board &board, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
            void getUpdatedHashs(const Board &board, Move move, square_t oldEnPassentSquare, square_t newEnPassentSquare, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
            void updateHashsAfterNullMove(hash_t& hash, hash_t& pawnHash, square_t oldEnPassantSquare);

            static Zobrist zobrist;
    };
}