#pragma once

#include <inttypes.h>
#include <algorithm>

namespace Arcanum
{
    namespace Interface
    {
        int64_t getAllocatedTime(int64_t time, int64_t inc, int64_t movesToGo, int64_t moveTime, int64_t moveOverhead)
        {
            constexpr int64_t T1 = 30;
            constexpr int64_t T2 = 2;

            // Add some margin to the time limit
            // In actuality, the search will likely use a bit more time than allocated
            // This will depend of OS activity, and delays in terminating the search.
            // Thus, we have to ensure it does not surpass the remaining time minus the moveOverhead.
            int64_t timeLimit = time - moveOverhead;
            int64_t allocatedTime = 0LL;

            if(movesToGo > 0)
            {
                // Note: This can exceed the time limit,
                //       but it is resolved at the bottom
                allocatedTime = (timeLimit / movesToGo) + inc;
            }
            else
            {
                allocatedTime = std::min((timeLimit / T1) + inc, timeLimit / T2);
            }

            // If a movetime is specified, it is also an upper bound on the allocated time
            // This has to be done after time allocation, because we want the allocation to depend on the remaining time.
            // Note that moveOverhead is already subtracted from moveTime.
            if(moveTime > 0)
                timeLimit = std::min(timeLimit, moveTime);

            // Ensure that the allocated time does not surpass the time limit
            // Note: timeLimit can be negative, due to subtracting the moveOverhead. Thus, a lower bound of 1ms is used.
            return std::max(std::min(timeLimit, allocatedTime), int64_t(1));
        }
    }
}