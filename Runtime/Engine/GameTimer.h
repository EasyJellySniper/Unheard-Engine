#pragma once
#include <cstdint>
#include <chrono>
#include <utility>
#include <vector>

// Game timer based on chrono 
// Each time is stored as milliseconds
using UHClock = std::chrono::steady_clock;
class UHGameTimer
{
public:
	UHGameTimer();

	float GetTotalTime() const;
	float GetDeltaTime() const;
	UHClock::time_point GetTime() const;

	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	void Tick();  // Call every frame.

private:
	double DeltaTime;

	UHClock::time_point BaseTime;
	UHClock::time_point PausedTime;
	UHClock::time_point StopTime;
	UHClock::time_point PreviousTime;
	UHClock::time_point CurrentTime;

	bool bStopped;
};

// simple scoped timer
class UHGameTimerScope : public UHGameTimer
{
public:
	UHGameTimerScope(std::string InName, bool bPrintTimeAfterStop);
	~UHGameTimerScope();

	static const std::vector<std::pair<std::string, float>>& GetResiteredGameTime();
	static void ClearRegisteredGameTime();

private:
	bool bPrintTimeAfterStop;
	std::string Name;

#if WITH_EDITOR
	// editor only registered game time, which will be displayed in profile
	static std::vector<std::pair<std::string, float>> RegisteredGameTime;
#endif
};