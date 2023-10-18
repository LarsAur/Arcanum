#pragma once
#include <board.hpp>
#include <optional>

namespace Arcanum
{
    #define TT_RECORD_STATS 1
    #define TT_FLAG_EXACT 1
    #define TT_FLAG_LOWERBOUND 2
    #define TT_FLAG_UPPERBOUND 3
    #define TT_FLAG_MASK 3

    #define CACHE_LINE_SIZE 64

    typedef hash_t ttEntryHash_t; // Use this to edit the size of the stored hash
    typedef struct ttEntry_t
    {
        ttEntryHash_t hash;
        Move bestMove;
        EvalTrace value;
        int8_t depth; // Depth == INT8_MIN marks the entry as invalid
        uint8_t flags; // Two first bits are FLAG, upper 6 bits are generation
    } ttEntry_t;

    typedef struct ttStats_t
    {
        uint64_t entriesAdded;
        uint64_t replacements;
        uint64_t updates;
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
            ttCluster_t* m_table;
            size_t m_clusterCount;
            size_t m_entryCount;
            ttStats_t m_stats;
            size_t m_getClusterIndex(hash_t hash);
        public:

            TranspositionTable(uint8_t mbSize);
            ~TranspositionTable();

            std::optional<ttEntry_t>get(hash_t hash, uint8_t plyFromRoot);
            void add(EvalTrace score, Move bestMove, uint8_t depth, uint8_t plyFromRoot, uint8_t flags, hash_t hash);
            ttStats_t getStats();
            void logStats();
    };
}