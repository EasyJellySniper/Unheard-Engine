#include "Thread.h"
#include "../../UnheardEngine.h"

UHThread::UHThread()
	: bIsThreadDoneTask(true)
	, bIsThreadTerminated(false)
{

}

// begin thread, called during initialization, don't try to begin a new one every frame!
void UHThread::BeginThread(std::thread InObj, uint32_t AffinityCore)
{
	ThreadObj = std::move(InObj);

	// set affinity
	uint32_t NumCores = std::thread::hardware_concurrency();
	AffinityCore = AffinityCore % NumCores;

	DWORD_PTR Result = SetThreadAffinityMask(ThreadObj.native_handle(), DWORD_PTR(1) << AffinityCore);
	if (Result == 0)
	{
		UHE_LOG("Failed to set affinity for thread!\n");
	}
}

void UHThread::BeginThread(std::thread InObj)
{
	ThreadObj = std::move(InObj);
}

// end thread, this should force infinite wait loop to finish and terminate the thread
void UHThread::EndThread()
{
	if (ThreadObj.joinable())
	{
		bIsThreadDoneTask = false;
		bIsThreadTerminated = true;
		ThreadWaitTask.notify_one();
		ThreadObj.join();
	}
}

// wake this thread, the thread will kick off the task
void UHThread::WakeThread()
{
	std::unique_lock<std::mutex> Lock(ThreadMutex);
	bIsThreadDoneTask = false;
	ThreadWaitTask.notify_one();
}

// wait notify, usually called at the top of thread loop
void UHThread::WaitNotify()
{
	std::unique_lock<std::mutex> Lock(ThreadMutex);
	ThreadWaitTask.wait(Lock, [this] {return !bIsThreadDoneTask; });
}

// notify task is done
void UHThread::NotifyTaskDone()
{
	std::unique_lock<std::mutex> Lock(ThreadMutex);
	bIsThreadDoneTask = true;
	Lock.unlock();
	ThreadDoneTask.notify_one();
}

// wait the task of this thread finished
void UHThread::WaitTask()
{
	std::unique_lock<std::mutex> Lock(ThreadMutex);
	ThreadDoneTask.wait(Lock, [this] { return bIsThreadDoneTask; });
}

void UHThread::SetTerminate(bool InFlag)
{
	bIsThreadTerminated = InFlag;
}

bool UHThread::IsTermindate() const
{
	return bIsThreadTerminated;
}

std::mutex& UHThread::GetThreadMutex()
{
	return ThreadMutex;
}