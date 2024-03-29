#pragma once

#include <types.hpp>
#include <utils.hpp>
#include <cstdint>
#include <iostream>
#include <bitset>
#include <sstream>

#if __x86_64__
    // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html
    #ifdef __linux__
    #include <x86intrin.h>
    #else
    #include <intrin.h>
    #endif
#endif

#define RANK(_square) ((_square) >> 3)
#define FILE(_square) ((_square) & 0b111)

namespace Arcanum
{
    typedef uint64_t bitboard_t;

    void initGenerateBetweens();
    // Includes the open set of squares between two squares.
    // I.e. not including to and from
    // Zero if not on the same rank, file or diagonal
    extern bitboard_t betweens[64][64];

    void initGenerateKnightAttacks();
    extern bitboard_t knightAttacks[64];

    void initGenerateKingMoves();
    extern bitboard_t kingMoves[64];

    void initGenerateRookMoves();
#ifdef USE_BMI2
    extern bitboard_t rookOccupancyMask[64];
    extern bitboard_t rookMoves[64][1 << 12]; // 12 occupancy bits for 6 file and 6 for rank
#else
    extern bitboard_t rookFileMoves[8 * (1 << 6)];
    extern bitboard_t rookRankMoves[8 * (1 << 6)];
#endif

    void initGenerateBishopMoves();
#ifdef USE_BMI2
    extern bitboard_t bishopOccupancyMask[64];
    extern bitboard_t bishopMoves[64][1 << 12];
#else
    extern bitboard_t bishopMoves[8 * (1 << 6)];
    extern bitboard_t diagonal[64];
    extern bitboard_t antiDiagonal[64];
#endif
    static inline void printBitBoard(bitboard_t bitboard)
    {
        for(int i = 7; i >= 0; i--)
        {
            std::bitset<8> reversed;
            std::bitset<8> x(bitboard >> (i << 3));
            for (int i = 0, j = 7; i < 8; i++, j--) {
                reversed[j] = x[i];
            }
            LOG(reversed)
        }
    }

    static inline int CNTSBITS(const bitboard_t bitboard)
    {
        #ifdef USE_POPCNT
            return _popcnt64 (bitboard);
        #else

            ERROR("CNTSBITS not implemented")
            return 0;
        #endif
    }

    // Source: https://www.chessprogramming.org/BitScan
    // returns the index of the lsb 1 bit and sets it to zero
    static inline int popLS1B(bitboard_t* bitboard)
    {
    #ifdef USE_BMI
        int popIdx = _tzcnt_u64(*bitboard);
        *bitboard = _blsr_u64(*bitboard);
    #else
        /**
         * bitScanForward
         * @author Kim Walisch (2012)
         * @param bitboard bitboard to scan
         * @precondition bb != 0
         * @return index (0..63) of least significant one bit
         */

        constexpr static square_t popLS1B_index64[64] = {
            0, 47,  1, 56, 48, 27,  2, 60,
            57, 49, 41, 37, 28, 16,  3, 61,
            54, 58, 35, 52, 50, 42, 21, 44,
            38, 32, 29, 23, 17, 11,  4, 62,
            46, 55, 26, 59, 40, 36, 15, 53,
            34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30,  9, 24,
            13, 18,  8, 12,  7,  6,  5, 63
        };

        constexpr static bitboard_t debruijn64 = bitboard_t(0x03f79d71b4cb0a89);
        // assert (bitboard != 0);
        int popIdx = popLS1B_index64[((*bitboard ^ (*bitboard-1)) * debruijn64) >> 58];
        // Pop the bit
        *bitboard &= ~(0b1LL << popIdx);
    #endif
        return popIdx;
    }

