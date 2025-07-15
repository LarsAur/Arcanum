#pragma once

#include <string>
#include <memory>
#include <vector>
#include <board.hpp>
#include <types.hpp>

namespace Arcanum
{
    // Virtual class to represent a parser to read chess games
    class DataParser
    {
        public:

            virtual ~DataParser() = default;
            virtual bool open(std::string path) = 0;
            virtual void close() = 0;
            virtual bool eof() = 0;
            virtual Board* getNextBoard() = 0;
            virtual Move getMove() = 0;
            virtual eval_t getScore() = 0;
            virtual GameResult getResult() = 0;
    };

    // Virtual class to represent an encoder to store chess games
    class DataEncoder
    {
        public:
        virtual ~DataEncoder() = default;
        virtual bool open(std::string path) = 0;
        virtual void close() = 0;
        virtual void addPosition(
            const Board& board,
            const Move& move,
            eval_t score,
            GameResult result
        ) = 0;
        virtual void addGame(
            const Board& startBoard,
            std::vector<Move>& moves,
            std::vector<eval_t>& scores,
            GameResult result
        ) = 0;
    };

    class DataLoader
    {
        private:
            std::unique_ptr<DataParser> m_parser;
        public:
            DataLoader();
            bool open(std::string path);
            void close();
            bool eof();
            Board* getNextBoard();
            Move getMove();
            eval_t getScore();
            GameResult getResult();
    };

    class DataStorer
    {
        private:
            std::unique_ptr<DataEncoder> m_encoder;
        public:
            DataStorer();
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
                std::vector<Move>& moves,
                std::vector<eval_t>& scores,
                GameResult result
            );
    };
}