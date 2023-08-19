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
    };

    class MoveSelector
    {
        private:
            const Move* m_moves;
            int m_plyFromRoot;
            Board* m_board;
            KillerMoveManager* m_killerMoveManager;
            uint8_t m_numMoves;
            Move m_ttMove;
            bitboard_t m_bbOpponentPawnAttacks;
            bitboard_t m_bbOpponentAttacks;
            std::pair<int32_t, uint8_t> m_scoreIdxPairs[218];
            int32_t m_getMoveScore(Move move);
            void m_scoreMoves();
        public:
            MoveSelector(const Move *moves, const uint8_t numMoves, int plyFromRoot, KillerMoveManager* killerMoveManager, Board *board, const Move ttMove = Move(0,0));
            const Move* getNextMove();
    };
}