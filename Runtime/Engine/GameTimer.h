#pragma once
#include <cstdint>
#include <chrono>

// Game timer based on chrono 
// Each time is stored as milliseconds
class UHGameTimer
{
public:
	UHGameTimer();

	float GetTotalTime() const;
	float GetDeltaTime() const;
	int64_t GetTime() const;
	double GetSecondsPerCount() const;

	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	void Tick();  // Call every frame.

private:
	double SecondsPerCount;
	double DeltaTime;

	int64_t BaseTime;
	int64_t PausedTime;
	int64_t StopTime;
	int64_t PreviousTime;
	int64_t CurrentTime;

	bool bStopped;
};

