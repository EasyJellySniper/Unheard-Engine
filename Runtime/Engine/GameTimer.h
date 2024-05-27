#pragma once
#include <cstdint>
#include <chrono>
#include <utility>
#include <vector>

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

// simple scoped timer
class UHGameTimerScope : public UHGameTimer
{
public:
	UHGameTimerScope(std::string InName, bool bPrintTimeAfterStop);
	~UHGameTimerScope();

	static std::vector<std::pair<std::string, float>> GetResiteredGameTime();
	static void ClearRegisteredGameTime();

private:
	bool bPrintTimeAfterStop;
	std::string Name;

#if WITH_EDITOR
	// editor only registered game time, which will be displayed in profile
	static std::vector<std::pair<std::string, float>> RegisteredGameTime;
#endif
};