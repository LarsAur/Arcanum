#pragma once

#include <string>

namespace Tuning
{
    void fengen(std::string startPosPath, std::string outputPath, size_t numFens, uint8_t numThreads, uint32_t depth);
}