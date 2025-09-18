#pragma once

#include <types.hpp>
#include <move.hpp>

namespace Arcanum
{
    class KillerMoveManager
    {
        private:
            static constexpr uint8_t KillerMoveMaxPly = 96;
            static constexpr uint8_t NumKillersPerPly = 2;
            static constexpr uint32_t TableSize = NumKillersPerPly*KillerMoveMaxPly;
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
}