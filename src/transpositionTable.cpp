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

TranspositionTable::TranspositionTable(uint32_t mbSize)
{
    m_mbSize = mbSize;
    m_clusterCount = (mbSize * 1024 * 1024) / sizeof(ttCluster_t);
    m_entryCount = clusterSize * m_clusterCount;
    m_table = static_cast<ttCluster_t*>(Memory::pageAlignedMalloc(m_clusterCount * sizeof(ttCluster_t)));

    if(m_table == nullptr)
    {
        ERROR("Unable to allocate transposition table")
        exit(EXIT_FAILURE);
    }

    clear();

    LOG("Created Transposition Table of " << m_clusterCount << " clusters and " << m_entryCount << " entries using " << unsigned(mbSize) << " MB")
}

TranspositionTable::~TranspositionTable()
{
    Memory::alignedFree(m_table);
}

void TranspositionTable::resize(uint32_t mbSize)
{
    if(m_mbSize == mbSize) return;

    ttCluster_t* newTable = nullptr;
    size_t clusterCount = (mbSize * 1024 * 1024) / sizeof(ttCluster_t);
    size_t entryCount = clusterSize * clusterCount;

    if(mbSize != 0)
    {
        newTable = static_cast<ttCluster_t*>(Memory::pageAlignedMalloc(clusterCount * sizeof(ttCluster_t)));

        if(newTable == nullptr)
        {
            WARNING("Failed to allocate new transposition table of size " << mbSize << "MB")
            return;
        }

        // Copy the previous entries.
        // If the new table is smaller, copy as many entries as possible.
        // Because it just copies the data, the low indices are prioritized over high indices
        size_t minClusterCount = std::min(m_clusterCount, clusterCount);
        memcpy(newTable, m_table, minClusterCount * sizeof(ttCluster_t));

        // If the new table is larger, initialize the remaining 'uncopied' entries
        for(size_t i = minClusterCount; i < clusterCount; i++)
            for(size_t j = 0; j < clusterSize; j++)
                newTable[i].entries[j].depth = UINT8_MAX;
    }

    // Free the old table and set the new configuration
    Memory::alignedFree(m_table);
    m_table = newTable;
    m_clusterCount = clusterCount;
    m_entryCount = entryCount;
    m_stats.maxEntries = entryCount;
    LOG("Successfully resized the transpostition table from " << m_mbSize << "MB to " << mbSize << "MB")
    m_mbSize = mbSize;
}

void TranspositionTable::clearStats()
{
    m_stats = {
        .entriesAdded = 0LL,
        .replacements = 0LL,
        .updates      = 0LL,
        .lookups      = 0LL,
        .lookupMisses = 0LL,
        .blockedReplacements = 0LL,
        .maxEntries   = m_entryCount,
    };
}

void TranspositionTable::clear()
{
    clearStats();

    // Set all table enties to be invalid
    for(size_t i = 0; i < m_clusterCount; i++)
        for(size_t j = 0; j < clusterSize; j++)
            m_table[i].entries[j].depth = UINT8_MAX;
}

inline size_t TranspositionTable::m_getClusterIndex(hash_t hash)
{
    return hash % m_clusterCount;
}

void TranspositionTable::prefetch(hash_t hash)
{
    _mm_prefetch(m_table + m_getClusterIndex(hash), _MM_HINT_T2);
}

