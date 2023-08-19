#pragma once

#include <cstdint>
#include <iostream>
#include <bitset>
#include <sstream>
#include <utils.hpp>

#if __x86_64__ || _WIN64
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html
#include <intrin.h>
#endif

#define IBM1
#define POPCNT

namespace ChessEngine2
{
    typedef uint64_t bitboard_t;

    void initGenerateKnightAttacks();
    extern bitboard_t knightAttacks[64];

    void initGenerateKingMoves();
    extern bitboard_t kingMoves[64];

    void initGenerateRookMoves();
    extern bitboard_t rookFileMoves[8 * (1 << 6)];
    extern bitboard_t rookRankMoves[8 * (1 << 6)];

    void initGenerateBishopMoves();
    extern bitboard_t bishopMoves[8 * (1 << 6)];
    extern bitboard_t diagonal[64];
    extern bitboard_t antiDiagonal[64];

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

    static inline int CNTSBITS(bitboard_t bitboard)
    {
        #ifdef POPCNT
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
    #ifdef IBM1
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

        // TODO: Consider using uint8_t to save space. Might be more efficient to use int due to alignment
        const static int popLS1B_index64[64] = {
            0, 47,  1, 56, 48, 27,  2, 60,
            57, 49, 41, 37, 28, 16,  3, 61,
            54, 58, 35, 52, 50, 42, 21, 44,
            38, 32, 29, 23, 17, 11,  4, 62,
            46, 55, 26, 59, 40, 36, 15, 53,
            34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30,  9, 24,
            13, 18,  8, 12,  7,  6,  5, 63
        };

        const static bitboard_t debruijn64 = bitboard_t(0x03f79d71b4cb0a89);
        // assert (bitboard != 0);
        int popIdx = popLS1B_index64[((*bitboard ^ (*bitboard-1)) * debruijn64) >> 58];
        // Pop the bit
        *bitboard &= ~(0b1LL << popIdx);
    #endif
        return popIdx;
    }

    static inline int LS1B(bitboard_t bitboard)
    {
    #ifdef IBM1
        int popIdx = _tzcnt_u64(bitboard);
        return popIdx;
    #else
        /**
         * bitScanForward
         * @author Kim Walisch (2012)
         * @param bitboard bitboard to scan
         * @precondition bb != 0
         * @return index (0..63) of least significant one bit
         */

        // TODO: Consider using uint8_t to save space. Might be more efficient to use int due to byte alignment
        const static int popLS1B_index64[64] = {
            0, 47,  1, 56, 48, 27,  2, 60,
            57, 49, 41, 37, 28, 16,  3, 61,
            54, 58, 35, 52, 50, 42, 21, 44,
            38, 32, 29, 23, 17, 11,  4, 62,
            46, 55, 26, 59, 40, 36, 15, 53,
            34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30,  9, 24,
            13, 18,  8, 12,  7,  6,  5, 63
        };

        const static bitboard_t debruijn64 = bitboard_t(0x03f79d71b4cb0a89);
        // assert (bitboard != 0);
        return popLS1B_index64[((bitboard ^ (bitboard-1)) * debruijn64) >> 58];
    #endif
    }

    static inline bitboard_t bitReverse(bitboard_t v) 
    {
        // Altered version of http://graphics.stanford.edu/~seander/bithacks.html#BitReverseObvious
        // swap odd and even bits
        v = ((v >> 1)  & 0x5555555555555555) | ((v & 0x5555555555555555) << 1);
        // swap consecutive pairs
        v = ((v >> 2)  & 0x3333333333333333) | ((v & 0x3333333333333333) << 2);
        // swap nibbles ... 
        v = ((v >> 4)  & 0x0F0F0F0F0F0F0F0F) | ((v & 0x0F0F0F0F0F0F0F0F) << 4);
        // swap bytes
        v = ((v >> 8)  & 0x00FF00FF00FF00FF) | ((v & 0x00FF00FF00FF00FF) << 8);
        // swap 2-byte long pairs
        v = ((v >> 16) & 0x0000FFFF0000FFFF) | ((v & 0x0000FFFF0000FFFF) << 16);
        // swap 4-byte long pairs
        v = (v >> 32) | (v << 32);
        
        return v;
    }

    static inline bitboard_t getWhitePawnAttacks(bitboard_t bitboard)
    {
        const static bitboard_t nAFile = ~0x0101010101010101;  // Every square except the A file
        const static bitboard_t nHFile = ~0x8080808080808080;  // Every square except the H file
        bitboard_t pawnAttacks = ((bitboard & nHFile) << 9) | ((bitboard & nAFile) << 7);
        return pawnAttacks;
    }

    static inline bitboard_t getWhitePawnAttacksLeft(bitboard_t bitboard)
    {
        const static bitboard_t nAFile = ~0x0101010101010101;  // Every square except the A file
        return ((bitboard & nAFile) << 7);
    }

