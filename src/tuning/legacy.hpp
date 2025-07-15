#pragma once

#include <tuning/dataloader.hpp>
#include <fstream>
#include <string>
#include <board.hpp>

namespace Arcanum
{
    class LegacyParser : public DataParser
    {
        private:
            std::ifstream m_ifs;
            Board m_board;
            GameResult m_result;
            eval_t m_score;
        public:
            LegacyParser();
            bool open(std::string path);
            void close();
            bool eof();
            Board* getNextBoard();
            Move getMove();
            eval_t getScore();
            GameResult getResult();
    };
}