#include <transpositionTable.hpp>
#include <board.hpp>
#include <utils.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

using namespace Arcanum;

TranspositionTable::TranspositionTable(uint8_t mbSize)
{
    m_clusterCount = (mbSize * 1024 * 1024) / sizeof(ttCluster_t);
    m_entryCount = clusterSize * m_clusterCount;
    m_table = static_cast<ttCluster_t*>(aligned_large_pages_alloc(m_clusterCount * sizeof(ttCluster_t)));

    if(m_table == nullptr)
    {
        ERROR("Unable to allocate transposition table");
        exit(EXIT_FAILURE);
    }

    // Set all table enties to be invalid
    // TODO: This can be done using memset
    //       Without any optimizations, this is slow
    for(size_t i = 0; i < m_clusterCount; i++)
    {
        for(size_t j = 0; j < clusterSize; j++)
        {
            m_table[i].entries[j].depth = INT8_MIN;
        }
    }

    m_stats = {
        .entriesAdded = 0LL,
        .replacements = 0LL,
        .lookups      = 0LL,
        .lookupMisses = 0LL,
        .blockedReplacements = 0LL,
    };

    LOG("Created Transposition Table of " << m_clusterCount << " clusters and " << m_entryCount << " entries using " << unsigned(mbSize) << " MB")
}

TranspositionTable::~TranspositionTable()
{
    aligned_large_pages_free(m_table);
}

inline size_t TranspositionTable::m_getClusterIndex(hash_t hash)
{
    return hash % m_clusterCount;
}

std::optional<ttEntry_t> TranspositionTable::get(hash_t hash, uint8_t plyFromRoot)
{
    ttCluster_t* cluster = &m_table[m_getClusterIndex(hash)];

    #if TT_RECORD_STATS == 1
    m_stats.lookups++;
    #endif

    for(size_t i = 0; i < clusterSize; i++)
    {
        ttEntry_t entry = cluster->entries[i];
        if(entry.depth != INT8_MIN && entry.hash == (ttEntryHash_t)hash)
        {
            ttEntry_t retEntry = entry;
            if(Eval::isCheckMateScore(entry.value))
            {
                retEntry.value.total = entry.value > 0 ? entry.value.total - plyFromRoot : entry.value.total + plyFromRoot;
            }
            return retEntry;
        }
    }

    #if TT_RECORD_STATS == 1
        m_stats.lookupMisses++;
    #endif

    return {}; // Return empty optional
}

inline int8_t m_replaceScore(ttEntry_t newEntry, ttEntry_t oldEntry)
{
    return (newEntry.depth - oldEntry.depth) // Depth
           + (((newEntry.flags & TT_FLAG_MASK) == TT_FLAG_EXACT && (newEntry.flags & TT_FLAG_MASK) != TT_FLAG_EXACT) ? 2 : 0)
           + (((newEntry.flags & ~TT_FLAG_MASK) - (oldEntry.flags & ~TT_FLAG_MASK)) >> 2);
}

void TranspositionTable::add(EvalTrace score, Move bestMove, uint8_t depth, uint8_t plyFromRoot, uint8_t flags, hash_t hash)
{
    ttCluster_t* cluster = &m_table[m_getClusterIndex(hash)];
    ttEntry_t entry = 
    {
        .hash = (ttEntryHash_t)hash,
        .bestMove = bestMove,
        .value = score, 
        .depth = (int8_t) depth,
        .flags = flags
    };

    if(Eval::isCheckMateScore(entry.value))
    {
        entry.value.total = entry.value > 0 ? entry.value.total + plyFromRoot : entry.value.total - plyFromRoot;
    }

    #if TT_RECORD_STATS == 1
        m_stats.entriesAdded++;
    #endif

    // Check if the entry is already in the cluster
    // If so, replace it
    for(size_t i = 0; i < clusterSize; i++)
    {
        ttEntry_t _entry = cluster->entries[i];
        if(_entry.hash == entry.hash && _entry.depth < entry.depth)
        {
            #if TT_RECORD_STATS == 1
                m_stats.updates++;
            #endif
            cluster->entries[i] = entry;
            return;
        }
    }

    // Search the cluster for an empty entry
    for(size_t i = 0; i < clusterSize; i++)
    {
        ttEntry_t _entry = cluster->entries[i];
        if(_entry.depth < 0)
        {
            cluster->entries[i] = entry;
            return;
        }
    }

    // If no empty entry is found, attempt to find a valid replacement
    ttEntry_t *replace = nullptr;
    int8_t bestReplaceScore = 0;
    for(size_t i = 0; i < clusterSize; i++)
    {
        ttEntry_t _entry = cluster->entries[i];
        int8_t replaceScore = m_replaceScore(entry, _entry);
        if(replaceScore > bestReplaceScore)
        {
            bestReplaceScore = replaceScore;
            replace = &cluster->entries[i];
        }
    }

    // Replace if a suitable replacement is found
    if(replace)
    {
        *replace = entry;

        #if TT_RECORD_STATS
            m_stats.replacements++;
        #endif
    }
    #if TT_RECORD_STATS
    else
    {
        m_stats.blockedReplacements++;
    }
    #endif

}

ttStats_t TranspositionTable::getStats()
{
    return m_stats;
}

size_t TranspositionTable::getEntryCount()
{
    return m_entryCount;
}