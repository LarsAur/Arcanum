#pragma once

#include <board.hpp>
#include <string>
#include <vector>
#include <fstream>

namespace Tuning
{
    struct PGNTag
    {
        std::string name;
        std::string value;
    };

    class PGNParser
    {
        private:
        std::ifstream* m_fstream;
        std::vector<PGNTag> m_tags;
        std::vector<std::string> m_pgnMoves;
        std::vector<Arcanum::Move> m_moves;
        float m_result;
        bool m_isWhitespace(char c);
        bool m_peekNextChar(char* c);
        bool m_consumeNextChar();
        std::string m_getNextToken();
        void m_parseTags();
        void m_parseMoves();
        void m_convertMovesToInternalFormat();
        bool m_isMatchingMove(std::string pgnMove, Arcanum::Move move);
        Arcanum::square_t m_getSquareIdx(std::string arithmetic);
        public:
        PGNParser(std::string path);
        ~PGNParser();
        std::vector<Arcanum::Move>& getMoves();
        float getResult();
    };
}