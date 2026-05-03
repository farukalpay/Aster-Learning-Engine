#include "profile_runtime_capi.h"

#if USE_PROFILE_RUNTIME

#include "profile_runtime_core.h"

#if defined(__MACH__)
#include <stdlib.h>
#else 
#include <malloc.h>
#endif
#include <string.h>

PROFILE_RUNTIME_API void ProfileRuntimeAPI_RegisterThread(const char* inThreadName, uint16_t inThreadNameLength)
{
	ProfileRuntime::ProfileRuntimeString<256> threadName(inThreadName, inThreadNameLength);
	ProfileRuntime::RegisterThread(threadName.data);
}

PROFILE_RUNTIME_API uint64_t ProfileRuntimeAPI_CreateEventDescription(const char* inFunctionName, uint16_t inFunctionLength, const char* inFileName, uint16_t inFileNameLenght, uint32_t inFileLine)
{
	ProfileRuntime::ProfileRuntimeString<128> name(inFunctionName, inFunctionLength);
	ProfileRuntime::ProfileRuntimeString<256> file(inFileName, inFileNameLenght);
	uint8_t flags = ProfileRuntime::EventDescription::COPY_NAME_STRING | ProfileRuntime::EventDescription::COPY_FILENAME_STRING | ProfileRuntime::EventDescription::IS_CUSTOM_NAME;
	return (uint64_t)::ProfileRuntime::CreateDescription(name.data, file.data, inFileLine, nullptr, ProfileRuntime::Category::None, flags);
}
PROFILE_RUNTIME_API uint64_t ProfileRuntimeAPI_PushEvent(uint64_t inEventDescription)
{
	return (uint64_t)ProfileRuntime::Event::Start(*((ProfileRuntime::EventDescription*)inEventDescription));
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_PopEvent(uint64_t inEventData)
{
	ProfileRuntime::Event::Stop(*((ProfileRuntime::EventData*)inEventData));
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_NextFrame()
{
	ProfileRuntime::Event::Pop();
	ProfileRuntime::EndFrame();
	ProfileRuntime::Update();
	ProfileRuntime::BeginFrame();
	ProfileRuntime::Event::Push(*ProfileRuntime::GetFrameDescription());
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_StartCapture()
{
	ProfileRuntime::StartCapture();
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_StopCapture(const char* inFileName, uint16_t inFileNameLength)
{
	ProfileRuntime::ProfileRuntimeString<256> fileName(inFileName, inFileNameLength);
	ProfileRuntime::StopCapture();
	ProfileRuntime::SaveCapture(fileName.data);
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_String(uint64_t inEventDescription, const char* inValue, uint16_t inValueLength)
{
	ProfileRuntime::Tag::Attach(*(ProfileRuntime::EventDescription*)inEventDescription, inValue, inValueLength);
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_Int32(uint64_t inEventDescription, int32_t inValue)
{
	ProfileRuntime::Tag::Attach(*(ProfileRuntime::EventDescription*)inEventDescription, inValue);
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_Float(uint64_t inEventDescription, float inValue)
{
	ProfileRuntime::Tag::Attach(*(ProfileRuntime::EventDescription*)inEventDescription, inValue);
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_UInt32(uint64_t inEventDescription, uint32_t inValue)
{
	ProfileRuntime::Tag::Attach(*(ProfileRuntime::EventDescription*)inEventDescription, inValue);
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_UInt64(uint64_t inEventDescription, uint64_t inValue)
{
	ProfileRuntime::Tag::Attach(*(ProfileRuntime::EventDescription*)inEventDescription, inValue);
}

PROFILE_RUNTIME_API void ProfileRuntimeAPI_AttachTag_Point(uint64_t inEventDescription, float x, float y, float z)
{
	ProfileRuntime::Tag::Attach(*(ProfileRuntime::EventDescription*)inEventDescription, x, y, z);
}

#endif //USE_PROFILE_RUNTIME
