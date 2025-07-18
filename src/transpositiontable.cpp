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
    m_numEntries(0),
    m_stats(TTStats(0))
{}

TranspositionTable::~TranspositionTable()
{
    Memory::alignedFree(m_table);
}

void TranspositionTable::resize(uint32_t mbSize)
{
    if(m_mbSize == mbSize) return;

    TTCluster* newTable = nullptr;
    size_t numClusters = (mbSize * 1024 * 1024) / sizeof(TTCluster);
    size_t numEntries = NumClusterEntries * numClusters;

    if(mbSize != 0)
    {
        newTable = static_cast<TTCluster*>(Memory::pageAlignedMalloc(numClusters * sizeof(TTCluster)));

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
    m_stats = TTStats(m_numEntries);
}

void TranspositionTable::clear()
{
    clearStats();

    // Set all table enties to be invalid
    for(size_t i = 0; i < m_numClusters; i++)
    {
        for(size_t j = 0; j < NumClusterEntries; j++)
        {
            m_table[i].entries[j].invalidate();
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

    TTCluster* clusterPtr = static_cast<TTCluster*>(__builtin_assume_aligned(m_table + m_getClusterIndex(hash), CACHE_LINE_SIZE));
    TTCluster cluster = *clusterPtr;

    m_stats.lookups++;

    for(size_t i = 0; i < NumClusterEntries; i++)
    {
        TTEntry entry = cluster.entries[i];

        if(entry.isValid() && entry.getHash() == (hash & TTEntry::HashMask))
        {
            TTEntry retEntry = entry;
            if(Evaluator::isMateScore(entry.eval))
            {
                retEntry.eval = entry.eval > 0 ? entry.eval - plyFromRoot : entry.eval + plyFromRoot;
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

void TranspositionTable::add(eval_t eval, Move move, bool isPv, uint8_t depth, uint8_t plyFromRoot, eval_t staticEval, TTFlag flag, uint8_t generation, uint8_t numPiecesRoot, uint8_t numPieces, hash_t hash)
{
    if(!m_table)
        return;

    TTCluster* cluster = &m_table[m_getClusterIndex(hash)];

    TTEntry newEntry(hash, move, eval, staticEval, depth, generation, numPieces, isPv, flag);

    // Adjust the mate score based on plyFromRoot to make the score represent the mate distance from this position
    if(Evaluator::isMateScore(newEntry.eval))
    {
        newEntry.eval = newEntry.eval > 0 ? newEntry.eval + plyFromRoot : newEntry.eval - plyFromRoot;
    }

    if(Evaluator::isMateScore(newEntry.staticEval))
    {
        newEntry.staticEval = newEntry.staticEval > 0 ? newEntry.staticEval + plyFromRoot : newEntry.staticEval - plyFromRoot;
    }

    m_stats.entriesAdded++;

    // Check if the entry is already in the cluster
    // If so, try to update it
    for(size_t i = 0; i < NumClusterEntries; i++)
    {
        TTEntry oldEntry = cluster->entries[i];
        if(oldEntry.isValid() && (oldEntry.getHash() == newEntry.getHash()))
        {
            if((oldEntry.depth < newEntry.depth) || (oldEntry.isPv() < newEntry.isPv()))
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

    // Check if the new entry can/should be placed into the cluster
    // Find the entry with the lowest priority, and replace it if the new entry has a higher priority
    TTEntry *replace = nullptr;
    int32_t lowestPriority = newEntry.getPriority();
    for(size_t i = 0; i < NumClusterEntries; i++)
    {
        TTEntry* oldEntry = &cluster->entries[i];

        // Prioritize replacing empty entries
        // Or positions which cannot be hit again (safe replacement)
        if(!oldEntry->isValid() || (oldEntry->getNumPieces() > numPiecesRoot))
        {
            m_stats.replacements += oldEntry->isValid();
            *oldEntry = newEntry;
            return;
        }

        int32_t priority = oldEntry->getPriority();
        if(priority < lowestPriority)
        {
            lowestPriority = priority;
            replace = oldEntry;
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