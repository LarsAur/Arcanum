#pragma once

#include <board.hpp>
#include <string>
#include <istream>

namespace Arcanum
{
    // https://www.chessprogramming.org/Extended_Position_Description
    struct EDP
    {
        std::string fen;      // FEN string
        int32_t acd;          // acd analysis count depth
        int32_t acn;          // acn analysis count nodes
        int32_t acs;          // acs analysis count seconds
        std::vector<Move> am; // am avoid move(s)
        std::vector<Move> bm; // bm best move(s)
        std::string c[10];    // c0 comment (primary, also c1 though c9)
        eval_t ce;            // ce centipawn evaluation
        int32_t dm;           // dm direct mate fullmove count
        std::string eco;      // eco Encyclopedia of Chess Openings opening code
        int32_t fmvn;         // fmvn fullmove number
        int32_t hmvc;         // hmvc halfmove clock
        std::string id;       // id position identification
        std::string nic;      // nic New In Chess opening code
        Move pm;              // pm predicted move
        std::vector<Move> pv; // pv predicted variation
        int32_t rc;           // rc repetition count
        Move sm;              // sm supplied move
        std::string v[10];    // v0 variation name (primary, also v1 though v9)
    };

    class FEN
    {
        private:
        static bool m_consumeExpectedSpace(std::istringstream& is);
        static bool m_setPosition(Board& board, std::istringstream& is);
        static bool m_setTurn(Board& board, std::istringstream& is);
        static bool m_setCastleRights(Board& board, std::istringstream& is, bool strict);
        static bool m_setEnpassantTarget(Board& board, std::istringstream& is);
        static bool m_setHalfmoveClock(Board& board, std::istringstream& is);
        static bool m_setFullmoveClock(Board& board, std::istringstream& is);
        public:
        static constexpr const char* startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        static bool setFEN(Board& board, const std::string fen, bool strict = true);
        static std::string getFEN(const Board& board);
        static std::string toString(const Board& board);
        static EDP parseEDP(std::string edp);
    };
};
