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
            static constexpr uint32_t TableSize = 2*64*64;
            //  [MovedColor][MovedFrom][MovedTo][CapturedPiece]
            int32_t* m_historyScore; // History: Count the number of times the move did cause a Beta-cut
            uint32_t m_getIndex(Color turn, square_t from, square_t to);
            int32_t m_getBonus(uint8_t depth);
            void m_addBonus(const Move& move, Color turn, int32_t bonus);
        public:
            History();
            ~History();
            void updateHistory(const Move& bestMove, const std::array<Move, MAX_MOVE_COUNT>& quiets, uint8_t numQuiets, uint8_t depth, Color turn);
            int32_t get(const Move& move, Color turn);
            void clear();
    };

    class CaptureHistory
    {
        private:
            static constexpr uint32_t TableSize = 2 * 64 * 64 * 6;
            //  [MovedColor][MovedFrom][MovedTo][CapturedPiece]
            int32_t* m_historyScore;
            uint32_t m_getIndex(Color turn, square_t from, square_t to, Piece capture);
            int32_t m_getBonus(uint8_t depth);
            void m_addBonus(const Move& move, Color turn, int32_t bonus);
        public:
            CaptureHistory();
            ~CaptureHistory();
            void updateHistory(const Move& bestMove, const std::array<Move, MAX_MOVE_COUNT>& captures, uint8_t numCaptures, uint8_t depth, Color turn);
            int32_t get(const Move& move, Color turn);
            void clear();
    };

    class CounterMoveManager
    {
        private:
            Move m_counterMoves[2][64][64];
        public:
            CounterMoveManager();
            void setCounter(const Move& counterMove, const Move& prevMove, Color turn);
            bool contains(const Move& move, const Move& prevMove, Color turn);
            void clear();
    };

    class MoveSelector
    {
        public:
            enum Phase : int8_t
            {
                TT_PHASE = -1,
                HIGH_SCORING_PHASE,
                LOW_SCORING_PHASE,
                NEGATIVE_SCORING_PHASE,
                NUM_PHASES,
            };
            MoveSelector(
                const Move *moves,
                const uint8_t numMoves,
                int plyFromRoot,
                KillerMoveManager* killerMoveManager,
                History* history,
                CaptureHistory* captureHistory,
                CounterMoveManager* counterMoveManager,
                Board *board,
                const Move ttMove,
                const Move prevMove
            );
            const Move* getNextMove();
            Phase getPhase() const;
        private:
            struct ScoreIndex
            {
                int32_t score;
                int32_t index;
            };
            Phase m_phase;
            const Move* m_moves;
            int m_plyFromRoot;
            Board* m_board;
            KillerMoveManager* m_killerMoveManager;
            History* m_history;
            CaptureHistory* m_captureHistory;
            CounterMoveManager* m_counterMoveManager;
            Move m_ttMove;
            Move m_prevMove;
            bool m_sortRequired;
            uint8_t m_numMoves;
            uint8_t m_numMovesInPhase[NUM_PHASES];                    // Not including TT
            ScoreIndex m_scoreIndexPairs[NUM_PHASES][MAX_MOVE_COUNT]; // Not including TT
            int32_t m_getMoveScore(const Move& move, Phase& phase);
            void m_scoreMoves();
    };

}