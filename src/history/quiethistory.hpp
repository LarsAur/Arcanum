#pragma once

#include <types.hpp>
#include <move.hpp>

namespace Arcanum
{
    class QuietHistory
    {
        private:
            static constexpr uint32_t TableSize = 2*64*64;
            //  [MovedColor][MovedFrom][MovedTo][CapturedPiece]
            int32_t* m_historyScore; // History: Count the number of times the move did cause a Beta-cut
            uint32_t m_getIndex(Color turn, square_t from, square_t to);
            int32_t m_getBonus(uint8_t depth);
            void m_addBonus(const Move& move, Color turn, int32_t bonus);
        public:
            QuietHistory();
            ~QuietHistory();
            void update(const Move& bestMove, const Move* quiets, uint8_t numQuiets, uint8_t depth, Color turn);
            int32_t get(const Move& move, Color turn);
            void clear();
    };
}