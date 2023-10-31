#include <transpositionTable.hpp>
#include <board.hpp>
#include <utils.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <utils.hpp>
#include <memory.hpp>

using namespace Arcanum;

TranspositionTable::TranspositionTable(uint8_t mbSize)
{
    m_clusterCount = (mbSize * 1024 * 1024) / sizeof(ttCluster_t);
    m_entryCount = clusterSize * m_clusterCount;
    m_table = static_cast<ttCluster_t*>(Memory::pageAlignedMalloc(m_clusterCount * sizeof(ttCluster_t)));

    if(m_table == nullptr)
    {
        ERROR("Unable to allocate transposition table")
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
        .updates      = 0LL,
        .lookups      = 0LL,
        .lookupMisses = 0LL,
        .blockedReplacements = 0LL,
        .maxEntries   = m_entryCount,
    };

    LOG("Created Transposition Table of " << m_clusterCount << " clusters and " << m_entryCount << " entries using " << unsigned(mbSize) << " MB")
}

TranspositionTable::~TranspositionTable()
{
    Memory::pageAlignedFree(m_table);
}

inline size_t TranspositionTable::m_getClusterIndex(hash_t hash)
{
    return hash % m_clusterCount;
}

std::optional<ttEntry_t> TranspositionTable::get(hash_t hash, uint8_t plyFromRoot)
{
    ttCluster_t* clusterPtr = static_cast<ttCluster_t*>(__builtin_assume_aligned(m_table + m_getClusterIndex(hash), CACHE_LINE_SIZE));
    ttCluster_t cluster = *clusterPtr;

    #if TT_RECORD_STATS == 1
    m_stats.lookups++;
    #endif

    for(size_t i = 0; i < clusterSize; i++)
    {
        ttEntry_t entry = cluster.entries[i];
        if(entry.depth != INT8_MIN && entry.hash == (ttEntryHash_t)hash)
        {
            ttEntry_t retEntry = entry;
            if(Evaluator::isCheckMateScore(entry.value))
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

    if(Evaluator::isCheckMateScore(entry.value))
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

void TranspositionTable::logStats()
{
    uint64_t entriesInTable = m_stats.entriesAdded - m_stats.replacements - m_stats.blockedReplacements - m_stats.updates;
    uint64_t lookupHits = m_stats.lookups - m_stats.lookupMisses;

    std::stringstream ss;
    ss << "\n----------------------------------";
    ss << "\nTransposition Table Stats:"; 
    ss << "\n----------------------------------";
    ss << "\nEntries Added:        " << m_stats.entriesAdded;
    ss << "\nEntries In Table:     " << entriesInTable;
    ss << "\nReplaced Entries:     " << m_stats.replacements;
    ss << "\nBlocked Replacements: " << m_stats.blockedReplacements;
    ss << "\nUpdated Entries:      " << m_stats.updates;
    ss << "\nLookups:              " << m_stats.lookups;
    ss << "\nLookup Hits:          " << lookupHits;
    ss << "\nLookup Misses:        " << m_stats.lookupMisses;
    ss << "\nTotal Capacity:       " << m_stats.maxEntries;
    ss << "\n";
    ss << "\nPercentages:";
    ss << "\n----------------------------------";
    ss << "\nCapacity Used:        " << (float) (100 * entriesInTable) / m_stats.maxEntries << "%";
    ss << "\nHitrate:              " << (float) (100 * lookupHits) / m_stats.lookups << "%";
    ss << "\nMissrate:             " << (float) (100 * m_stats.lookupMisses) / m_stats.lookups << "%";
    ss << "\n----------------------------------";

    LOG(ss.str())
} 