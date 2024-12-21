#pragma once

#include <string>
#include <memory>
#include <board.hpp>

namespace Arcanum
{
    class DataParser
    {
        public:
            enum Result
            {
                BLACK_WIN = -1,
                DRAW = 0,
                WHITE_WIN = 1,
            };

            virtual ~DataParser() = default;
            virtual bool open(std::string path) = 0;
            virtual void close() = 0;
            virtual bool eof() = 0;
            virtual Board* getNextBoard() = 0;
            virtual eval_t getScore() = 0;
            virtual Result getResult() = 0;
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
            DataParser::Result getResult();
    };
}

