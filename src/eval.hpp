#pragma once
#include <board.hpp>
#include <nnue/nnue.hpp>

namespace Arcanum
{
    typedef int16_t eval_t;
    typedef struct EvalTrace
    {
        #ifdef FULL_TRACE
        uint8_t phase;
        eval_t mobility;
        eval_t material;
        eval_t pawns;
        eval_t king;
        eval_t center;
        #endif // FULL_TRACE
        eval_t total;


        EvalTrace(eval_t eval)
        {
        #ifdef FULL_TRACE
            phase = 0;
            mobility = 0;
            material = 0;
            pawns = 0;
            king = 0;
            center = 0;
        #endif // FULL_TRACE
            total = eval;
        }
        EvalTrace() : EvalTrace(0) {};

        bool operator> (const EvalTrace&) const;
        bool operator>=(const EvalTrace&) const;
        bool operator==(const EvalTrace&) const;
        bool operator<=(const EvalTrace&) const;
        bool operator< (const EvalTrace&) const;

        EvalTrace operator-()
        {
            EvalTrace eval;
            #ifdef FULL_TRACE
            eval.phase    = phase;
            eval.mobility = -mobility;
            eval.material = -material;
            eval.pawns    = -pawns;
            eval.king     = -king;
            eval.center   = -center;
            #endif // FULL_TRACE
            eval.total    = -total;
            return eval;
        }

        std::string toString() const
        {
            std::stringstream ss;
            #ifdef FULL_TRACE
            ss << "Mobility: " << mobility  << std::endl;
            ss << "Material: " << material  << std::endl;
            ss << "Pawns   : " << pawns     << std::endl;
            ss << "King    : " << king      << std::endl;
            ss << "Center  : " << center    << std::endl;
            #endif // FULL_TRACE
            ss << "Total   : " << total;
            return ss.str();
        }

        friend inline std::ostream& operator<<(std::ostream& os, const EvalTrace& trace)
        {
            #ifdef FULL_TRACE
            os << "Mobility: " << trace.mobility << std::endl;
            os << "Material: " << trace.material << std::endl;
            os << "Pawns   : " << trace.pawns    << std::endl;
            os << "King    : " << trace.king     << std::endl;
            os << "Center  : " << trace.center   << std::endl;
            #endif // FULL_TRACE
            os << "Total   : " << trace.total;
            return os;
        }
    } EvalTrace;

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

            EvalEntry m_pawnEvalTable[Evaluator::pawnTableSize];
            EvalEntry m_materialEvalTable[Evaluator::materialTableSize];
            EvalEntry m_shelterEvalTable[Evaluator::shelterTableSize];
            PhaseEntry m_phaseTable[Evaluator::phaseTableSize];

            void m_initEval(const Board& board);
            uint8_t m_getPhase(const Board& board, EvalTrace& eval);
            eval_t m_getPawnEval(const Board& board, uint8_t phase, EvalTrace& eval);
            eval_t m_getMaterialEval(const Board& board, uint8_t phase, EvalTrace& eval);
            eval_t m_getMobilityEval(const Board& board, uint8_t phase, EvalTrace& eval);
            eval_t m_getKingEval(const Board& board, uint8_t phase, EvalTrace& eval);
            eval_t m_getCenterEval(const Board& board, uint8_t phase, EvalTrace& eval);
            template <typename Arcanum::Color turn>
            eval_t m_getShelterEval(const Board& board, uint8_t square);
        public:
            Evaluator();
            ~Evaluator();
            void setEnableNNUE(bool enabled);
            void initializeAccumulatorStack(const Board& board);
            void pushMoveToAccumulator(const Board& board, const Move& move);
            void popMoveFromAccumulator();
            EvalTrace evaluate(Board& board, uint8_t plyFromRoot);
            EvalTrace getDrawValue(Board& board, uint8_t plyFromRoot);
            static bool isCheckMateScore(EvalTrace eval);
    };
}