#pragma once

#include <string>
#include <types.hpp>
#include <search.hpp>

namespace Arcanum
{
    class PostProcessing
    {
        public:
            struct QuietGenParameters
            {
                std::string inputPath;
                std::string outputPath;
                uint32_t numThreads;
                eval_t qMargin;
                eval_t margin;
                uint32_t depth;
                uint32_t nodes;
                uint32_t movetime;
                uint32_t offset;

                QuietGenParameters() :
                    numThreads(1),
                    qMargin(0),
                    margin(0),
                    depth(0),
                    nodes(0),
                    movetime(0),
                    offset(0)
                {}
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
                uint32_t numThreads;
                eval_t margin;
                uint32_t offset;

                FilterParameters() :
                    numThreads(1),
                    margin(0),
                    offset(0)
                {}
            };

            struct DeduplicateParameters
            {
                std::string inputPath;
                std::string outputPath;
                uint32_t buckets;
                uint32_t startBucket;
            };

            static void generateQuiets(const QuietGenParameters& params);
            static void reeval(const ReEvalParameters& params);
            static void filter(const FilterParameters& params);
            static void deduplicate(const DeduplicateParameters& params);
    };
}