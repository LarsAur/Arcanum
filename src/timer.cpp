#include <timer.hpp>

using namespace Arcanum;

Timer::Timer()
{

}

Timer::~Timer()
{

}

void Timer::start()
{
    m_startTime = std::chrono::high_resolution_clock::now();
}

int64_t Timer::getMs()
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_startTime);
    return ms.count();
}

int64_t Timer::getNs()
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - m_startTime);
    return ns.count();
}