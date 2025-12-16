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
        uint32_t    numRandomMoves; // Number of random moves at the beginning of the game
        std::string outputPath;   // Path to the output file
        uint32_t offset;          // Start offset in the startpos EDP file
        uint32_t numFens;         // Number of FENs to generate
        uint32_t numThreads;      // Number of threads to use
        uint32_t depth;           // Max depth to search to. Unused if 0
        uint32_t movetime;        // Max time to search (ms). Unused if 0
        uint32_t nodes;           // Max nodes to search. Unused if 0

        FengenParameters() :
        startposPath(""),
        numRandomMoves(0),
        outputPath(""),
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
        // Parses command line arguments and runs fengen if the arguments are valid
        // Returns false if the arguments are not a fengen command
        static bool parseArgumentsAndRunFengen(int argc, char* argv[]);
        static void start(FengenParameters params);
    };
}