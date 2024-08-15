#pragma once

#include <chrono>

namespace Arcanum
{

    class Timer
    {
    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;
    public:
        Timer();
        ~Timer();

        void start();
        int64_t getMs();
        int64_t getNs();
    };

}