std::optional<ttEntry_t> TranspositionTable::get(hash_t hash, uint8_t plyFromRoot)
{
    if(!m_table)
        return {};

    ttCluster_t* clusterPtr = static_cast<ttCluster_t*>(__builtin_assume_aligned(m_table + m_getClusterIndex(hash), CACHE_LINE_SIZE));
    ttCluster_t cluster = *clusterPtr;

    m_stats.lookups++;

    for(size_t i = 0; i < clusterSize; i++)
    {
        ttEntry_t entry = cluster.entries[i];
        if(entry.depth != UINT8_MAX && entry.hash == (ttEntryHash_t)hash)
        {
            ttEntry_t retEntry = entry;
            if(Evaluator::isCheckMateScore(entry.value) || Evaluator::isTbCheckMateScore(entry.value))
            {
                retEntry.value = entry.value > 0 ? entry.value - plyFromRoot : entry.value + plyFromRoot;
            }
            if(Evaluator::isCheckMateScore(entry.staticEval) || Evaluator::isTbCheckMateScore(entry.staticEval))
            {
                retEntry.staticEval = entry.staticEval > 0 ? entry.staticEval - plyFromRoot : entry.staticEval + plyFromRoot;
            }
            return retEntry;
        }
    }

    m_stats.lookupMisses++;

    return {}; // Return empty optional
}

inline int8_t m_replaceScore(ttEntry_t newEntry, ttEntry_t oldEntry)
{
    return (newEntry.depth - oldEntry.depth) // Depth
           + (newEntry.generation - oldEntry.generation);
}

void TranspositionTable::add(eval_t score, Move bestMove, uint8_t depth, uint8_t plyFromRoot, eval_t staticEval, TTFlag flag, uint8_t generation, uint8_t numNonRevMovesRoot, uint8_t numNonRevMoves, hash_t hash)
{
    if(!m_table)
        return;

    ttCluster_t* cluster = &m_table[m_getClusterIndex(hash)];
    ttEntry_t entry =
    {
        .hash = (ttEntryHash_t)hash,
        .value = score,
        .staticEval = staticEval,
        .depth = depth,
        .flags = flag,
        .generation = generation,
        .numNonRevMoves = numNonRevMoves,
        .bestMove = bestMove,
        // Padding is not set
    };

    if(Evaluator::isCheckMateScore(entry.value) || Evaluator::isTbCheckMateScore(entry.value))
    {
        entry.value = entry.value > 0 ? entry.value + plyFromRoot : entry.value - plyFromRoot;
    }

    if(Evaluator::isCheckMateScore(entry.staticEval) || Evaluator::isTbCheckMateScore(entry.staticEval))
    {
        entry.staticEval = entry.staticEval > 0 ? entry.staticEval + plyFromRoot : entry.staticEval - plyFromRoot;
    }

    m_stats.entriesAdded++;

    // Check if the entry is already in the cluster
    // If so, replace it
    for(size_t i = 0; i < clusterSize; i++)
    {
        ttEntry_t _entry = cluster->entries[i];
        if(_entry.hash == entry.hash)
        {
            if(_entry.depth < entry.depth)
            {
                m_stats.updates++;
                cluster->entries[i] = entry;
            }
            else
            {
                m_stats.blockedUpdates++;
            }

            return;
        }
    }

    // Search the cluster for an empty entry
    // Or if it can replace a position which cannot be hit again (safe replacement)
    for(size_t i = 0; i < clusterSize; i++)
    {
        ttEntry_t _entry = cluster->entries[i];
        if((_entry.depth == UINT8_MAX) || (_entry.numNonRevMoves < numNonRevMovesRoot))
        {
            m_stats.replacements += (_entry.depth != UINT8_MAX);
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
        m_stats.replacements++;
    }
    else
    {
        m_stats.blockedReplacements++;
    }

}

// Note: If the table has been resized to a smaller table, the stats may not be entirely accurate.
ttStats_t TranspositionTable::getStats()
{
    return m_stats;
}

void TranspositionTable::logStats()
{
    uint64_t entriesInTable = m_stats.entriesAdded - m_stats.replacements - m_stats.blockedReplacements - m_stats.updates - m_stats.blockedUpdates;
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
    ss << "\nBlocked Updates:      " << m_stats.blockedUpdates;
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

uint32_t TranspositionTable::permills()
{
    uint64_t entriesInTable = m_stats.entriesAdded - m_stats.replacements - m_stats.blockedReplacements - m_stats.updates - m_stats.blockedUpdates;
    return m_stats.maxEntries > 0 ? (1000 * entriesInTable / m_stats.maxEntries) : 1000;
}