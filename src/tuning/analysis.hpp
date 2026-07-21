#pragma once

#include <string>

namespace Arcanum
{
    class Analyser
    {
        private:
            static constexpr uint32_t EvalIndices = UINT16_MAX + 1;
            uint32_t m_analysedPositions;
            uint32_t m_checkedPositions;
            uint32_t m_piecePositions[2][6][64]; // [opponent][pieceType][square]
            uint32_t m_pieceCounts[33]; // [NumPieces]
            uint32_t m_rule50Counts[101]; // [HalfMoveClock]
            uint32_t* m_evaluations; // [EvalIndex]

            void m_analysePosition(Board* board);
            void m_analyseEvaluation(eval_t eval);
            uint16_t m_evalToIndex(eval_t eval) const;
            eval_t m_indexToEval(uint16_t index) const;
            std::string m_getPiecePositions(bool opponent, Piece pieceType) const;
        public:
            Analyser();
            ~Analyser();
            void clear();
            void analyseDataset(std::string inputPath);
            void printResults();
    };
}