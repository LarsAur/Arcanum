#pragma once

#include <inttypes.h>

#define CACHE_LINE_SIZE 64

namespace Arcanum
{
    typedef uint64_t hash_t;
    typedef uint64_t bitboard_t;
    typedef int16_t eval_t;
    typedef uint8_t square_t;

    enum GameResult
    {
        BLACK_WIN = -1,
        DRAW = 0,
        WHITE_WIN = 1,
    };
}
