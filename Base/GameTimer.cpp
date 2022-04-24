#include "GameTimer.h"

GameTimer::GameTimer()
: m_deltaTime(-1.0), m_secondPerCount(0.0), m_stopped(false), m_baseTime(0ll), m_pausedTime(0ll), m_stopTime(0ll), m_currentTime(0ll), m_prevTime(0ll)
{
    long long counterPerSecond;
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&counterPerSecond));
    m_secondPerCount = 1.0 / static_cast<double>(counterPerSecond);
}

void GameTimer::Reset() {
    long long currTime;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
    m_baseTime = currTime;
    m_prevTime = currTime;
    m_stopTime = 0;
	m_stopped = false;
}

double GameTimer::TotalTime() const {
    if (m_stopped)
    {
        return static_cast<double>(((m_stopTime - m_pausedTime) - m_baseTime)) * m_secondPerCount;
    } 
	return static_cast<double>(((m_currentTime - m_pausedTime) - m_baseTime)) * m_secondPerCount;
}

double GameTimer::DeltaTime() const {
    return m_deltaTime;
}

void GameTimer::Tick() {
    if (m_stopped)
    {
        m_deltaTime = 0.0;
        return;
    }
    long long currTime;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
    m_currentTime = currTime;
    m_deltaTime = (m_currentTime - m_prevTime) * m_secondPerCount;
    m_prevTime = currTime;

    if (m_deltaTime < 0.0)
        m_deltaTime = 0.0;
}

void GameTimer::Start() {
    long long startTime;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&startTime));
	if (m_stopped)
    {
        m_pausedTime += (startTime - m_stopTime);
        m_prevTime = startTime;
        m_stopTime = 0;
        m_stopped = false;
    }
}

void GameTimer::Stop() {
    if (!m_stopped)
    {
        long long currTime;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
        m_stopTime = currTime;
        m_stopped = true;
    }
}