    static inline int LS1B(bitboard_t bitboard)
    {
    #ifdef USE_BMI
        int idx = _tzcnt_u64(bitboard);
        return idx;
    #else
        /**
         * bitScanForward
         * @author Kim Walisch (2012)
         * @param bitboard bitboard to scan
         * @precondition bb != 0
         * @return index (0..63) of least significant one bit
         */

        constexpr static square_t popLS1B_index64[64] = {
            0, 47,  1, 56, 48, 27,  2, 60,
            57, 49, 41, 37, 28, 16,  3, 61,
            54, 58, 35, 52, 50, 42, 21, 44,
            38, 32, 29, 23, 17, 11,  4, 62,
            46, 55, 26, 59, 40, 36, 15, 53,
            34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30,  9, 24,
            13, 18,  8, 12,  7,  6,  5, 63
        };

        constexpr static bitboard_t debruijn64 = bitboard_t(0x03f79d71b4cb0a89);
        // assert (bitboard != 0);
        return popLS1B_index64[((bitboard ^ (bitboard-1)) * debruijn64) >> 58];
    #endif
    }

    static inline int MS1B(bitboard_t bitboard)
    {
    #ifdef USE_LZCNT
        constexpr int bbBitSize = 8*sizeof(bitboard) - 1;
        int lzeros = _lzcnt_u64(bitboard);
        return bbBitSize - lzeros;
    #else
        ERROR("Missing implementation of MS1B")
        exit(-1);
        return 0;
    #endif
    }

    static inline bitboard_t getWhitePawnAttacks(const bitboard_t bitboard)
    {
        constexpr static bitboard_t nAFile = ~0x0101010101010101;  // Every square except the A file
        constexpr static bitboard_t nHFile = ~0x8080808080808080;  // Every square except the H file
        bitboard_t pawnAttacks = ((bitboard & nHFile) << 9) | ((bitboard & nAFile) << 7);
        return pawnAttacks;
    }

    static inline bitboard_t getWhitePawnAttacksLeft(const bitboard_t bitboard)
    {
        constexpr static bitboard_t nAFile = ~0x0101010101010101;  // Every square except the A file
        return ((bitboard & nAFile) << 7);
    }

    static inline bitboard_t getWhitePawnAttacksRight(const bitboard_t bitboard)
    {
        constexpr static bitboard_t nHFile = ~0x8080808080808080;  // Every square except the H file
        return ((bitboard & nHFile) << 9);
    }

    static inline bitboard_t getBlackPawnAttacks(const bitboard_t bitboard)
    {
        constexpr static bitboard_t nAFile = ~0x0101010101010101;  // Every square except the A file
        constexpr static bitboard_t nHFile = ~0x8080808080808080;  // Every square except the H file
        bitboard_t pawnAttacks = ((bitboard & nHFile) >> 7) | ((bitboard & nAFile) >> 9);
        return pawnAttacks;
    }

    static inline bitboard_t getBlackPawnAttacksLeft(const bitboard_t bitboard)
    {
        constexpr static bitboard_t nAFile = ~0x0101010101010101;  // Every square except the A file
        return ((bitboard & nAFile) >> 9);
    }

    static inline bitboard_t getBlackPawnAttacksRight(const bitboard_t bitboard)
    {
        constexpr static bitboard_t nHFile = ~0x8080808080808080;  // Every square except the H file
        return ((bitboard & nHFile) >> 7);
    }

    static inline bitboard_t getWhitePawnMoves(const bitboard_t bitboard)
    {
        return bitboard << 8;
    }

    static inline bitboard_t getWhitePawnMove(const uint8_t pawnIdx)
    {
        return (0b1LL << pawnIdx) << 8;
    }

    static inline bitboard_t getBlackPawnMoves(const bitboard_t bitboard)
    {
        return bitboard >> 8;
    }

    static inline bitboard_t getBlackPawnMove(const uint8_t pawnIdx)
    {
        return (0b1LL << pawnIdx) >> 8;
    }

