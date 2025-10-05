#pragma once

#include <search.hpp>
#include <fen.hpp>

namespace Arcanum::Benchmark
{
    class EngineTest
    {
        private:
            static uint32_t m_successes;
            static uint32_t m_attempts;
            static void m_testPosition(EDP& edp, Searcher& searcher);
        public:
            static void runEngineTest();
    };
}