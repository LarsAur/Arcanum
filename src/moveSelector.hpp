#include <board.hpp>

namespace ChessEngine2
{
    class MoveSelector
    {
        private:
            const Move* m_moves;
            Board* m_board;
            uint8_t m_numMoves;
            Move m_ttMove;
            bitboard_t m_bbOpponentPawnAttacks;
            bitboard_t m_bbOpponentAttacks;
            std::pair<int32_t, uint8_t> m_scoreIdxPairs[218];
            int32_t m_getMoveScore(Move move);
            void m_scoreMoves();
        public:
            MoveSelector(const Move *moves, const uint8_t numMoves, Board *board, const Move ttMove = Move(0,0));
            const Move* getNextMove();
    };
}