#pragma once
#include <board.hpp>

namespace ChessEngine2
{
    #define TT_RECORD_STATS 1
    #define TT_FLAG_VALID 1
    #define TT_FLAG_EXACT 2
    #define TT_FLAG_LOWERBOUND 4
    #define TT_FLAG_UPPERBOUND 8

    typedef struct ttEntry_t
    {
        hash_t hash;
        Move bestMove; // TODO: Can store only the to and from to save space
        eval_t value;
        uint8_t depth;
        uint8_t flags;
    } ttEntry_t;

    typedef struct ttStats_t
    {
        uint64_t entriesAdded;
        uint64_t replacements;
        uint64_t lookups;
        uint64_t lookupMisses;
        uint64_t blockedReplacements;
    } ttStats_t;

    class TranspositionTable
    {
        private:
            ttEntry_t* m_table;
            size_t m_size;
            size_t m_indexBitmask;
            uint8_t m_indexSize;
            ttStats_t m_stats;

            size_t m_getTableIndex(hash_t hash);
        public:

            TranspositionTable(uint8_t indexSize);
            ~TranspositionTable();

            ttEntry_t getEntry(hash_t hash);
            void addEntry(ttEntry_t entry);
            ttStats_t getStats();
    };
}