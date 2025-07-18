#pragma once

#include <types.hpp>
#include <eval.hpp>
#include <board.hpp>
#include <optional>

namespace Arcanum
{
    enum class TTFlag : uint8_t
    {
        EXACT = 0,
        LOWER_BOUND = 1,
        UPPER_BOUND = 2,
    };

    struct TTEntry
    {
        static constexpr uint8_t InvalidDepth = UINT8_MAX;

        // Note: For the hash, the LSB does not matter as much as hashes
        // placed in the same cluster will have the same LSBs of the hash
        // In fact, for the smallest non-zero TT (1MB) the 15 LSBs will match in each cluster
        static constexpr hash_t HashMask = 0xFFFFFFFFFFFFC000;
        static constexpr hash_t PvMask   = 0x1;
        static constexpr hash_t NumPiecesMask = 0b111110;

        uint8_t depth;
        uint8_t generation;
        eval_t eval;
        eval_t staticEval;
        uint16_t _flagAndPackedMove; // 2 bits = TT Flag | 14 bits = PackedMove info
        hash_t _hashNpAndIsPv; // 50 bits hash | 5 bits numPieces-2 | 1 bit isPv

        TTEntry(
            hash_t hash,
            Move move,
            eval_t eval,
            eval_t staticEval,
            uint8_t depth,
            uint8_t generation,
            uint8_t numPieces,
            bool isPv,
            TTFlag flag
        ) :
            depth(depth),
            generation(generation),
            eval(eval),
            staticEval(staticEval)
        {
            PackedMove packedMove(move);
            _flagAndPackedMove = (static_cast<uint8_t>(flag) << 14) | packedMove._info;
            _hashNpAndIsPv = (hash & HashMask) | (static_cast<hash_t>(numPieces-2) << 1) | static_cast<hash_t>(isPv);
        }

        // Returns how valuable it is to keep the entry in TT
        inline int32_t getPriority() const
        {
            return depth + generation;
        }

        inline PackedMove getPackedMove() const
        {
            return PackedMove(_flagAndPackedMove & 0x3FFF);
        }

        inline hash_t getHash() const
        {
            return _hashNpAndIsPv & HashMask;
        }

        inline TTFlag getTTFlag() const
        {
            return TTFlag((_flagAndPackedMove >> 14) & 0b11);
        }

        inline uint8_t getNumPieces() const
        {
            return static_cast<uint8_t>((_hashNpAndIsPv & NumPiecesMask) >> 1) + 2;
        }

        inline bool isPv() const
        {
            return _hashNpAndIsPv & PvMask;
        }

        inline bool isValid() const
        {
            return depth != InvalidDepth;
        }

        inline void invalidate()
        {
            depth = InvalidDepth;
        }
    };

    struct TTStats
    {
        uint64_t entriesAdded;
        uint64_t replacements;
        uint64_t updates;
        uint64_t blockedUpdates;
        uint64_t lookups;
        uint64_t lookupMisses;
        uint64_t blockedReplacements;
        uint64_t maxEntries;

        TTStats(uint64_t maxEntries) :
            entriesAdded(0),
            replacements(0),
            updates(0),
            blockedUpdates(0),
            lookups(0),
            lookupMisses(0),
            blockedReplacements(0),
            maxEntries(maxEntries)
        {};
    };

    class TranspositionTable
    {
        private:
            // Make each cluster fit into NumClusterBytes bytes
            static constexpr uint32_t NumClusterBytes = 32;
            static constexpr uint32_t NumClusterEntries = NumClusterBytes / sizeof(TTEntry);
            struct TTCluster
            {
                TTEntry entries[NumClusterEntries];
            };

            static_assert(sizeof(TTCluster) == NumClusterBytes, "The size of TTCluster is not correct. Padding might be needed");

            TTCluster* m_table;
            uint32_t m_mbSize;
            size_t m_numClusters;
            size_t m_numEntries;
            TTStats m_stats;
            size_t m_getClusterIndex(hash_t hash);
        public:
            TranspositionTable();
            ~TranspositionTable();

            void prefetch(hash_t hash);
            std::optional<TTEntry> get(hash_t hash, uint8_t plyFromRoot);
            void add(eval_t score, Move move, bool isPv, uint8_t depth, uint8_t plyFromRoot, eval_t staticEval, TTFlag flag, uint8_t generation, uint8_t numPiecesRoot, uint8_t numPieces, hash_t hash);
            void resize(uint32_t mbSize);
            void clear();
            void clearStats();
            TTStats getStats();
            void logStats();
            uint32_t permills(); // Returns how full the table is in permills
    };
}