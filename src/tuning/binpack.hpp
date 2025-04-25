#pragma once

#include <tuning/dataloader.hpp>
#include <types.hpp>
#include <string>
#include <fstream>
#include <board.hpp>

namespace Arcanum
{
    enum class CompressedMoveType
    {
        NORMAL,
        PROMOTION,
        CASTLE,
        ENPASSANT,
    };

    // A parser for the binpack fileformat for storing chess positions for nnue training
    // Spec: https://github.com/official-stockfish/Stockfish/blob/tools/docs/binpack.md
    class BinpackParser : public DataParser
    {
        private:
        std::ifstream m_ifs;
        std::vector<char> m_buffer;
        uint32_t m_currentChuckSize;
        uint32_t m_numBytesRead;

        Board m_currentBoard;
        int16_t m_currentScore;
        GameResult m_currentResult;
        Move m_currentMove;
        uint16_t m_currentMoveTextCount;

        uint8_t m_numBitsInBitBuffer;
        uint16_t m_bitBuffer; // Bits are stored in the MSBs of this uint16 buffer
        uint8_t m_getNextNBits(uint8_t numBits);
        int16_t m_unsignedToSigned(uint16_t u);
        uint16_t m_bigToLittleEndian(uint16_t b);
        square_t m_getNthSetBitIndex(bitboard_t bb, uint8_t n);
        uint8_t m_getMinRepBits(uint8_t value);
        void m_readBytesFromBuffer(void* dest, uint32_t numBytes);

        void m_parseBlock();
        void m_parseChain();
        void m_parseStem();
        void m_parsePos();
        void m_parseMove();
        void m_parseScore();
        void m_parsePlyAndResult();
        void m_parseRule50();
        void m_parseMovetextCount();
        void m_parseNextMoveAndScore();
        void m_parseVEncodedScore();

        public:
        BinpackParser();
        bool open(std::string path);
        void close();
        bool eof();
        Board* getNextBoard();
        Move getMove();
        eval_t getScore();
        GameResult getResult();
    };


    class BinpackEncoder : public DataEncoder
    {
        private:
            std::ofstream m_ofs;
            std::vector<char> m_buffer;

            uint16_t m_bitBuffer;
            uint8_t m_numBitsInBitBuffer;

            void m_writeBlock();
            void m_writeStem(Board& board, Move& move, eval_t score, GameResult result);
            void m_writePos(Board& board);
            void m_writeMove(Move& move);
            void m_writeScore(eval_t score);
            void m_writePlyAndResult(GameResult result, Color turn);
            void m_writeRule50(uint8_t rule50);
            void m_writeMovetextCount(uint32_t numMoves);
            void m_writeEncodedMove(Board& board, Move& move);
            void m_writeVEncodedScore(eval_t prevScore, eval_t currentScore);

            void m_writeNbits(uint8_t bits, uint8_t numBits);
            void m_flushBitbuffer();
            uint8_t m_getMinRepBits(uint8_t value);
            uint16_t m_littleToBigEndian(uint16_t little);
            uint16_t m_signedToUnsigned(int16_t s);
            void m_writeBytesToBuffer(void* src, uint32_t numBytes);
        public:
            BinpackEncoder();
            bool open(std::string path);
            void close();
            void addGame(
                std::string startfen,
                std::array<Move, DataEncoder::MaxGameLength>& moves,
                std::array<eval_t, DataEncoder::MaxGameLength>& scores,
                uint32_t numMoves,
                GameResult result
            );
    };
}