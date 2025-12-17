#pragma once

#include <tuning/dataloader.hpp>
#include <types.hpp>
#include <string>
#include <search.hpp>

namespace Arcanum
{
    struct FengenParameters
    {
        std::string startposPath; // Path to EDP file containing start positions
        std::string outputPath;   // Path to the output file
        std::string syzygyPath;   // Path to the syzygy tablebases
        uint32_t    numRandomMoves; // Number of random moves at the beginning of the game
        uint32_t offset;          // Start offset in the startpos EDP file
        uint32_t numFens;         // Number of FENs to generate
        uint32_t numThreads;      // Number of threads to use
        uint32_t depth;           // Max depth to search to. Unused if 0
        uint32_t movetime;        // Max time to search (ms). Unused if 0
        uint32_t nodes;           // Max nodes to search. Unused if 0

        FengenParameters() :
        startposPath(""),
        outputPath(""),
        syzygyPath(""),
        numRandomMoves(0),
        offset(0),
        numFens(0),
        numThreads(0),
        depth(0),
        movetime(0),
        nodes(0) 
        {};
    };

    class Fengen{
        public:
        static void start(FengenParameters params);
    };
}