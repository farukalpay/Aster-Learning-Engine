#pragma once
#if defined(__FreeBSD__)

#include "profile_runtime.config.h"
#if USE_PROFILE_RUNTIME

#include "profile_runtime_core.platform.h"

#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

namespace ProfileRuntime
{
	const char* Platform::GetName()
	{
		return "PS4";
	}

	ThreadID Platform::GetThreadID()
	{
		return (uint64_t)pthread_self();
	}

	ProcessID Platform::GetProcessID()
	{
		return (ProcessID)getpid();
	}

	int64 Platform::GetFrequency()
	{
		return 1000000000;
	}

	int64 Platform::GetTime()
	{
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		return ts.tv_sec * 1000000000LL + ts.tv_nsec;
	}

	Trace* Platform::CreateTrace()
	{
		return nullptr;
	}

	SymbolEngine* Platform::CreateSymbolEngine()
	{
		return nullptr;
	}
}

#endif //USE_PROFILE_RUNTIME
#endif //__FreeBSD__