#pragma once
#include <board.hpp>
#include <search.hpp>

namespace Arcanum
{
    class UCI
    {
        private:

        public:
        static void loop();
        static void go(Board& board, Searcher& searcher, std::istringstream& is);
        static void setoption(std::istringstream& is);
        static void position(Board& board, std::istringstream& is);
        static void ischeckmate(Board& board);
        static void eval(Board& board);
    };
}