#pragma once

#include <types.hpp>
#include <board.hpp>
#include <nnue.hpp>

namespace Arcanum
{
    class Evaluator
    {
        private:
            struct AccumulatorUpdateInfo
            {
                bool updated[2];
                NNUE::DeltaFeatures deltaFeatures;
            };

            uint32_t m_accumulatorStackIndex;
            std::vector<NNUE::Accumulator*> m_accumulatorStack;
            std::vector<AccumulatorUpdateInfo> m_accumulatorUpdates;

            void m_propagateAccumulatorUpdates(Color perspective);
        public:
            constexpr static eval_t MateScore = INT16_MAX;
            constexpr static eval_t MaxMateDistance = 256;
            constexpr static eval_t TbMateScore = MateScore - MaxMateDistance;
            constexpr static eval_t TbMaxMateDistance = 256;

            // Returns true if the score is a mate score not from the TB
            static bool isRealMateScore(eval_t eval);
            // Returns true if the score is a mate score from the TB
            static bool isTbMateScore(eval_t eval);
            // Returns true if the score is a mate score
            static bool isMateScore(eval_t eval);
            // Returns true if the score is a winning TB or mate score
            static bool isWinningScore(eval_t eval);
            // Returns true if the score is a losing TB or mate score
            static bool isLosingScore(eval_t eval);
            // Returns true if the board and score indicates that it might be close to mate
            static bool isCloseToMate(Board& board, eval_t eval);
            // Clamps the eval to avoid mate scores
            static eval_t clampEval(eval_t eval);
            // Gets the distance to mate from a real mate score
            static int32_t getMateDistance(eval_t eval);

            static NNUE nnue;

            Evaluator();
            ~Evaluator();
            eval_t evaluate(Board& board, uint8_t plyFromRoot);

            void initAccumulatorStack(const Board& board);
            void pushMoveToAccumulator(const Board& board, const Move& move);
            void popMoveFromAccumulator();
    };
}