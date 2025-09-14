#pragma once
#include <board.hpp>
#include <array>

#define KILLER_MOVES_MAX_PLY 96

namespace Arcanum
{
    class KillerMoveManager
    {
        private:
            static constexpr uint32_t TableSize = 2*KILLER_MOVES_MAX_PLY;
            //  [PlyFromRoot][offset]
            Move* m_killerMoves;
            uint32_t m_getIndex(uint8_t plyFromRoot, uint8_t offset) const;
        public:
            KillerMoveManager();
            ~KillerMoveManager();
            void add(Move move, uint8_t plyFromRoot);
            bool contains(Move move, uint8_t plyFromRoot) const;
            void clearPly(uint8_t plyFromRoot);
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
            void updateHistory(const Move& bestMove, const Move* quiets, uint8_t numQuiets, uint8_t depth, Color turn);
            int32_t get(const Move& move, Color turn);
            void clear();
    };

    class CaptureHistory
    {
        private:
            static constexpr uint32_t TableSize = 2 * 64 * 6 * 6;
            //  [MovedColor][MovedTo][MovedPiece][CapturedPiece]
            int32_t* m_historyScore;
            uint32_t m_getIndex(Color turn, square_t to, Piece movedPiece, Piece capturedPiece);
            int32_t m_getBonus(uint8_t depth);
            void m_addBonus(const Move& move, Color turn, int32_t bonus);
        public:
            CaptureHistory();
            ~CaptureHistory();
            void updateHistory(const Move& bestMove, const Move* captures, uint8_t numCaptures, uint8_t depth, Color turn);
            int32_t get(const Move& move, Color turn);
            void clear();
    };

    class CounterMoveManager
    {
        private:
            static constexpr uint32_t TableSize = 2*64*64;
            // [turn][prevMoveFrom][prevMoveTo]
            Move* m_counterMoves;
            uint32_t m_getIndex(Color turn, square_t prevFrom, square_t prevTo);
        public:
            CounterMoveManager();
            ~CounterMoveManager();
            void setCounter(const Move& counterMove, const Move& prevMove, Color turn);
            bool contains(const Move& move, const Move& prevMove, Color turn);
            void clear();
    };

    class MoveSelector
    {
        public:
            enum class Phase : uint8_t
            {
                TT_PHASE,
                GOOD_CAPTURES_PHASE,
                KILLERS_PHASE,
                COUNTERS_PHASE,
                QUIETS_PHASE,
                BAD_CAPTURES_PHASE,
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
            void skipQuiets();
            bool isSkippingQuiets();
            uint8_t getNumQuietsLeft();

            private:
            struct MoveAndScore
            {
                const Move* move;
                int32_t score;
            };

            static MoveAndScore popBestMoveAndScore(MoveAndScore* list, uint8_t numElements);

            Phase m_phase;
            const Move* m_moves;
            int m_plyFromRoot;
            Board* m_board;
            KillerMoveManager* m_killerMoveManager;
            History* m_history;
            CaptureHistory* m_captureHistory;
            CounterMoveManager* m_counterMoveManager;
            Move m_moveFromTT;
            Move m_prevMove;
            uint8_t m_numMoves;

            bool m_skipQuiets;
            uint8_t m_numKillers;
            uint8_t m_numCaptures;
            uint8_t m_numBadCaptures;
            uint8_t m_nextBadCapture;
            uint8_t m_numQuiets;

            const Move* m_ttMove;
            const Move* m_killers[2];
            const Move* m_counter;

            MoveAndScore m_movesAndScores[MAX_MOVE_COUNT];
            MoveAndScore* m_captureMovesAndScores;
            MoveAndScore* m_badCaptureMovesAndScores;
            MoveAndScore* m_quietMovesAndScores;
            void m_scoreMoves();
    };

}