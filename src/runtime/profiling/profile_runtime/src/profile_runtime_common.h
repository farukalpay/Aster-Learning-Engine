#pragma once

#include "profile_runtime.config.h"

#if USE_PROFILE_RUNTIME

#include "profile_runtime.h"

#include <cstdio>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(PROFILE_RUNTIME_MSVC)

#ifdef PROFILE_RUNTIME_UE4
#include "Core/Public/Windows/AllowWindowsPlatformTypes.h"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef PROFILE_RUNTIME_UE4
#include "Core/Public/Windows/HideWindowsPlatformTypes.h"
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif

namespace ProfileRuntime
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Types
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef signed char int8;
typedef unsigned char uint8;
typedef unsigned char byte;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
#if defined(PROFILE_RUNTIME_MSVC)
typedef __int64 int64;
typedef unsigned __int64 uint64;
#elif defined(PROFILE_RUNTIME_GCC)
typedef int64_t int64;
typedef uint64_t uint64;
#else
#error Compiler is not supported
#endif
static_assert(sizeof(int8) == 1, "Invalid type size, int8");
static_assert(sizeof(uint8) == 1, "Invalid type size, uint8");
static_assert(sizeof(byte) == 1, "Invalid type size, byte");
static_assert(sizeof(int16) == 2, "Invalid type size, int16");
static_assert(sizeof(uint16) == 2, "Invalid type size, uint16");
static_assert(sizeof(int32) == 4, "Invalid type size, int32");
static_assert(sizeof(uint32) == 4, "Invalid type size, uint32");
static_assert(sizeof(int64) == 8, "Invalid type size, int64");
static_assert(sizeof(uint64) == 8, "Invalid type size, uint64");
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef uint64 ThreadID;
static const ThreadID INVALID_THREAD_ID = (ThreadID)-1;
typedef uint32 ProcessID;
static const ProcessID INVALID_PROCESS_ID = (ProcessID)-1;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PROFILE_RUNTIME_MSVC)
#define PROFILE_RUNTIME_ALIGN(N) __declspec( align( N ) )
#elif defined(PROFILE_RUNTIME_GCC)
#define PROFILE_RUNTIME_ALIGN(N) __attribute__((aligned(N)))
#else
#error Can not define PROFILE_RUNTIME_ALIGN. Unknown platform.
#endif
#define PROFILE_RUNTIME_ARRAY_SIZE(ARR) (sizeof(ARR)/sizeof((ARR)[0]))
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PROFILE_RUNTIME_MSVC)
#define PROFILE_RUNTIME_NOINLINE __declspec(noinline)
#elif defined(PROFILE_RUNTIME_GCC)
#define PROFILE_RUNTIME_NOINLINE __attribute__((__noinline__))
#else
#error Compiler is not supported
#endif
////////////////////////////////////////////////////////////////////////
// PROFILE_RUNTIME_THREAD_LOCAL
////////////////////////////////////////////////////////////////////////
#if defined(PROFILE_RUNTIME_MSVC)
#define PROFILE_RUNTIME_THREAD_LOCAL __declspec(thread)
#elif defined(PROFILE_RUNTIME_GCC)
#define PROFILE_RUNTIME_THREAD_LOCAL __thread
#else
#error Can not define PROFILE_RUNTIME_THREAD_LOCAL. Unknown platform.
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Asserts
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PROFILE_RUNTIME_MSVC)
#define PROFILE_RUNTIME_DEBUG_BREAK __debugbreak()
#elif defined(PROFILE_RUNTIME_GCC)
#define PROFILE_RUNTIME_DEBUG_BREAK __builtin_trap()
#else
	#error Can not define PROFILE_RUNTIME_DEBUG_BREAK. Unknown platform.
#endif
#define PROFILE_RUNTIME_UNUSED(x) (void)(x)
#ifdef _DEBUG
	#define PROFILE_RUNTIME_ASSERT(arg, description) if (!(arg)) { PROFILE_RUNTIME_DEBUG_BREAK; }
	#define PROFILE_RUNTIME_FAILED(description) { PROFILE_RUNTIME_DEBUG_BREAK; }
#else
	#define PROFILE_RUNTIME_ASSERT(arg, description)
	#define PROFILE_RUNTIME_FAILED(description)
#endif
#define PROFILE_RUNTIME_VERIFY(arg, description, operation) if (!(arg)) { PROFILE_RUNTIME_DEBUG_BREAK; operation; }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Safe functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PROFILE_RUNTIME_LINUX) || defined(PROFILE_RUNTIME_OSX)
template<size_t sizeOfBuffer>
inline int sprintf_s(char(&buffer)[sizeOfBuffer], const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	int result = vsnprintf(buffer, sizeOfBuffer, format, ap);
	va_end(ap);
	return result;
}
#endif

#if defined(PROFILE_RUNTIME_GCC)
#include <string.h>
template<size_t sizeOfBuffer>
inline int wcstombs_s(char(&buffer)[sizeOfBuffer], const wchar_t* src, size_t maxCount)
{
	return wcstombs(buffer, src, maxCount);
}
#endif

#if defined(PROFILE_RUNTIME_MSVC)
template<size_t sizeOfBuffer>
inline int wcstombs_s(char(&buffer)[sizeOfBuffer], const wchar_t* src, size_t maxCount)
{
	size_t converted = 0;
	return ::wcstombs_s(&converted, buffer, src, maxCount);
}
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //USE_PROFILE_RUNTIME
