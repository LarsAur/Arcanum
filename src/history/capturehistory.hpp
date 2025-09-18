#pragma once

#include <types.hpp>
#include <move.hpp>

namespace Arcanum
{
    class CaptureHistory
    {
        private:
            static constexpr uint32_t TableSize = 2 * 64 * 6 * 6;
            //  [MovedColor][MovedTo][MovedPiece][CapturedPiece]
            int32_t* m_historyScore;
            uint32_t m_getIndex(Color turn, square_t to, Piece movedPiece, Piece capturedPiece);
            int32_t m_getBonus(uint8_t depth);
            void m_addBonus(const Move& move, Color turn, int32_t bonus);
        public:
            CaptureHistory();
            ~CaptureHistory();
            void updateHistory(const Move& bestMove, const Move* captures, uint8_t numCaptures, uint8_t depth, Color turn);
            int32_t get(const Move& move, Color turn);
            void clear();
    };
}