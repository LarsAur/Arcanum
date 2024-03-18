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
            NN::NNUE m_nnue;
            uint32_t m_accumulatorStackPointer;
            std::vector<NN::Accumulator*> m_accumulatorStack;

        public:
            static bool isCheckMateScore(EvalTrace eval);
            static bool isTbCheckMateScore(EvalTrace eval);

            Evaluator();
            ~Evaluator();
            EvalTrace evaluate(Board& board, uint8_t plyFromRoot, bool noMoves = false);

            void initAccumulatorStack(const Board& board);
            void pushMoveToAccumulator(const Board& board, const Move& move);
            void popMoveFromAccumulator();
    };
}