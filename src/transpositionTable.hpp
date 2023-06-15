#pragma once
#include <board.hpp>

namespace ChessEngine2
{
    #define TT_RECORD_STATS 1
    #define TT_FLAG_EXACT 1
    #define TT_FLAG_LOWERBOUND 2
    #define TT_FLAG_UPPERBOUND 3
    #define TT_FLAG_MASK 3

    #define CACHE_LINE_SIZE 64

    typedef struct ttEntry_t
    {
        uint16_t hash;
        Move bestMove;
        eval_t value;
        uint8_t depth; // Depth == 0 marks the entry as invalid
        uint8_t flags; // Two first bits are FLAG, upper 6 bits are generation
    } ttEntry_t;

    typedef struct ttStats_t
    {
        uint64_t entriesAdded;
        uint64_t replacements;
        uint64_t lookups;
        uint64_t lookupMisses;
        uint64_t blockedReplacements;
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

            ttEntry_t* getEntry(hash_t hash, bool* hit);
            void addEntry(ttEntry_t entry, hash_t hash);
            ttStats_t getStats();
            size_t getEntryCount();
    };
}