#pragma once
#include <thread>
#include <mutex>

// UH thread wrapper
class UHThread
{
public:
	UHThread();
	~UHThread();

	// kick off a thread with or without affinity setting
	void BeginThread(std::thread InObj, uint32_t AffinityCore);
	void BeginThread(std::thread InObj);

	// end a thread
	void EndThread();

	// wake this thread
	void WakeThread();

	// wait for notify
	void WaitNotify();

	// notify when this thread done the task
	void NotifyTaskDone();

	// wait the task of this thread
	void WaitTask();

	// termination control
	void SetTerminate(bool InFlag);
	bool IsTermindate() const;
	std::mutex& GetThreadMutex();

private:
	std::thread ThreadObj;
	std::condition_variable ThreadWaitTask;
	std::condition_variable ThreadDoneTask;
	std::mutex ThreadMutex;

	bool bIsThreadDoneTask;
	bool bIsThreadTerminated;
};