#pragma once
#include <board.hpp>

#define KILLER_MOVES_MAX_PLY 64

namespace Arcanum
{
    class KillerMoveManager
    {
        private:
            // Maximum search depth (ply) for 64 for killer moves
            // 2 killer moves per ply
            Move m_killerMoves[KILLER_MOVES_MAX_PLY][2];
        public:
            KillerMoveManager();
            void add(Move move, uint8_t plyFromRoot);
            bool contains(Move move, uint8_t plyFromRoot) const;
            void clear();
    };

    class RelativeHistory
    {
        private:
            uint32_t m_hhScores[2][64][64]; // History: Count the number of times the move did cause a Beta-cut
            uint32_t m_bfScores[2][64][64]; // Butterfly: Count the number of times the move did not cause a Beta-cut
        public:
            RelativeHistory();
            void addHistory(const Move& move, uint8_t depth, Color turn);
            void addButterfly(const Move& move, uint8_t depth, Color turn);
            uint32_t get(const Move& move, Color turn);
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
            RelativeHistory* m_relativeHistory;
            uint8_t m_numMoves;
            uint8_t m_numLowScoreMoves;
            uint8_t m_numHighScoreMoves;
            Move m_ttMove;
            bitboard_t m_bbOpponentPawnAttacks;
            bitboard_t m_bbOpponentAttacks;
            ScoreIndex m_highScoreIdxPairs[218];
            ScoreIndex m_lowScoreIdxPairs[218];
            int32_t m_getMoveScore(const Move& move);
            void m_scoreMoves();
        public:
            MoveSelector(const Move *moves, const uint8_t numMoves, int plyFromRoot, KillerMoveManager* killerMoveManager, RelativeHistory* relativeHistory, Board *board, const Move ttMove = Move(0,0));
            const Move* getNextMove();
    };
}