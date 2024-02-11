#pragma once

#include <types.hpp>
#include <board.hpp>
#include <nnue/nnue.hpp>

namespace Arcanum
{
    class Evaluator
    {
        private:
            static constexpr size_t pawnTableSize     = 1 << 13; // Required to be a power of 2
            static constexpr size_t materialTableSize = 1 << 13; // Required to be a power of 2
            static constexpr size_t shelterTableSize  = 1 << 13; // Required to be a power of 2
            static constexpr size_t phaseTableSize    = 1 << 13; // Required to be a power of 2

            static constexpr size_t pawnTableMask     = Evaluator::pawnTableSize     - 1;
            static constexpr size_t materialTableMask = Evaluator::materialTableSize - 1;
            static constexpr size_t shelterTableMask  = Evaluator::shelterTableSize  - 1;
            static constexpr size_t phaseTableMask    = Evaluator::phaseTableSize    - 1;

            typedef struct EvalEntry
            {
                hash_t hash;
                eval_t value;
            } EvalEntry;

            typedef struct PhaseEntry
            {
                hash_t hash;
                uint8_t value;
            } PhaseEntry;

            bool m_enabledNNUE;
            NN::NNUE m_nnue;
            uint32_t m_accumulatorStackPointer;
            std::vector<NN::Accumulator*> m_accumulatorStack;

            // It is allocated the maximum number of the same piece which can occur
            int8_t m_numPawns[Color::NUM_COLORS];
            int8_t m_numRooks[Color::NUM_COLORS];
            int8_t m_numKnights[Color::NUM_COLORS];
            int8_t m_numBishops[Color::NUM_COLORS];
            int8_t m_numQueens[Color::NUM_COLORS];
            bitboard_t m_pawnAttacks[Color::NUM_COLORS];
            bitboard_t m_rookMoves[Color::NUM_COLORS][10];
            bitboard_t m_knightMoves[Color::NUM_COLORS][10];
            bitboard_t m_bishopMoves[Color::NUM_COLORS][10];
            bitboard_t m_queenMoves[Color::NUM_COLORS][10];
            bitboard_t m_kingMoves[Color::NUM_COLORS];

            EvalEntry* m_pawnEvalTable;
            EvalEntry* m_materialEvalTable;
            EvalEntry* m_shelterEvalTable;
            PhaseEntry* m_phaseTable;

            eval_t m_pawnValue          = 100;
            eval_t m_rookValue          = 500;
            eval_t m_knightValue        = 300;
            eval_t m_bishopValue        = 300;
            eval_t m_queenValue         = 900;

            void m_initEval(const Board& board);
        public:
            Evaluator();
            ~Evaluator();
            static std::string s_hceWeightsFile;
            void setEnableNNUE(bool enabled);
            void setHCEModelFile(std::string path);
            void loadWeights(eval_t* weights);
            void initializeAccumulatorStack(const Board& board);
            void pushMoveToAccumulator(const Board& board, const Move& move);
            void popMoveFromAccumulator();
            eval_t evaluate(Board& board, uint8_t plyFromRoot, bool noMoves = false);
            static bool isCheckMateScore(eval_t eval);
    };
}