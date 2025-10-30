#pragma once
#include <types.hpp>
#include <board.hpp>

namespace Arcanum
{
    class CorrectionHistory
    {
        private:
            constexpr static int32_t BonusLimit = 4096;
            constexpr static int32_t CorrectionLimit = 16384;
            constexpr static size_t PawnTableSize = 2 * 8192;
            // [turn][hashIndex]
            int16_t* m_pawnCorrections;

            uint32_t m_getPawnIndex(hash_t pawnHash, Color turn) const;
        public:
            CorrectionHistory();
            ~CorrectionHistory();
            void update(const Board& board, eval_t bestScore, eval_t staticEval, uint8_t depth);
            eval_t get(const Board& board) const;
            void clear();
    };
}