    static inline bitboard_t getAllKnightsAttacks(bitboard_t bitboard)
    {
        // For all knights get the attack bitboard
        bitboard_t knightAttacksBitBoard = 0LL;
        while (bitboard)
        {
            // Index of the knight
            int kidx = popLS1B(&bitboard);
            // Find and add the attack bitboard for the knight
            knightAttacksBitBoard |= knightAttacks[kidx];
        }
        return knightAttacksBitBoard;
    }

    static inline bitboard_t getKnightAttacks(const uint8_t knightIdx)
    {
        return knightAttacks[knightIdx];
    }

    static inline bitboard_t getKingMoves(const uint8_t kingIdx)
    {
        // Find and add the attack bitboard for the knight
        return kingMoves[kingIdx];
    }

    static inline bitboard_t getRookMoves(const bitboard_t allPiecesBitboard, const uint8_t rookIdx)
    {
        #ifdef USE_BMI2
        bitboard_t occupancyIdx = _pext_u64(allPiecesBitboard, rookOccupancyMask[rookIdx]);
        return rookMoves[rookIdx][occupancyIdx];
        #else
        // Find file and rank of rook
        const uint8_t file = FILE(rookIdx);
        const uint8_t rank8 = rookIdx & ~0b111; // 8 * rank

        // Shift the file down to the first rank and get the 6 middle squares
        const bitboard_t fileOccupied = (allPiecesBitboard >> (rank8 + 1)) & 0b111111LL;
        // Read the move bitboard and shift it to the correct rank
        const bitboard_t fileMoves = rookFileMoves[(file << 6) | fileOccupied] << rank8;

        // Shift the file to the A file and mask it
        const bitboard_t rankOccupied = (allPiecesBitboard >> file) & (0x0101010101010101LL);
        // Find the occupancy index using https://www.chessprogramming.org/Kindergarten_Bitboards
        const bitboard_t rankOccupiedIdx = (rankOccupied * 0x4081020408000LL) >> 58;
        const bitboard_t rankMoves = rookRankMoves[(rank8 << 3) | rankOccupiedIdx] << file;

        return fileMoves | rankMoves;
        #endif
    }

    // https://www.chessprogramming.org/Efficient_Generation_of_Sliding_Piece_Attacks
    static inline bitboard_t getBishopMoves(const bitboard_t allPiecesBitboard, const uint8_t bishopIdx)
    {
        #ifdef USE_BMI2
        bitboard_t occupancyIdx = _pext_u64(allPiecesBitboard, bishopOccupancyMask[bishopIdx]);
        return bishopMoves[bishopIdx][occupancyIdx];
        #else
        const uint8_t file = FILE(bishopIdx);

        constexpr static bitboard_t bFile = 0x0202020202020202LL;
        const bitboard_t diagonalOccupancy     = ((diagonal[bishopIdx] & allPiecesBitboard) * bFile) >> 58;
        const bitboard_t antiDiagonalOccupancy = ((antiDiagonal[bishopIdx] & allPiecesBitboard) * bFile) >> 58;
        const bitboard_t moves = (
            (diagonal[bishopIdx] & bishopMoves[file << 6 | diagonalOccupancy]) |
            (antiDiagonal[bishopIdx] & bishopMoves[file << 6 | antiDiagonalOccupancy])
        );

        return moves;
        #endif
    }

    static inline bitboard_t getQueenMoves(const bitboard_t allPiecesBitboard, const square_t queenIdx)
    {
        return getRookMoves(allPiecesBitboard, queenIdx) | getBishopMoves(allPiecesBitboard, queenIdx);
    }

    static inline bitboard_t getBetweens(const square_t fromIdx, const square_t toIdx)
    {
        return betweens[fromIdx][toIdx];
    }

    static inline std::string getArithmeticNotation(const square_t square)
    {
        std::stringstream ss;

        int rank = RANK(square);
        int file = FILE(square);

        ss << char('a' + file) << char('1' + rank);

        return ss.str();
    }
}