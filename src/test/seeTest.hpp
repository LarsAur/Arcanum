#pragma once

#include <string>
#include <move.hpp>

namespace Arcanum::Benchmark
{
    class SeeTest
    {
        private:
            static bool m_testPosition(std::string fen, Move move, bool expected);
        public:
            static void runSeeTest();
    };
}