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

namespace Arcanum
{
    inline uint64_t CNTSBITS(const uint64_t v)
    {
    #ifdef USE_POPCNT
        return _popcnt64(v);
    #else
        #error "Missing implementation of MS1B"
    #endif
    }

    // Source: https://www.chessprogramming.org/BitScan
    // returns the index of the lsb 1 bit and sets it to zero
    inline int popLS1B(bitboard_t* bitboard)
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

        constexpr square_t popLS1B_index64[64] = {
            0, 47,  1, 56, 48, 27,  2, 60,
            57, 49, 41, 37, 28, 16,  3, 61,
            54, 58, 35, 52, 50, 42, 21, 44,
            38, 32, 29, 23, 17, 11,  4, 62,
            46, 55, 26, 59, 40, 36, 15, 53,
            34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30,  9, 24,
            13, 18,  8, 12,  7,  6,  5, 63
        };

        constexpr bitboard_t debruijn64 = bitboard_t(0x03f79d71b4cb0a89);
        // assert (bitboard != 0);
        int popIdx = popLS1B_index64[((*bitboard ^ (*bitboard-1)) * debruijn64) >> 58];
        // Pop the bit
        *bitboard &= ~(0b1LL << popIdx);
    #endif
        return popIdx;
    }

    inline uint64_t LS1B(bitboard_t bitboard)
    {
    #ifdef USE_BMI
        return _tzcnt_u64(bitboard);
    #else
        /**
         * bitScanForward
         * @author Kim Walisch (2012)
         * @param bitboard bitboard to scan
         * @precondition bb != 0
         * @return index (0..63) of least significant one bit
         */

        constexpr square_t popLS1B_index64[64] = {
            0, 47,  1, 56, 48, 27,  2, 60,
            57, 49, 41, 37, 28, 16,  3, 61,
            54, 58, 35, 52, 50, 42, 21, 44,
            38, 32, 29, 23, 17, 11,  4, 62,
            46, 55, 26, 59, 40, 36, 15, 53,
            34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30,  9, 24,
            13, 18,  8, 12,  7,  6,  5, 63
        };

        constexpr bitboard_t debruijn64 = bitboard_t(0x03f79d71b4cb0a89);
        return popLS1B_index64[((bitboard ^ (bitboard-1)) * debruijn64) >> 58];
    #endif
    }

    inline uint64_t MS1B(uint64_t v)
    {
    #ifdef USE_LZCNT
        constexpr uint64_t bits = 8 * sizeof(v) - 1;
        uint64_t lzeros = _lzcnt_u64(v);
        return bits - lzeros;
    #else
        #error "Missing implementation of MS1B"
    #endif
    }

    inline uint64_t PEXT(uint64_t v, uint64_t mask)
    {
    #ifdef USE_BMI2
        return _pext_u64(v, mask);
    #else
        #error "Missing implementation of PEXT"
    #endif
    }

    inline uint64_t ROTL(uint64_t v, uint8_t shift)
    {
        return _rotl64(v, shift);
    }
}