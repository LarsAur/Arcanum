#pragma once

#include <eval.hpp>
#include <types.hpp>

namespace Arcanum
{

    struct WDL
    {
        uint32_t win;
        uint32_t draw;
        uint32_t loss;
    };

    class WDLModel
    {
        private:
            static uint32_t m_getMaterialCount(const Board& board);
            static std::tuple<float, float> m_getWDLParameters(const Board& board);
        public:
            static eval_t getNormalizedScore(const Board& board, eval_t eval);
            static WDL getExpectedWDL(const Board& board, eval_t eval);
    };


}