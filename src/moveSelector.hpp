#include <board.hpp>

namespace ChessEngine2
{
    class MoveSelector
    {
        private:
            const Move* m_moves;
            uint8_t m_numMoves;
            std::pair<int32_t, uint8_t> m_scoreIdxPairs[218];
            int32_t m_getMoveScore(Move move);
            void m_scoreMoves();
        public:
            MoveSelector(const Move *moves, const uint8_t numMoves);
            const Move* getNextMove();
    };
}