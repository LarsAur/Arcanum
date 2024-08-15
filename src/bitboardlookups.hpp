#pragma once

#include <types.hpp>

#define RANK(_SQUARE) ((_SQUARE) >> 3)
#define FILE(_SQUARE) ((_SQUARE) & 0b111)
#define SQUARE(_FILE, _RANK) ((_FILE) + ((_RANK) << 3))
#define SQUARE_BB(_FILE, _RANK) (1LL << (SQUARE(_FILE, _RANK)))

namespace Arcanum
{
    namespace BitboardLookups
    {
        extern bitboard_t betweens[64][64];
        extern bitboard_t knightMoves[64];
        extern bitboard_t kingMoves[64];
        #ifdef USE_BMI2
        extern bitboard_t rookOccupancyMask[64];
        extern bitboard_t rookMoves[64][1 << 12]; // Indexed by rook index and 12-bit occupancy mask (6 for file and 6 for rank)
        extern bitboard_t bishopOccupancyMask[64];
        extern bitboard_t bishopMoves[64][1 << 12]; // Indexed by rook index and 12-bit occupancy mask (6 for file and 6 for rank)
        #else
        extern bitboard_t rookFileMoves[8 * (1 << 6)];
        extern bitboard_t rookRankMoves[8 * (1 << 6)];
        extern bitboard_t bishopMoves[8 * (1 << 6)];
        extern bitboard_t diagonal[64];
        extern bitboard_t antiDiagonal[64];
        #endif

        void generateBitboardLookups();
    }
}

