#pragma once

#include <types.hpp>
#include <move.hpp>

namespace Arcanum
{
    class CounterMoveManager
    {
        private:
            static constexpr uint32_t TableSize = 2*64*64;
            // [turn][prevMoveFrom][prevMoveTo]
            Move* m_counterMoves;
            uint32_t m_getIndex(Color turn, square_t prevFrom, square_t prevTo);
        public:
            CounterMoveManager();
            ~CounterMoveManager();
            void setCounter(const Move& counterMove, const Move& prevMove, Color turn);
            bool contains(const Move& move, const Move& prevMove, Color turn);
            void clear();
    };
}