#pragma once

#include <Windows.h>

class GameTimer {
public:
	GameTimer();
	void Reset();
	double TotalTime() const;
	double DeltaTime() const;
	void Tick();
	void Start();
	void Stop();
private:
	double		m_deltaTime;
	double		m_secondPerCount;
	bool		m_stopped;

	long long	m_baseTime;
	long long	m_pausedTime;
	long long	m_stopTime;
	long long	m_prevTime;
	long long	m_currentTime;
};

