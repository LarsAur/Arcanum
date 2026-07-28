#pragma once

#include <string>
#include <types.hpp>
#include <search.hpp>

namespace Arcanum
{
    class PostProcessing
    {
        public:
            struct QuiesceParameters
            {
                std::string inputPath;
                std::string outputPath;
                uint32_t numThreads;
                uint32_t offset;
            };

            struct ReEvalParameters
            {
                std::string inputPath;
                std::string outputPath;
                uint32_t numThreads;
                uint32_t depth;
                uint32_t nodes;
                uint32_t movetime;
                uint32_t offset;
                uint32_t ttSize;

                ReEvalParameters() :
                    numThreads(1),
                    depth(0),
                    nodes(0),
                    movetime(0),
                    offset(0),
                    ttSize(0)
                {}
            };

            struct FilterParameters
            {
                std::string inputPath;
                std::string outputPath;
                uint32_t offset;
                eval_t staticMargin;
                bool filterStaticMargin;
                uint32_t maxHalfMoves;
                bool filterMaxHalfMoves;
                eval_t maxEval;
                bool filterMaxEval;
                uint32_t minPieces;
                bool filterMinPieces;
                bool filterCaptures;
                bool filterChecks;
                bool filterSingleMove;

                FilterParameters() :
                    offset(0),
                    staticMargin(0),
                    filterStaticMargin(false),
                    maxHalfMoves(0),
                    filterMaxHalfMoves(false),
                    maxEval(0),
                    filterMaxEval(false),
                    minPieces(0),
                    filterMinPieces(false),
                    filterCaptures(false),
                    filterChecks(false),
                    filterSingleMove(false)
                {}
            };

            struct DeduplicateParameters
            {
                std::string inputPath;
                std::string outputPath;
                uint32_t buckets;
                uint32_t startBucket;
            };

            static void quiesce(const QuiesceParameters& params);
            static void reeval(const ReEvalParameters& params);
            static void filter(const FilterParameters& params);
            static void deduplicate(const DeduplicateParameters& params);
    };
}