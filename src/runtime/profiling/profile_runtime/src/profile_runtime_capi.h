#pragma once
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Config
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "profile_runtime.config.h"
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EXPORTS 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PROFILE_RUNTIME_EXPORTS) && defined(_MSC_VER)
#define PROFILE_RUNTIME_API __declspec(dllexport)
#else
#define PROFILE_RUNTIME_API 
#endif


#ifdef __cplusplus
extern "C" {
#endif

#if USE_PROFILE_RUNTIME
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_RegisterThread(const char* inThreadName, uint16_t inThreadNameLength);

	PROFILE_RUNTIME_API uint64_t ProfileRuntimeAPI_CreateEventDescription(const char* inFunctionName, uint16_t inFunctionLength, const char* inFileName, uint16_t inFileNameLenght, uint32_t inFileLine);
	PROFILE_RUNTIME_API uint64_t ProfileRuntimeAPI_PushEvent(uint64_t inEventDescription);
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_PopEvent(uint64_t inEventData);
	
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_NextFrame();

	PROFILE_RUNTIME_API void ProfileRuntimeAPI_StartCapture();
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_StopCapture(const char* inFileName, uint16_t inFileNameLength);

	PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_String(uint64_t inEventDescription, const char* inValue, uint16_t intValueLength);
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_Int32(uint64_t inEventDescription, int inValue);
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_Float(uint64_t inEventDescription, float inValue);
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_UInt32(uint64_t inEventDescription, uint32_t inValue);
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_UInt64(uint64_t inEventDescription, uint64_t inValue);
	PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_Point(uint64_t inEventDescription, float x, float y, float z);
#else

#define PROFILE_RUNTIME_CAPI_UNUSED(x) (void)(x)
	inline void ProfileRuntimeAPI_RegisterThread(const char* inThreadName, uint16_t inThreadNameLength) { PROFILE_RUNTIME_CAPI_UNUSED(inThreadName); PROFILE_RUNTIME_CAPI_UNUSED(inThreadNameLength); }
	inline uint64_t ProfileRuntimeAPI_CreateEventDescription(const char* inFunctionName, uint16_t inFunctionLength, const char* inFileName, uint16_t inFileNameLenght, uint32_t inFileLine) { PROFILE_RUNTIME_CAPI_UNUSED(inFunctionName); PROFILE_RUNTIME_CAPI_UNUSED(inFunctionLength); PROFILE_RUNTIME_CAPI_UNUSED(inFileName); PROFILE_RUNTIME_CAPI_UNUSED(inFileNameLenght); PROFILE_RUNTIME_CAPI_UNUSED(inFileLine); return 0; }
	inline uint64_t ProfileRuntimeAPI_PushEvent(uint64_t inEventDescription) { PROFILE_RUNTIME_CAPI_UNUSED(inEventDescription); return 0; }
	inline void ProfileRuntimeAPI_PopEvent(uint64_t inEventData) { PROFILE_RUNTIME_CAPI_UNUSED(inEventData); }
	inline void ProfileRuntimeAPI_NextFrame() {}
	inline void ProfileRuntimeAPI_StartCapture() {}
	inline void ProfileRuntimeAPI_StopCapture(const char* inFileName, uint16_t inFileNameLength) { PROFILE_RUNTIME_CAPI_UNUSED(inFileName); PROFILE_RUNTIME_CAPI_UNUSED(inFileNameLength); }
	inline void ProfileRuntimeAPI_AttachTag_String(uint64_t inEventDescription, const char* inValue, uint16_t intValueLength) { PROFILE_RUNTIME_CAPI_UNUSED(inEventDescription); PROFILE_RUNTIME_CAPI_UNUSED(inValue); PROFILE_RUNTIME_CAPI_UNUSED(intValueLength); }
	inline void ProfileRuntimeAPI_AttachTag_Int(uint64_t inEventDescription, int inValue) { PROFILE_RUNTIME_CAPI_UNUSED(inEventDescription); PROFILE_RUNTIME_CAPI_UNUSED(inValue); }
	inline void ProfileRuntimeAPI_AttachTag_Float(uint64_t inEventDescription, float inValue) { PROFILE_RUNTIME_CAPI_UNUSED(inEventDescription); PROFILE_RUNTIME_CAPI_UNUSED(inValue); }
	inline void ProfileRuntimeAPI_AttachTag_Int32(uint64_t inEventDescription, uint32_t inValue) { PROFILE_RUNTIME_CAPI_UNUSED(inEventDescription); PROFILE_RUNTIME_CAPI_UNUSED(inValue); }
	inline void ProfileRuntimeAPI_AttachTag_UInt64(uint64_t inEventDescription, uint64_t inValue) { PROFILE_RUNTIME_CAPI_UNUSED(inEventDescription); PROFILE_RUNTIME_CAPI_UNUSED(inValue); }
	inline void ProfileRuntimeAPI_AttachTag_Point(uint64_t inEventDescription, float x, float y, float z) { PROFILE_RUNTIME_CAPI_UNUSED(inEventDescription); PROFILE_RUNTIME_CAPI_UNUSED(x); PROFILE_RUNTIME_CAPI_UNUSED(y); PROFILE_RUNTIME_CAPI_UNUSED(z); }
#endif

#ifdef __cplusplus
} // extern "C"
#endif