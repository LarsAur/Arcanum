#include <transpositiontable.hpp>
#include <board.hpp>
#include <utils.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <utils.hpp>
#include <memory.hpp>

using namespace Arcanum;

TranspositionTable::TranspositionTable() :
    m_table(nullptr),
    m_mbSize(0),
    m_numClusters(0),
    m_numEntries(0)
{}

TranspositionTable::~TranspositionTable()
{
    Memory::alignedFree(m_table);
}

void TranspositionTable::resize(uint32_t mbSize)
{
    if(m_mbSize == mbSize) return;

    TTcluster* newTable = nullptr;
    size_t numClusters = (mbSize * 1024 * 1024) / sizeof(TTcluster);
    size_t numEntries = ClusterSize * numClusters;

    if(mbSize != 0)
    {
        newTable = static_cast<TTcluster*>(Memory::pageAlignedMalloc(numClusters * sizeof(TTcluster)));

        if(newTable == nullptr)
        {
            WARNING("Failed to allocate new transposition table of size " << mbSize << "MB")
            return;
        }
    }

    // Free the old table and set the new configuration
    if(m_table != nullptr)
        Memory::alignedFree(m_table);

    m_table = newTable;
    m_numClusters = numClusters;
    m_numEntries = numEntries;
    m_stats.maxEntries = numEntries;
    m_mbSize = mbSize;

    LOG("Resized the transpostition table to " << m_mbSize << "MB (" << m_numClusters << " Clusters, " << m_numEntries << " Entries)")

    clear();
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
        .maxEntries   = m_numEntries,
    };
}

void TranspositionTable::clear()
{
    clearStats();

    // Set all table enties to be invalid
    for(size_t i = 0; i < m_numClusters; i++)
    {
        for(size_t j = 0; j < ClusterSize; j++)
        {
            m_table[i].entries[j].depth = InvalidDepth;
            m_table[i].entries[j].hash  = 0LL;
        }
    }
}

inline size_t TranspositionTable::m_getClusterIndex(hash_t hash)
{
    return hash % m_numClusters;
}

void TranspositionTable::prefetch(hash_t hash)
{
    if(m_table)
        _mm_prefetch(m_table + m_getClusterIndex(hash), _MM_HINT_T0);
}

std::optional<TTEntry> TranspositionTable::get(hash_t hash, uint8_t plyFromRoot)
{
    if(!m_table)
        return {};

    TTcluster* clusterPtr = static_cast<TTcluster*>(__builtin_assume_aligned(m_table + m_getClusterIndex(hash), CACHE_LINE_SIZE));
    TTcluster cluster = *clusterPtr;

    m_stats.lookups++;

    for(size_t i = 0; i < ClusterSize; i++)
    {
        TTEntry entry = cluster.entries[i];
        if(entry.depth != InvalidDepth && entry.hash == hash)
        {
            TTEntry retEntry = entry;
            if(Evaluator::isMateScore(entry.value))
            {
                retEntry.value = entry.value > 0 ? entry.value - plyFromRoot : entry.value + plyFromRoot;
            }
            if(Evaluator::isMateScore(entry.staticEval))
            {
                retEntry.staticEval = entry.staticEval > 0 ? entry.staticEval - plyFromRoot : entry.staticEval + plyFromRoot;
            }
            return retEntry;
        }
    }

    m_stats.lookupMisses++;

    return {}; // Return empty optional
}

inline int8_t m_replaceScore(TTEntry& newEntry, TTEntry& oldEntry)
{
    return (newEntry.depth - oldEntry.depth) // Depth
           + (newEntry.generation - oldEntry.generation);
}

void TranspositionTable::add(eval_t score, Move bestMove, bool isPv, uint8_t depth, uint8_t plyFromRoot, eval_t staticEval, TTFlag flag, uint8_t generation, uint8_t numPiecesRoot, uint8_t numPieces, hash_t hash)
{
    if(!m_table)
        return;

    TTcluster* cluster = &m_table[m_getClusterIndex(hash)];

    TTEntry newEntry =
    {
        .hash = hash,
        .value = score,
        .staticEval = staticEval,
        .depth = depth,
        .flags = flag,
        .generation = generation,
        .numPieces = numPieces,
        .bestMove = bestMove,
        .isPv = isPv,
        // Padding is not set
    };

    // Adjust the mate score based on plyFromRoot to make the score represent the mate distance from this position
    if(Evaluator::isMateScore(newEntry.value))
    {
        newEntry.value = newEntry.value > 0 ? newEntry.value + plyFromRoot : newEntry.value - plyFromRoot;
    }

    if(Evaluator::isMateScore(newEntry.staticEval))
    {
        newEntry.staticEval = newEntry.staticEval > 0 ? newEntry.staticEval + plyFromRoot : newEntry.staticEval - plyFromRoot;
    }

    m_stats.entriesAdded++;

    // Check if the entry is already in the cluster
    // If so, replace it
    for(size_t i = 0; i < ClusterSize; i++)
    {
        TTEntry oldEntry = cluster->entries[i];
        if(oldEntry.hash == newEntry.hash)
        {
            if((oldEntry.depth < newEntry.depth) || (oldEntry.isPv < newEntry.isPv))
            {
                m_stats.updates++;
                cluster->entries[i] = newEntry;
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
    for(size_t i = 0; i < ClusterSize; i++)
    {
        TTEntry oldEntry = cluster->entries[i];
        if((oldEntry.depth == InvalidDepth) || (oldEntry.numPieces > numPiecesRoot))
        {
            m_stats.replacements += (oldEntry.depth != InvalidDepth);
            cluster->entries[i] = newEntry;
            return;
        }
    }

    // If no empty entry is found, attempt to find a valid replacement
    TTEntry *replace = nullptr;
    int8_t bestReplaceScore = 0;
    for(size_t i = 0; i < ClusterSize; i++)
    {
        TTEntry oldEntry = cluster->entries[i];
        int8_t replaceScore = m_replaceScore(newEntry, oldEntry);
        if(replaceScore > bestReplaceScore)
        {
            bestReplaceScore = replaceScore;
            replace = &cluster->entries[i];
        }
    }

    // Replace if a suitable replacement is found
    if(replace)
    {
        *replace = newEntry;
        m_stats.replacements++;
    }
    else
    {
        m_stats.blockedReplacements++;
    }

}

// Note: If the table has been resized to a smaller table, the stats may not be entirely accurate.
TTStats TranspositionTable::getStats()
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