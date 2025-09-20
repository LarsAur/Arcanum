#pragma once
#include <board.hpp>
#include <array>
#include <history/killermanager.hpp>
#include <history/countermanager.hpp>
#include <history/quiethistory.hpp>
#include <history/capturehistory.hpp>
#include <history/continuationhistory.hpp>

namespace Arcanum
{
    struct MoveOrderHeuristics
    {
        QuietHistory quietHistory;
        CaptureHistory captureHistory;
        KillerManager killerManager;
        CounterManager counterManager;
        ContinuationHistory continuationHistory;

        void clear()
        {
            quietHistory.clear();
            captureHistory.clear();
            killerManager.clear();
            counterManager.clear();
            continuationHistory.clear();
        }
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
                MoveOrderHeuristics* heuristics,
                Board *board,
                const Move ttMove,
                const Move* moveStack
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
            const Move* m_moveStack;
            int m_plyFromRoot;
            Board* m_board;
            MoveOrderHeuristics* m_heuristics;
            Move m_moveFromTT;
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