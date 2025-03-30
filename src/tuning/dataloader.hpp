#pragma once

#include <string>
#include <memory>
#include <board.hpp>

namespace Arcanum
{
    enum GameResult
    {
        BLACK_WIN = -1,
        DRAW = 0,
        WHITE_WIN = 1,
    };

    // Virtual class to represent a parser to read chess games
    class DataParser
    {
        public:

            virtual ~DataParser() = default;
            virtual bool open(std::string path) = 0;
            virtual void close() = 0;
            virtual bool eof() = 0;
            virtual Board* getNextBoard() = 0;
            virtual eval_t getScore() = 0;
            virtual GameResult getResult() = 0;
    };

    // Virtual class to represent an encoder to store chess games
    class DataEncoder
    {
        public:
        static constexpr uint32_t MaxGameLength = 300;
        virtual ~DataEncoder() = default;
        virtual bool open(std::string path) = 0;
        virtual void close() = 0;
        virtual void addGame(
            std::string startfen,
            std::array<Move, DataEncoder::MaxGameLength>& moves,
            std::array<eval_t, DataEncoder::MaxGameLength>& scores,
            uint32_t numMoves,
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
            eval_t getScore();
            GameResult getResult();
    };
}