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

    typedef hash_t ttEntryHash_t; // Use this to edit the size of the stored hash
    typedef struct ttEntry_t
    {
        ttEntryHash_t hash;
        eval_t value;
        eval_t staticEval;
        uint8_t depth;     // Depth == UINT8_MAX is invalid
        TTFlag flags;
        uint8_t generation;
        // Number of non-reversable moves performed in the position.
        // If the root has more non-reversable moves performed, this position can never be reached.
        // In that case it can safely be replaced.
        uint8_t numNonRevMoves;
        Move bestMove;     // It would also be possible to pack this data into 6 bytes. For now it is 8
        int16_t _padding; // These bytes are free and can be used for something later
    } ttEntry_t;

    typedef struct ttStats_t
    {
        uint64_t entriesAdded;
        uint64_t replacements;
        uint64_t updates;
        uint64_t blockedUpdates;
        uint64_t lookups;
        uint64_t lookupMisses;
        uint64_t blockedReplacements;
        uint64_t maxEntries;
    } ttStats_t;

    // Make each cluster fit into CACHE_LINE_SIZE bytes
    static constexpr uint32_t clusterSize = CACHE_LINE_SIZE / sizeof(ttEntry_t);
    static constexpr uint32_t clusterPaddingBytes = CACHE_LINE_SIZE - clusterSize * sizeof(ttEntry_t);

    typedef struct ttCluster_t
    {
        ttEntry_t entries[clusterSize];
        uint8_t padding[clusterPaddingBytes];
    } ttCluster_t;

    class TranspositionTable
    {
        private:
            uint32_t m_mbSize;
            ttCluster_t* m_table;
            size_t m_clusterCount;
            size_t m_entryCount;
            ttStats_t m_stats;
            size_t m_getClusterIndex(hash_t hash);
        public:

            TranspositionTable(uint32_t mbSize);
            ~TranspositionTable();

            void prefetch(hash_t hash);
            std::optional<ttEntry_t>get(hash_t hash, uint8_t plyFromRoot);
            void add(eval_t score, Move bestMove, uint8_t depth, uint8_t plyFromRoot, eval_t staticEval, TTFlag flag, uint8_t generation, uint8_t nonRevMovesRoot, uint8_t nonRevMoves, hash_t hash);
            void resize(uint32_t mbSize);
            void clear();
            void clearStats();
            ttStats_t getStats();
            void logStats();
            uint32_t permills(); // Returns how full the table is in permills
    };
}