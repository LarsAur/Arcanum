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

    struct TTEntry
    {
        static constexpr uint8_t InvalidDepth = UINT8_MAX;

        // Note: For the hash, the LSB does not matter as much as hashes
        // placed in the same cluster will have the same LSBs of the hash
        // In fact, for the smallest non-zero TT (1MB) the 15 LSBs will match in each cluster
        static constexpr hash_t  HashMask        = 0xFFFFFFFFFFFFC000;
        static constexpr hash_t  PvMask          = 0b1;
        static constexpr uint8_t PvOffset        = 0;
        static constexpr hash_t  NumPiecesMask   = 0b111110;
        static constexpr uint8_t NumPiecesOffset = 1;
        static constexpr hash_t  TTFlagMask      = 0b11000000;
        static constexpr uint8_t TTFlagOffset    = 6;
        static constexpr uint8_t MaxGeneration   = 0xff;

        // Total 16 bytes
        uint8_t depth;             // 1 byte
        uint8_t generation;        // 1 byte
        eval_t eval;               // 2 bytes
        eval_t rawEval;            // 2 bytes
        PackedMove packedMove;     // 2 bytes
        hash_t _hashNpFlagAndIsPv; // 8 bytes: [50 bits: hash | 2 bits: TT Flag | 5 bits: numPieces-2 | 1 bit: isPv]

        TTEntry(
            hash_t hash,
            Move move,
            eval_t eval,
            eval_t rawEval,
            uint8_t depth,
            uint8_t generation,
            uint8_t numPieces,
            bool isPv,
            TTFlag flag
        ) :
            depth(depth),
            generation(generation),
            eval(eval),
            rawEval(rawEval),
            packedMove(PackedMove(move))
        {
            _hashNpFlagAndIsPv = (hash & HashMask)
            | (static_cast<hash_t>(flag) << TTFlagOffset)
            | (static_cast<hash_t>(numPieces-2) << NumPiecesOffset)
            | (static_cast<hash_t>(isPv) << PvOffset);
        }

        // Returns how valuable it is to keep the entry in TT
        inline int32_t getPriority(uint8_t currentGeneration) const
        {
            return depth - getAge(currentGeneration);
        }

        inline int8_t getAge(uint8_t currentGeneration) const
        {
            // Find the age of the entry with respect to the current generation
            // This supports warp around for the generation counter
            return (MaxGeneration + 1 + currentGeneration - generation) & MaxGeneration;
        }

        inline PackedMove getPackedMove() const
        {
            return packedMove;
        }

        inline hash_t getHash() const
        {
            return _hashNpFlagAndIsPv & HashMask;
        }

        inline TTFlag getTTFlag() const
        {
            return TTFlag((_hashNpFlagAndIsPv & TTFlagMask) >> TTFlagOffset);
        }

        inline uint8_t getNumPieces() const
        {
            return static_cast<uint8_t>((_hashNpFlagAndIsPv & NumPiecesMask) >> NumPiecesOffset) + 2;
        }

        inline bool isPv() const
        {
            return (_hashNpFlagAndIsPv & PvMask) >> PvOffset;
        }

        inline bool isValid() const
        {
            return depth != InvalidDepth;
        }

        inline void invalidate()
        {
            depth = InvalidDepth;
        }
    };

    struct TTStats
    {
        uint64_t entriesAdded;
        uint64_t replacements;
        uint64_t updates;
        uint64_t blockedUpdates;
        uint64_t lookups;
        uint64_t lookupMisses;
        uint64_t blockedReplacements;
        uint64_t maxEntries;

        TTStats(uint64_t maxEntries) :
            entriesAdded(0),
            replacements(0),
            updates(0),
            blockedUpdates(0),
            lookups(0),
            lookupMisses(0),
            blockedReplacements(0),
            maxEntries(maxEntries)
        {};
    };

    class TranspositionTable
    {
        private:
            // Make each cluster fit into NumClusterBytes bytes
            static constexpr uint32_t NumClusterBytes = 32;
            static constexpr uint32_t NumClusterEntries = NumClusterBytes / sizeof(TTEntry);
            struct TTCluster
            {
                TTEntry entries[NumClusterEntries];
            };

            static_assert(sizeof(TTCluster) == NumClusterBytes, "The size of TTCluster is not correct. Padding might be needed");

            TTCluster* m_table;
            uint32_t m_mbSize;
            size_t m_numClusters;
            size_t m_numEntries;
            TTStats m_stats;
            uint8_t m_generation;
            size_t m_getClusterIndex(hash_t hash);
            eval_t m_toTTEval(eval_t eval, uint8_t plyFromRoot);
            eval_t m_fromTTEval(eval_t eval, uint8_t plyFromRoot);
        public:
            TranspositionTable();
            ~TranspositionTable();

            void prefetch(hash_t hash);
            void incrementGeneration();
            std::optional<TTEntry> get(hash_t hash, uint8_t plyFromRoot);
            void add(eval_t score, Move move, bool isPv, uint8_t depth, uint8_t plyFromRoot, eval_t rawEval, TTFlag flag, uint8_t numPiecesRoot, uint8_t numPieces, hash_t hash);
            void resize(uint32_t mbSize);
            void clear();
            void clearStats();
            TTStats getStats();
            void logStats();
            uint32_t permills(); // Returns how full the table is in permills
    };
}