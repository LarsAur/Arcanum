#pragma once

#include <types.hpp>
#include <move.hpp>

namespace Arcanum
{
    class ContinuationHistory
    {
        private:
            static constexpr uint32_t TableSize = 2*6*64*6*64;
            // [turn][prevPiece][prevTo][movePiece][moveTo]
            int32_t* m_scores;
            uint32_t m_getIndex(Color turn, Piece prevPiece, square_t prevTo, Piece movePiece, square_t moveTo);
            void m_addBonus(const Move& move, const Move& prevMove, Color turn, int32_t bonus);
            int32_t m_getScore(const Move& move, const Move& prevMove, Color turn);
            int32_t m_getBonus(uint8_t depth);
        public:
            ContinuationHistory();
            ~ContinuationHistory();
            void updateContinuation(const Move* moveStack, uint8_t plyFromRoot, const Move& move, const Move* quiets, uint8_t numQuiets, Color turn, uint8_t depth);
            int32_t getContinuationScore(const Move* moveStack, uint8_t plyFromRoot, const Move& move, Color turn);
            void clear();
    };
}