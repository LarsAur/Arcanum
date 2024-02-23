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
            static constexpr size_t pawnTableSize     = 1 << 13; // Required to be a power of 2
            static constexpr size_t shelterTableSize  = 1 << 13; // Required to be a power of 2

            static constexpr size_t pawnTableMask     = Evaluator::pawnTableSize     - 1;
            static constexpr size_t shelterTableMask  = Evaluator::shelterTableSize  - 1;

            typedef struct EvalEntry
            {
                hash_t hash;
                eval_t value;
            } EvalEntry;

            typedef struct ImmEvalEntry
            {
                eval_t early;
                eval_t late;
                uint8_t phase;
                ImmEvalEntry() {early = 0; late = 0; phase = 0;}
            } ImmEvalEntry;

            bool m_enabledNNUE;
            NN::NNUE m_nnue;
            uint32_t m_accumulatorStackPointer;
            std::vector<NN::Accumulator*> m_accumulatorStack;
            std::vector<ImmEvalEntry> m_immAccumulatorStack;

            // It is allocated the maximum number of the same piece which can occur
            bitboard_t m_pawnAttacks[Color::NUM_COLORS];
            bitboard_t m_rookMoves[Color::NUM_COLORS][10];
            bitboard_t m_knightMoves[Color::NUM_COLORS][10];
            bitboard_t m_bishopMoves[Color::NUM_COLORS][10];
            bitboard_t m_queenMoves[Color::NUM_COLORS][10];
            bitboard_t m_kingMoves[Color::NUM_COLORS];

            EvalEntry* m_pawnEvalTable;
            EvalEntry* m_shelterEvalTable;

            eval_t m_pieceValues[6];
            eval_t m_pieceSquareTablesEarly[6][32];
            eval_t m_pieceSquareTablesLate[6][32];

            // void m_initEval(const Board& board);
            void m_initImmediateEval(ImmEvalEntry& immEval, const Board& board);
            void m_incrementImmediateEval(ImmEvalEntry& prevImmEval, ImmEvalEntry& newImmEval, const Board& board, const Move& move);
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
            eval_t immEvaluation(Board& board, uint8_t plyFromRoot);
            static bool isCheckMateScore(eval_t eval);
            static bool isTbCheckMateScore(eval_t eval);
    };
}