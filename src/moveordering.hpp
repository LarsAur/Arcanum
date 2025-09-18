#pragma once
#include <board.hpp>
#include <array>
#include <history/killermanager.hpp>
#include <history/countermanager.hpp>
#include <history/quiethistory.hpp>
#include <history/capturehistory.hpp>

namespace Arcanum
{
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
                QuietHistory* quietHistory,
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
            QuietHistory* m_quietHistory;
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