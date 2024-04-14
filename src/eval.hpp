#pragma once

#include <types.hpp>
#include <board.hpp>
#include <nnue/nnue.hpp>

namespace Arcanum
{
    #define MATE_SCORE (INT16_MAX)
    #define MAX_MATE_DISTANCE (256)
    #define TB_MATE_SCORE (INT16_MAX - MAX_MATE_DISTANCE)
    #define TB_MAX_MATE_DISTANCE (256)

    class Evaluator
    {
        private:
            uint32_t m_accumulatorStackPointer;
            std::vector<NN::Accumulator*> m_accumulatorStack;

        public:
            static bool isCheckMateScore(eval_t eval);
            static bool isTbCheckMateScore(eval_t eval);
            static NN::NNUE nnue;
            static const char* nnuePathDefault;

            Evaluator();
            ~Evaluator();
            eval_t evaluate(Board& board, uint8_t plyFromRoot, bool noMoves = false);

            void initAccumulatorStack(const Board& board);
            void pushMoveToAccumulator(const Board& board, const Move& move);
            void popMoveFromAccumulator();
    };
}