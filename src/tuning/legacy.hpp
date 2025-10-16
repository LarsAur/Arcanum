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

    class LegacyEncoder : public DataEncoder
    {
        private:
            std::ofstream m_ofs;
        public:
            LegacyEncoder();
            bool open(std::string path);
            void close();
            void addPosition(
                const Board& board,
                const Move& move,
                eval_t score,
                GameResult result
            );
            void addGame(
                const Board& startBoard,
                const std::vector<Move>& moves,
                const std::vector<eval_t>& scores,
                GameResult result
            );
    };
}