#include "GameTimer.h"
#include "framework.h"
#include "UnheardEngine.h"

#if WITH_EDITOR
std::mutex GTimeScopeLock;
std::vector<std::pair<std::string, float>> UHGameTimerScope::RegisteredGameTime;
#endif

UHGameTimer::UHGameTimer()
	: DeltaTime(0.0)
	, BaseTimePoint(UHClock::time_point())
	, PausedTimePoint(UHClock::time_point())
	, StopTimePoint(UHClock::time_point())
	, PreviousTimePoint(UHClock::time_point())
	, CurrentTimePoint(UHClock::time_point())
	, bStopped(false)
{

}

// Returns the total time elapsed since Reset() was called, NOT counting any
// time when the clock is stopped.
float UHGameTimer::GetTotalTime() const
{
	if (bStopped)
	{
		auto D0 = StopTimePoint - PausedTimePoint;
		auto D1 = PausedTimePoint - BaseTimePoint;

		return std::chrono::duration<float>(D0 + D1).count();
	}

	auto D0 = CurrentTimePoint - PausedTimePoint;
	auto D1 = PausedTimePoint - BaseTimePoint;
	return std::chrono::duration<float>(D0 + D1).count();
}

float UHGameTimer::GetDeltaTime() const
{
	return static_cast<float>(DeltaTime);
}

// get current time, GetCurrentTime() will conflict to other function, so I name it GetTime()
UHClock::time_point UHGameTimer::GetTime() const
{
	return UHClock::now();
}

void UHGameTimer::Reset()
{
	BaseTimePoint = UHClock::now();
	PreviousTimePoint = BaseTimePoint;
	StopTimePoint = UHClock::time_point();
	bStopped = false;
}

void UHGameTimer::Start()
{
	// Accumulate the time elapsed between stop and start pairs
	if (bStopped)
	{
		UHClock::time_point StartTime = UHClock::now();
		PausedTimePoint += StartTime - StopTimePoint;
		PreviousTimePoint = StartTime;
		StopTimePoint = UHClock::time_point();
		bStopped = false;
	}
}

void UHGameTimer::Stop()
{
	if (!bStopped)
	{
		StopTimePoint = UHClock::now();
		bStopped = true;
	}
}

void UHGameTimer::Tick()
{
	if (bStopped)
	{
		DeltaTime = 0.0;
		return;
	}

	CurrentTimePoint = UHClock::now();

	// Time difference between this frame and the previous.
	DeltaTime = std::chrono::duration<double>(CurrentTimePoint - PreviousTimePoint).count();

	// Prepare for next frame.
	PreviousTimePoint = CurrentTimePoint;

	if (DeltaTime < 0.0)
	{
		DeltaTime = 0.0;
	}

	// check if the true delta time is larger than a threshold
	// this can happen with debug breakpoint or some other pausing mechanisms, correct it to a constant rate
	if (DeltaTime > 1.0)
	{
		// fix to 60hz should be fine for now, follow the target FPS in the future if needed
		double DesiredDeltaTime = 0.01666666666666666666666666666667;
		DeltaTime = DesiredDeltaTime;
	}
}

// UHGameTimerScope
UHGameTimerScope::UHGameTimerScope(std::string InName, bool bPrintTimeAfterStop)
{
#if WITH_EDITOR
	this->bPrintTimeAfterStop = bPrintTimeAfterStop;
	Name = InName;
	Reset();
	Start();
#endif
}

UHGameTimerScope::~UHGameTimerScope()
{
#if WITH_EDITOR
	Stop();

	const float TotalTime = GetTotalTime() * 1000.0f;
	if (bPrintTimeAfterStop)
	{
		UHE_LOG(Name + " takes " + std::to_string(TotalTime) + " ms.\n");
	}

	std::unique_lock<std::mutex> Lock(GTimeScopeLock);
	RegisteredGameTime.push_back(std::make_pair(Name, TotalTime));
#endif
}

#if WITH_EDITOR
const std::vector<std::pair<std::string, float>>& UHGameTimerScope::GetResiteredGameTime()
{
	std::unique_lock<std::mutex> Lock(GTimeScopeLock);
	return RegisteredGameTime;
}

void UHGameTimerScope::ClearRegisteredGameTime()
{
	std::unique_lock<std::mutex> Lock(GTimeScopeLock);
	RegisteredGameTime.clear();
}
#endif