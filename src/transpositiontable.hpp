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
        hash_t hash;
        eval_t value;
        eval_t staticEval;
        uint8_t depth;     // Depth == UINT8_MAX is invalid
        TTFlag flags;
        uint8_t generation;
        // Number of pieces on the board
        // If the root has less pieces, this position can never be reached, and can safely be replaced.
        uint8_t numPieces;
        Move bestMove;     // It would also be possible to pack this data into 6 bytes. For now it is 8
        uint8_t isPv;
        uint8_t _padding; // These bytes are free and can be used for something later

        // Returns how valuable it is to keep the entry in TT
        inline int32_t getPriority() const
        {
            return depth + generation;
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
    };

    class TranspositionTable
    {
        private:
            // Make each cluster fit into CACHE_LINE_SIZE bytes
            static constexpr uint32_t ClusterSize = CACHE_LINE_SIZE / sizeof(TTEntry);
            static constexpr uint32_t ClusterPaddingBytes = CACHE_LINE_SIZE - ClusterSize * sizeof(TTEntry);
            static constexpr uint8_t InvalidDepth = UINT8_MAX;

            struct TTcluster
            {
                TTEntry entries[ClusterSize];
                uint8_t padding[ClusterPaddingBytes];
            };

            TTcluster* m_table;
            uint32_t m_mbSize;
            size_t m_numClusters;
            size_t m_numEntries;
            TTStats m_stats;
            size_t m_getClusterIndex(hash_t hash);
        public:
            TranspositionTable();
            ~TranspositionTable();

            void prefetch(hash_t hash);
            std::optional<TTEntry>get(hash_t hash, uint8_t plyFromRoot);
            void add(eval_t score, Move bestMove, bool isPv, uint8_t depth, uint8_t plyFromRoot, eval_t staticEval, TTFlag flag, uint8_t generation, uint8_t numPiecesRoot, uint8_t numPieces, hash_t hash);
            void resize(uint32_t mbSize);
            void clear();
            void clearStats();
            TTStats getStats();
            void logStats();
            uint32_t permills(); // Returns how full the table is in permills
    };
}