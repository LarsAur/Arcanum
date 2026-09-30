#pragma once

#include <types.hpp>
#include <utils.hpp>
#include <cstdint>
#include <iostream>
#include <bitset>
#include <sstream>

#if defined(__x86_64__)
    // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html
    #ifdef __linux__
    #include <x86intrin.h>
    #else
    #include <intrin.h>
    #endif
#endif

namespace Arcanum
{
    // Counts the number of set bits (1s) in the given 64-bit value
    inline uint64_t CNTSBITS(const uint64_t v)
    {
    #if defined(USE_POPCNT)
        return _popcnt64(v);
    #else
        return __builtin_popcountll(v);
    #endif
    }

    // Returns the index of the lsb 1 bit and sets it to zero
    // 64 is returned if no set bit is found (v is 0)
    inline uint32_t popLS1B(uint64_t* v)
    {
    #if defined(USE_BMI)
        uint32_t popIdx = _tzcnt_u64(*v);
        *v = _blsr_u64(*v);
        return popIdx;
    #else
        if(*v == 0) return 64;
        uint32_t popIdx = __builtin_ctzll(*v);
        *v &= (*v - 1); // Pop the least significant 1 bit
        return popIdx;
    #endif
    }
    // Returns the index of the lsb 1 bit
    // 64 is returned if no set bit is found (v is 0)
    inline uint32_t LS1B(uint64_t v)
    {
    #if defined(USE_BMI)
        return _tzcnt_u64(v);
    #else
        // Return 64 if v is 0 to replicate the behavior of _tzcnt_u64 on zero input
        return v ? __builtin_ctzll(v) : 64;
    #endif
    }

    // Returns the index of the most significant 1 bit
    // 64 is returned if v is 0
    inline uint32_t MS1B(uint64_t v)
    {
    #ifdef USE_LZCNT
        if(v == 0) return 64;
        uint32_t lzeros = _lzcnt_u64(v);
        return 63 - lzeros;
    #else
        if(v == 0) return 64;
        uint32_t lzeros = __builtin_clzll(v);
        return 63 - lzeros;
    #endif
    }

    // Returns the minimum number of bits required to represent v
    // Returns 0 if v is 0
    inline uint32_t MINREP(uint64_t v)
    {
    #ifdef USE_LZCNT
        uint32_t lzeros = _lzcnt_u64(v);
        return 64 - lzeros;
    #else
        // Return 64 if v is 0 to replicate the behavior of _tzcnt_u64 on zero input
        uint32_t lzeros = v ? __builtin_clzll(v) : 64;
        return 64 - lzeros;
    #endif
    }

    inline uint64_t PEXT(uint64_t v, uint64_t mask)
    {
    #if defined(USE_BMI2)
        return _pext_u64(v, mask);
    #else
        // This is very inefficient compared to the hardware PEXT instruction
        // if BMI2 better to use the workarounds to avoid using it
        // like how it is done in bitboard lookup tables
        uint64_t result = 0;
        uint64_t bit = 1;
        while (mask)
        {
            uint64_t lsb = mask & (~mask + 1);
            if (v & lsb)
                result |= bit;
            mask &= mask - 1;
            bit <<= 1;
        }
        return result;
    #endif
    }

    inline uint64_t ROTL(uint64_t v, uint8_t shift)
    {
    #if defined(__x86_64__)
        #ifdef __linux__
            return _lrotl(v, shift);
        #else
            return _rotl64(v, shift);
        #endif
    #else
        return (v << shift) | (v >> (64 - shift));
    #endif
    }

    inline void ENABLE_FTZ_AND_DAZ()
    {
        #if defined(__x86_64__)
        INFO("Enabling flush denormals to zero (FTZ) and denormals are zero (DAZ)")
        // FTZ (bit 15) and DAZ (bit 6) in MXCSR avoid denormal slowdowns.
        uint32_t mxcsr = _mm_getcsr();
        mxcsr |= (1u << 15); // FTZ
        mxcsr |= (1u << 6);  // DAZ
        _mm_setcsr(mxcsr);
        #endif
    }

    inline void DISABLE_FTZ_AND_DAZ()
    {
        #if defined(__x86_64__)
        INFO("Disabling flush denormals to zero (FTZ) and denormals are zero (DAZ)")
        uint32_t mxcsr = _mm_getcsr();
        mxcsr &= ~(1u << 15); // FTZ
        mxcsr &= ~(1u << 6);  // DAZ
        _mm_setcsr(mxcsr);
        #endif
    }
}