    static inline bitboard_t getWhitePawnAttacksRight(bitboard_t bitboard)
    {
        const static bitboard_t nHFile = ~0x8080808080808080;  // Every square except the H file
        return ((bitboard & nHFile) << 9);
    }

    static inline bitboard_t getBlackPawnAttacks(bitboard_t bitboard)
    {
        const static bitboard_t nAFile = ~0x0101010101010101;  // Every square except the A file
        const static bitboard_t nHFile = ~0x8080808080808080;  // Every square except the H file
        bitboard_t pawnAttacks = ((bitboard & nHFile) >> 7) | ((bitboard & nAFile) >> 9);
        return pawnAttacks;
    }

    static inline bitboard_t getBlackPawnAttacksLeft(bitboard_t bitboard)
    {
        const static bitboard_t nAFile = ~0x0101010101010101;  // Every square except the A file
        return ((bitboard & nAFile) >> 9);
    }

    static inline bitboard_t getBlackPawnAttacksRight(bitboard_t bitboard)
    {
        const static bitboard_t nHFile = ~0x8080808080808080;  // Every square except the H file
        return ((bitboard & nHFile) >> 7);
    }

    static inline bitboard_t getWhitePawnMoves(bitboard_t bitboard)
    {
        return bitboard << 8;
    }

    static inline bitboard_t getWhitePawnMove(uint8_t pawnIdx)
    {
        return (0b1LL << pawnIdx) << 8;
    }

    static inline bitboard_t getBlackPawnMoves(bitboard_t bitboard)
    {
        return bitboard >> 8;
    }

    static inline bitboard_t getBlackPawnMove(uint8_t pawnIdx)
    {
        return (0b1LL << pawnIdx) >> 8;
    }

    static inline bitboard_t getAllKnightsAttacks(bitboard_t bitboard)
    {
        // Local bitboard to be edited
        bitboard_t kbb = bitboard;
        // For all knights get the attack bitboard
        bitboard_t knightAttacksBitBoard = 0LL;
        while (kbb)
        {   
            // Index of the knight
            int kidx = popLS1B(&kbb);
            // Find and add the attack bitboard for the knight
            knightAttacksBitBoard |= knightAttacks[kidx];
        }
        return knightAttacksBitBoard;
    }
    
    static inline bitboard_t getKnightAttacks(bitboard_t knightIdx)
    {
        return knightAttacks[knightIdx];
    }

    static inline bitboard_t getKingMoves(uint8_t kingIdx)
    {
        // Find and add the attack bitboard for the knight
        return kingMoves[kingIdx];
    }

    // Assumes single-rook bitboard
    static inline bitboard_t getRookMoves(bitboard_t allPiecesBitboard, uint8_t rookIdx)
    {
        // Find file and rank of rook
        int file = rookIdx & 0b111;
        int rank8 = rookIdx & ~0b111; // 8 * rank

        // Shift the file down to the first rank and get the 6 middle squares
        bitboard_t fileOccupied = (allPiecesBitboard >> (rank8 + 1)) & 0b111111LL;
        // Read the move bitboard and shift it to the correct rank
        bitboard_t fileMoves = rookFileMoves[(file << 6) | fileOccupied] << rank8; 

        // Shift the file to the A file and mask it
        bitboard_t rankOccupied = (allPiecesBitboard >> file) & (0x0101010101010101LL); 
        // Find the occupancy index using https://www.chessprogramming.org/Kindergarten_Bitboards
        bitboard_t rankOccupiedIdx = (rankOccupied * 0x4081020408000LL) >> 58;
        bitboard_t rankMoves = rookRankMoves[(rank8 << 3) | rankOccupiedIdx] << file;

        return fileMoves | rankMoves;
    }

    // https://www.chessprogramming.org/Efficient_Generation_of_Sliding_Piece_Attacks
    static inline bitboard_t getBishopMoves(bitboard_t allPiecesBitboard, uint8_t bishopIdx)
    {
        int file = bishopIdx & 0b111;

        const static bitboard_t bFile = 0x0202020202020202LL;
        bitboard_t diagonalOccupancy     = ((diagonal[bishopIdx] & allPiecesBitboard) * bFile) >> 58;
        bitboard_t antiDiagonalOccupancy = ((antiDiagonal[bishopIdx] & allPiecesBitboard) * bFile) >> 58;
        bitboard_t moves = (
            (diagonal[bishopIdx] & bishopMoves[file << 6 | diagonalOccupancy]) |
            (antiDiagonal[bishopIdx] & bishopMoves[file << 6 | antiDiagonalOccupancy])
        );

        return moves;
    }

    static inline bitboard_t getQueenMoves(bitboard_t allPiecesBitboard, uint8_t queenIdx)
    {
        return getRookMoves(allPiecesBitboard, queenIdx) | getBishopMoves(allPiecesBitboard, queenIdx);
    }

    static inline std::string getArithmeticNotation(uint8_t square)
    {
        std::stringstream ss;
        
        int rank = square >> 3;
        int file = square & 0b111;

        ss << char('a' + file) << char('1' + rank);

        return ss.str();
    }
}