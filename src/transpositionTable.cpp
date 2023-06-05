#include <transpositionTable.hpp>
#include <utils.hpp>

using namespace ChessEngine2;

TranspositionTable::TranspositionTable(uint8_t indexSize)
{
    m_indexSize = indexSize;
    m_size = 1LL << m_indexSize;
    m_table = new ttEntry_t[m_size];
    m_indexBitmask = m_size - 1;
    CHESS_ENGINE2_DEBUG(m_indexBitmask)
    m_stats = {
        .entriesAdded = 0LL,
        .replacements = 0LL,
        .lookups      = 0LL,
        .lookupMisses = 0LL,
    };

    // Set all table enties to be invalid
    for(size_t i = 0; i < m_size; i++)
    {
        m_table->flags = 0; 
    }

    CHESS_ENGINE2_LOG("Created Transposition Table of " << m_size << " elements")
}

TranspositionTable::~TranspositionTable()
{
    delete[] m_table;
}

inline size_t TranspositionTable::m_getTableIndex(hash_t hash)
{
    return hash & m_indexBitmask;
}

ttEntry_t TranspositionTable::getEntry(hash_t hash)
{
    ttEntry_t entry = m_table[m_getTableIndex(hash)];
    #if TT_RECORD_STATS == 1
        m_stats.lookups++;
        if(((entry.flags & TT_FLAG_VALID) == 0) || (((entry.flags & TT_FLAG_VALID) == 1) && (entry.hash != hash)))
        {
            m_stats.lookupMisses++;
        }
    #endif

    return entry;
}

void TranspositionTable::addEntry(ttEntry_t entry)
{
    hash_t tableIndex = m_getTableIndex(entry.hash);
    #if TT_RECORD_STATS == 1
    m_stats.entriesAdded++;
    ttEntry_t _entry = m_table[tableIndex];
    if((_entry.flags & TT_FLAG_VALID) && (_entry.hash != entry.hash))
    {
        m_stats.replacements++;
    }
    #endif
    m_table[tableIndex] = entry;
}

ttStats_t TranspositionTable::getStats()
{
    return m_stats;
}