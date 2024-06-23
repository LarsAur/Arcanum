#pragma once

#include <intrinsics.hpp>
#include <bitboardLookups.hpp>
#include <string>

namespace Arcanum
{
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
            knightAttacksBitBoard |= BitboardLookups::knightMoves[kidx];
        }
        return knightAttacksBitBoard;
    }

    static inline bitboard_t getKnightAttacks(const uint8_t knightIdx)
    {
        return BitboardLookups::knightMoves[knightIdx];
    }

    static inline bitboard_t getKingMoves(const uint8_t kingIdx)
    {
        // Find and add the attack bitboard for the knight
        return BitboardLookups::kingMoves[kingIdx];
    }

    static inline bitboard_t getRookMoves(const bitboard_t allPiecesBitboard, const uint8_t rookIdx)
    {
    #ifdef USE_BMI2
        bitboard_t occupancyIdx = PEXT(allPiecesBitboard, BitboardLookups::rookOccupancyMask[rookIdx]);
        return BitboardLookups::rookMoves[rookIdx][occupancyIdx];
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
        bitboard_t occupancyIdx = PEXT(allPiecesBitboard, BitboardLookups::bishopOccupancyMask[bishopIdx]);
        return BitboardLookups::bishopMoves[bishopIdx][occupancyIdx];
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
        return BitboardLookups::betweens[fromIdx][toIdx];
    }

    static std::string squareToString(square_t square)
    {
        std::string str = "a1";
        str[0] += FILE(square);
        str[1] += RANK(square);
        return str;
    }
}
