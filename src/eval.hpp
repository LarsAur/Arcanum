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

            eval_t m_pawnValue          = 100;
            eval_t m_rookValue          = 481;
            eval_t m_knightValue        = 311;
            eval_t m_bishopValue        = 306;
            eval_t m_queenValue         = 919;
            eval_t m_doublePawnScore    = -27;
            eval_t m_pawnSupportScore   = 1;
            eval_t m_pawnBackwardScore  = -5;
            eval_t m_mobilityBonusKnightBegin[9]  = {-43, -34, 6, 16, 21, 22, 28, 35, 33};
            eval_t m_mobilityBonusKnightEnd[9]    = {-61, -42, -12, 2, 14, 32, 31, 30, 20};
            eval_t m_mobilityBonusBishopBegin[14] = {-28, -1, 26, 33, 35, 49, 58, 71, 76, 74, 76, 72, 74, 95};
            eval_t m_mobilityBonusBishopEnd[14]   = {-40, -6, 11, 30, 30, 38, 42, 52, 59, 74, 61, 72, 69, 87};
            eval_t m_mobilityBonusRookBegin[15]   = {-41, -5, -9, 9, -8, -3, 9, 17, 28, 30, 41, 34, 42, 46, 49};
            eval_t m_mobilityBonusRookEnd[15]     = {-63, 4, 36, 59, 89, 107, 119, 119, 135, 145, 146, 143, 152, 152, 156};
            eval_t m_mobilityBonusQueenBegin[28]  = {-10, 3, -7, 11, 23, 34, 30, 27, 34, 52, 46, 55, 53, 51, 78, 86, 86, 90, 88, 89, 105, 95, 99, 93, 100, 100, 121, 111};
            eval_t m_mobilityBonusQueenEnd[28]    = {-34, -11, 7, 36, 53, 71, 58, 84, 93, 111, 114, 115, 139, 140, 151, 150, 153, 159, 163, 163, 166, 154, 154, 152, 159, 167, 191, 202};
            eval_t m_pawnRankBonusBegin[8]        = {0, -4, 2, 13, 5, 22, 16, 0};
            eval_t m_pawnRankBonusEnd[8]          = {0, 10, 3, 9, 31, 49, 128, 0};
            eval_t m_passedPawnRankBonusBegin[8]  = {0, -3, 6, 10, 44, 67, 135, 0};
            eval_t m_passedPawnRankBonusEnd[8]    = {0, 11, 31, 54, 88, 129, 208, 0};
            eval_t m_kingAreaAttackScore[50]      = {19, 0, 0, 14, 3, 11, -11, -1, -7, 26, 5, 3, 12, 11, 16, 20, 25, 38, 37, 55, 49, 87, 65, 66, 96, 78, 86, 94, 141, 112, 121, 169, 150, 199, 210, 221, 232, 244, 218, 267, 279, 253, 283, 295, 307, 333, 330, 342, 354, 373};
            eval_t m_whiteKingPositionBegin[64]   = {8, 6, 44, 18, -7, 26, 28, 1, -11, -8, -5, 7, 3, 5, -3, -11, -2, -6, 4, 6, 6, 6, 5, -5, 2, 6, -12, -21, -13, -10, 5, 4, 5, 5, 0, -28, -24, -1, 7, 3, -2, -1, -13, -10, -27, -5, 7, 6, 0, 5, 5, 7, 7, 0, 6, 6, 6, 6, 7, 6, -11, 1, 3, -1};
            eval_t m_kingPositionEnd[64]          = {-45, -33, -14, -18, -6, -20, -41, -59, -44, -21, -11, -4, 3, 6, -6, -21, -30, -12, -4, 1, -6, -1, 7, -22, -6, 6, 8, -7, -6, 8, -1, -6, -6, 7, 16, -7, 5, -2, -4, -11, -14, -1, -1, 10, -6, 6, 0, -19, -6, -6, -6, 7, -13, -14, -13, -18, -57, -31, -15, -7, -11, -23, -37, -69};
            eval_t m_pawnShelterScores[4][8]      = {{16, 21, 26, 11, 15, 29, 22, 0}, {-7, 17, -4, -32, -10, 9, 14, 0}, {9, 18, 11, -7, -9, 18, -6, 0}, {-5, 14, 7, -9, -21, -29, -24, 0}};
            eval_t m_centerControlScoreBegin[16]  = {0, 10, 20, 30, 55, 54, 48, 77, 64, 108, 119, 125, 139, 140, 136, 132};

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
            static std::string s_hceWeightsFile;
            void setEnableNNUE(bool enabled);
            void setHCEModelFile(std::string path);
            void loadWeights(eval_t* weights);
            void initializeAccumulatorStack(const Board& board);
            void pushMoveToAccumulator(const Board& board, const Move& move);
            void popMoveFromAccumulator();
            EvalTrace evaluate(Board& board, uint8_t plyFromRoot);
            EvalTrace getDrawValue(Board& board, uint8_t plyFromRoot);
            static bool isCheckMateScore(EvalTrace eval);
    };
}