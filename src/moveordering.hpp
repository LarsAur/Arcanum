#pragma once
#include <board.hpp>
#include <array>

#define KILLER_MOVES_MAX_PLY 96

namespace Arcanum
{
    class KillerMoveManager
    {
        private:
            Move m_killerMoves[KILLER_MOVES_MAX_PLY][2];
        public:
            KillerMoveManager();
            void add(Move move, uint8_t plyFromRoot);
            bool contains(Move move, uint8_t plyFromRoot) const;
            void clear();
    };

    class History
    {
        private:
            int32_t m_historyScore[2][64][64]; // History: Count the number of times the move did cause a Beta-cut
            int32_t m_getBonus(uint8_t depth);
            void m_addBonus(const Move& move, Color turn, int32_t bonus);
        public:
            History();
            void updateHistory(const Move& bestMove, const std::array<Move, MAX_MOVE_COUNT>& quiets, uint8_t numQuiets, uint8_t depth, Color turn);
            int32_t get(const Move& move, Color turn);
            void clear();
    };

    class MoveSelector
    {
        private:
            struct ScoreIndex
            {
                int32_t score;
                int32_t index;
            };
            const Move* m_moves;
            int m_plyFromRoot;
            Board* m_board;
            KillerMoveManager* m_killerMoveManager;
            History* m_history;
            uint8_t m_numMoves;
            uint8_t m_numLowScoreMoves;
            uint8_t m_numHighScoreMoves;
            Move m_ttMove;
            bitboard_t m_bbOpponentPawnAttacks;
            bitboard_t m_bbOpponentAttacks;
            ScoreIndex m_highScoreIdxPairs[MAX_MOVE_COUNT];
            ScoreIndex m_lowScoreIdxPairs[MAX_MOVE_COUNT];
            int32_t m_getMoveScore(const Move& move);
            void m_scoreMoves();
        public:
            MoveSelector(const Move *moves, const uint8_t numMoves, int plyFromRoot, KillerMoveManager* killerMoveManager, History* relativeHistory, Board *board, const Move ttMove = NULL_MOVE);
            const Move* getNextMove();
    };
}