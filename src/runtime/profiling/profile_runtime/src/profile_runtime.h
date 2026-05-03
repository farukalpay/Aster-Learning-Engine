#pragma once
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Config
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "profile_runtime.config.h"

#if USE_PROFILE_RUNTIME
#include <stdint.h>
#include <stddef.h>

#if defined(_MSC_VER)
#	define PROFILE_RUNTIME_MSVC (1)
#	define PROFILE_RUNTIME_64BIT (1)
#	if defined(_DURANGO)
#		define PROFILE_RUNTIME_PC (0)
#	else
#		define PROFILE_RUNTIME_PC (1)
#   endif
#elif defined(__clang__) || defined(__GNUC__)
#	define PROFILE_RUNTIME_GCC (1)
#	if defined(__APPLE_CC__)
#		define PROFILE_RUNTIME_OSX (1)
#		define PROFILE_RUNTIME_64BIT (1)
#	elif defined(__linux__)
#		define PROFILE_RUNTIME_LINUX (1)
#		define PROFILE_RUNTIME_64BIT (1)
#	elif defined(__FreeBSD__)
#		define PROFILE_RUNTIME_FREEBSD (1)
#		define PROFILE_RUNTIME_64BIT (1)
#	endif
#	if defined(__aarch64__) || defined(_M_ARM64)
#		define PROFILE_RUNTIME_ARM (1)
#		define PROFILE_RUNTIME_64BIT (1)
#	elif defined(__arm__) || defined(_M_ARM)
#		define PROFILE_RUNTIME_ARM (1)
#		define PROFILE_RUNTIME_32BIT (1)
#	endif
#else
#error Compiler not supported
#endif

////////////////////////////////////////////////////////////////////////
// Target Platform
////////////////////////////////////////////////////////////////////////

#if defined(PROFILE_RUNTIME_GCC)
#define PROFILE_RUNTIME_FUNC __PRETTY_FUNCTION__
#elif defined(PROFILE_RUNTIME_MSVC)
#define PROFILE_RUNTIME_FUNC __FUNCSIG__
#else
#error Compiler not supported
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EXPORTS 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(PROFILE_RUNTIME_EXPORTS) && defined(PROFILE_RUNTIME_MSVC)
#define PROFILE_RUNTIME_API __declspec(dllexport)
#else
#define PROFILE_RUNTIME_API 
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define PROFILE_RUNTIME_CONCAT_IMPL(x, y) x##y
#define PROFILE_RUNTIME_CONCAT(x, y) PROFILE_RUNTIME_CONCAT_IMPL(x, y)

#if defined(PROFILE_RUNTIME_MSVC)
#define PROFILE_RUNTIME_INLINE __forceinline
#elif defined(PROFILE_RUNTIME_GCC)
#define PROFILE_RUNTIME_INLINE __attribute__((always_inline)) inline
#else
#error Compiler is not supported
#endif


// Vulkan Forward Declarations
#define PROFILE_RUNTIME_DEFINE_HANDLE(object) typedef struct object##_T *object;
PROFILE_RUNTIME_DEFINE_HANDLE(VkDevice);
PROFILE_RUNTIME_DEFINE_HANDLE(VkPhysicalDevice);
PROFILE_RUNTIME_DEFINE_HANDLE(VkQueue);
PROFILE_RUNTIME_DEFINE_HANDLE(VkCommandBuffer);
PROFILE_RUNTIME_DEFINE_HANDLE(VkQueryPool);
PROFILE_RUNTIME_DEFINE_HANDLE(VkCommandPool);
PROFILE_RUNTIME_DEFINE_HANDLE(VkFence);

struct VkPhysicalDeviceProperties;
struct VkQueryPoolCreateInfo;
struct VkAllocationCallbacks;
struct VkCommandPoolCreateInfo;
struct VkCommandBufferAllocateInfo;
struct VkFenceCreateInfo;
struct VkSubmitInfo;
struct VkCommandBufferBeginInfo;

#ifndef VKAPI_PTR
#define PROFILE_RUNTIME_VKAPI_PTR_DEFINED 1
#if defined(_WIN32)
    // On Windows, Vulkan commands use the stdcall convention
	#define VKAPI_PTR  __stdcall
#else
	#define VKAPI_PTR 
#endif
#endif

typedef void (VKAPI_PTR *PFN_vkGetPhysicalDeviceProperties_)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
typedef int32_t (VKAPI_PTR *PFN_vkCreateQueryPool_)(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool);
typedef int32_t (VKAPI_PTR *PFN_vkCreateCommandPool_)(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool);
typedef int32_t (VKAPI_PTR *PFN_vkAllocateCommandBuffers_)(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers);
typedef int32_t (VKAPI_PTR *PFN_vkCreateFence_)(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence);
typedef void (VKAPI_PTR *PFN_vkCmdResetQueryPool_)(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);
typedef int32_t (VKAPI_PTR *PFN_vkQueueSubmit_)(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
typedef int32_t (VKAPI_PTR *PFN_vkWaitForFences_)(VkDevice device, uint32_t fenceCount, const VkFence* pFences, uint32_t waitAll, uint64_t timeout);
typedef int32_t (VKAPI_PTR *PFN_vkResetCommandBuffer_)(VkCommandBuffer commandBuffer, uint32_t flags);
typedef void (VKAPI_PTR *PFN_vkCmdWriteTimestamp_)(VkCommandBuffer commandBuffer, uint32_t pipelineStage, VkQueryPool queryPool, uint32_t query);
typedef int32_t (VKAPI_PTR *PFN_vkGetQueryPoolResults_)(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, uint64_t stride, uint32_t flags);
typedef int32_t (VKAPI_PTR *PFN_vkBeginCommandBuffer_)(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo);
typedef int32_t (VKAPI_PTR *PFN_vkEndCommandBuffer_)(VkCommandBuffer commandBuffer);
typedef int32_t (VKAPI_PTR *PFN_vkResetFences_)(VkDevice device, uint32_t fenceCount, const VkFence* pFences);
typedef void (VKAPI_PTR *PFN_vkDestroyCommandPool_)(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator);
typedef void (VKAPI_PTR *PFN_vkDestroyQueryPool_)(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator);
typedef void (VKAPI_PTR *PFN_vkDestroyFence_)(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator);
typedef void (VKAPI_PTR *PFN_vkFreeCommandBuffers_)(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers);

#if PROFILE_RUNTIME_VKAPI_PTR_DEFINED
#undef VKAPI_PTR
#endif

// D3D12 Forward Declarations
struct ID3D12CommandList;
struct ID3D12Device;
struct ID3D12CommandQueue;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ProfileRuntime
{
	struct PROFILE_RUNTIME_API VulkanFunctions
	{
		PFN_vkGetPhysicalDeviceProperties_ vkGetPhysicalDeviceProperties;
		PFN_vkCreateQueryPool_ vkCreateQueryPool;
		PFN_vkCreateCommandPool_ vkCreateCommandPool;
		PFN_vkAllocateCommandBuffers_ vkAllocateCommandBuffers;
		PFN_vkCreateFence_ vkCreateFence;
		PFN_vkCmdResetQueryPool_ vkCmdResetQueryPool;
		PFN_vkQueueSubmit_ vkQueueSubmit;
		PFN_vkWaitForFences_ vkWaitForFences;
		PFN_vkResetCommandBuffer_ vkResetCommandBuffer;
		PFN_vkCmdWriteTimestamp_ vkCmdWriteTimestamp;
		PFN_vkGetQueryPoolResults_ vkGetQueryPoolResults;
		PFN_vkBeginCommandBuffer_ vkBeginCommandBuffer;
		PFN_vkEndCommandBuffer_ vkEndCommandBuffer;
		PFN_vkResetFences_ vkResetFences;
		PFN_vkDestroyCommandPool_ vkDestroyCommandPool;
		PFN_vkDestroyQueryPool_ vkDestroyQueryPool;
		PFN_vkDestroyFence_ vkDestroyFence;
		PFN_vkFreeCommandBuffers_ vkFreeCommandBuffers;
	};

	// Source: http://msdn.microsoft.com/en-us/library/system.windows.media.colors(v=vs.110).aspx
	// Image:  http://i.msdn.microsoft.com/dynimg/IC24340.png
	struct Color
	{
		enum
		{
			Null = 0x00000000,
			AliceBlue = 0xFFF0F8FF,
			AntiqueWhite = 0xFFFAEBD7,
			Aqua = 0xFF00FFFF,
			Aquamarine = 0xFF7FFFD4,
			Azure = 0xFFF0FFFF,
			Beige = 0xFFF5F5DC,
			Bisque = 0xFFFFE4C4,
			Black = 0xFF000000,
			BlanchedAlmond = 0xFFFFEBCD,
			Blue = 0xFF0000FF,
			BlueViolet = 0xFF8A2BE2,
			Brown = 0xFFA52A2A,
			BurlyWood = 0xFFDEB887,
			CadetBlue = 0xFF5F9EA0,
			Chartreuse = 0xFF7FFF00,
			Chocolate = 0xFFD2691E,
			Coral = 0xFFFF7F50,
			CornflowerBlue = 0xFF6495ED,
			Cornsilk = 0xFFFFF8DC,
			Crimson = 0xFFDC143C,
			Cyan = 0xFF00FFFF,
			DarkBlue = 0xFF00008B,
			DarkCyan = 0xFF008B8B,
			DarkGoldenRod = 0xFFB8860B,
			DarkGray = 0xFFA9A9A9,
			DarkGreen = 0xFF006400,
			DarkKhaki = 0xFFBDB76B,
			DarkMagenta = 0xFF8B008B,
			DarkOliveGreen = 0xFF556B2F,
			DarkOrange = 0xFFFF8C00,
			DarkOrchid = 0xFF9932CC,
			DarkRed = 0xFF8B0000,
			DarkSalmon = 0xFFE9967A,
			DarkSeaGreen = 0xFF8FBC8F,
			DarkSlateBlue = 0xFF483D8B,
			DarkSlateGray = 0xFF2F4F4F,
			DarkTurquoise = 0xFF00CED1,
			DarkViolet = 0xFF9400D3,
			DeepPink = 0xFFFF1493,
			DeepSkyBlue = 0xFF00BFFF,
			DimGray = 0xFF696969,
			DodgerBlue = 0xFF1E90FF,
			FireBrick = 0xFFB22222,
			FloralWhite = 0xFFFFFAF0,
			ForestGreen = 0xFF228B22,
			Fuchsia = 0xFFFF00FF,
			Gainsboro = 0xFFDCDCDC,
			GhostWhite = 0xFFF8F8FF,
			Gold = 0xFFFFD700,
			GoldenRod = 0xFFDAA520,
			Gray = 0xFF808080,
			Green = 0xFF008000,
			GreenYellow = 0xFFADFF2F,
			HoneyDew = 0xFFF0FFF0,
			HotPink = 0xFFFF69B4,
			IndianRed = 0xFFCD5C5C,
			Indigo = 0xFF4B0082,
			Ivory = 0xFFFFFFF0,
			Khaki = 0xFFF0E68C,
			Lavender = 0xFFE6E6FA,
			LavenderBlush = 0xFFFFF0F5,
			LawnGreen = 0xFF7CFC00,
			LemonChiffon = 0xFFFFFACD,
			LightBlue = 0xFFADD8E6,
			LightCoral = 0xFFF08080,
			LightCyan = 0xFFE0FFFF,
			LightGoldenRodYellow = 0xFFFAFAD2,
			LightGray = 0xFFD3D3D3,
			LightGreen = 0xFF90EE90,
			LightPink = 0xFFFFB6C1,
			LightSalmon = 0xFFFFA07A,
			LightSeaGreen = 0xFF20B2AA,
			LightSkyBlue = 0xFF87CEFA,
			LightSlateGray = 0xFF778899,
			LightSteelBlue = 0xFFB0C4DE,
			LightYellow = 0xFFFFFFE0,
			Lime = 0xFF00FF00,
			LimeGreen = 0xFF32CD32,
			Linen = 0xFFFAF0E6,
			Magenta = 0xFFFF00FF,
			Maroon = 0xFF800000,
			MediumAquaMarine = 0xFF66CDAA,
			MediumBlue = 0xFF0000CD,
			MediumOrchid = 0xFFBA55D3,
			MediumPurple = 0xFF9370DB,
			MediumSeaGreen = 0xFF3CB371,
			MediumSlateBlue = 0xFF7B68EE,
			MediumSpringGreen = 0xFF00FA9A,
			MediumTurquoise = 0xFF48D1CC,
			MediumVioletRed = 0xFFC71585,
			MidnightBlue = 0xFF191970,
			MintCream = 0xFFF5FFFA,
			MistyRose = 0xFFFFE4E1,
			Moccasin = 0xFFFFE4B5,
			NavajoWhite = 0xFFFFDEAD,
			Navy = 0xFF000080,
			OldLace = 0xFFFDF5E6,
			Olive = 0xFF808000,
			OliveDrab = 0xFF6B8E23,
			Orange = 0xFFFFA500,
			OrangeRed = 0xFFFF4500,
			Orchid = 0xFFDA70D6,
			PaleGoldenRod = 0xFFEEE8AA,
			PaleGreen = 0xFF98FB98,
			PaleTurquoise = 0xFFAFEEEE,
			PaleVioletRed = 0xFFDB7093,
			PapayaWhip = 0xFFFFEFD5,
			PeachPuff = 0xFFFFDAB9,
			Peru = 0xFFCD853F,
			Pink = 0xFFFFC0CB,
			Plum = 0xFFDDA0DD,
			PowderBlue = 0xFFB0E0E6,
			Purple = 0xFF800080,
			Red = 0xFFFF0000,
			RosyBrown = 0xFFBC8F8F,
			RoyalBlue = 0xFF4169E1,
			SaddleBrown = 0xFF8B4513,
			Salmon = 0xFFFA8072,
			SandyBrown = 0xFFF4A460,
			SeaGreen = 0xFF2E8B57,
			SeaShell = 0xFFFFF5EE,
			Sienna = 0xFFA0522D,
			Silver = 0xFFC0C0C0,
			SkyBlue = 0xFF87CEEB,
			SlateBlue = 0xFF6A5ACD,
			SlateGray = 0xFF708090,
			Snow = 0xFFFFFAFA,
			SpringGreen = 0xFF00FF7F,
			SteelBlue = 0xFF4682B4,
			Tan = 0xFFD2B48C,
			Teal = 0xFF008080,
			Thistle = 0xFFD8BFD8,
			Tomato = 0xFFFF6347,
			Turquoise = 0xFF40E0D0,
			Violet = 0xFFEE82EE,
			Wheat = 0xFFF5DEB3,
			White = 0xFFFFFFFF,
			WhiteSmoke = 0xFFF5F5F5,
			Yellow = 0xFFFFFF00,
			YellowGreen = 0xFF9ACD32,
		};
	};

	struct Filter
	{
		enum Type : uint32_t
		{
			None,
			
			// CPU
			AI,
			Animation, 
			Audio,
			Debug,
			Camera,
			Cloth,
			GameLogic,
			Input,
			Navigation,
			Network,
			Physics,
			Rendering,
			Scene,
			Script,
			Streaming,
			UI,
			VFX,
			Visibility,
			Wait,

			// IO
			IO,

			// GPU
			GPU_Cloth,
			GPU_Lighting,
			GPU_PostFX,
			GPU_Reflections,
			GPU_Scene,
			GPU_Shadows,
			GPU_UI,
			GPU_VFX,
			GPU_Water,

		};
	};

	#define PROFILE_RUNTIME_MAKE_CATEGORY(filter, color) ((ProfileRuntime::Category::Type)(((uint64_t)(1ull) << (filter + 32)) | (uint64_t)color))

	struct Category
	{
		enum Type : uint64_t
		{
			// CPU
			None			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::None, Color::Null),
			AI				= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::AI, Color::Purple),
			Animation		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Animation, Color::LightSkyBlue),
			Audio			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Audio, Color::HotPink),
			Debug			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Debug, Color::Black),
			Camera			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Camera, Color::Black),
			Cloth			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Cloth, Color::DarkGreen),
			GameLogic		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GameLogic, Color::RoyalBlue),
			Input			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Input, Color::Ivory),
			Navigation		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Navigation, Color::Magenta),
			Network			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Network, Color::Olive),
			Physics			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Physics, Color::LawnGreen),
			Rendering		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Rendering, Color::BurlyWood),
			Scene			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Scene, Color::RoyalBlue),
			Script			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Script, Color::Plum),
			Streaming		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Streaming, Color::Gold),
			UI				= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::UI, Color::PaleTurquoise),
			VFX				= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::VFX, Color::SaddleBrown),
			Visibility		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Visibility, Color::Snow),
			Wait			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Wait, Color::Tomato),
			WaitEmpty		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::Wait, Color::White),
			// IO
			IO				= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::IO, Color::Khaki),
			// GPU
			GPU_Cloth		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_Cloth, Color::DarkGreen),
			GPU_Lighting	= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_Lighting, Color::Khaki),
			GPU_PostFX		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_PostFX, Color::Maroon),
			GPU_Reflections = PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_Reflections, Color::CadetBlue),
			GPU_Scene		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_Scene, Color::RoyalBlue),
			GPU_Shadows		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_Shadows, Color::LightSlateGray),
			GPU_UI			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_UI, Color::PaleTurquoise),
			GPU_VFX			= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_VFX, Color::SaddleBrown),
			GPU_Water		= PROFILE_RUNTIME_MAKE_CATEGORY(Filter::GPU_Water, Color::SteelBlue),
		};

		static uint32_t GetMask(Type t) { return (uint32_t)(t >> 32); }
		static uint32_t GetColor(Type t) { return (uint32_t)(t); }
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}


namespace ProfileRuntime
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Mode
{
	enum Type
	{
		// OFF
		OFF = 0x0,
		// Collect Categories (top-level events)
		INSTRUMENTATION_CATEGORIES = (1 << 0),
		// Collect Events
		INSTRUMENTATION_EVENTS = (1 << 1),
		// Collect Events + Categories
		INSTRUMENTATION = (INSTRUMENTATION_CATEGORIES | INSTRUMENTATION_EVENTS),
		// Legacy (keep for compatibility reasons)
		SAMPLING = (1 << 2),
		// Collect Data Tags
		TAGS = (1 << 3),
		// Enable Autosampling Events (automatic callstacks)
		AUTOSAMPLING = (1 << 4),
		// Enable Switch-Contexts Events
		SWITCH_CONTEXT = (1 << 5),
		// Collect I/O Events
		IO = (1 << 6),
		// Collect GPU Events
		GPU = (1 << 7),
		END_SCREENSHOT = (1 << 8),
		RESERVED_0 = (1 << 9),
		RESERVED_1 = (1 << 10),
		// Collect HW Events
		HW_COUNTERS = (1 << 11),
		// Collect Events in Live mode
		LIVE = (1 << 12),
		RESERVED_2 = (1 << 13),
		RESERVED_3 = (1 << 14),
		RESERVED_4 = (1 << 15),
		// Collect System Calls
		SYS_CALLS = (1 << 16),
		// Collect Events from Other Processes
		OTHER_PROCESSES = (1 << 17),
		// Automation
		NOGUI = (1 << 18),

		TRACER = AUTOSAMPLING | SWITCH_CONTEXT | SYS_CALLS,
		DEFAULT = INSTRUMENTATION | TAGS | AUTOSAMPLING | SWITCH_CONTEXT | IO | GPU | SYS_CALLS | OTHER_PROCESSES,
	};
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct FrameType
{
	enum Type
	{
		CPU,
		GPU,
		Render,
		COUNT,

		NONE = -1,
	};
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PROFILE_RUNTIME_API int64_t GetHighPrecisionTime();
PROFILE_RUNTIME_API int64_t GetHighPrecisionFrequency();
PROFILE_RUNTIME_API void Update();
PROFILE_RUNTIME_API uint32_t BeginFrame(FrameType::Type type = FrameType::CPU, int64_t timestamp = -1, uint64_t threadID = (uint64_t)-1);
PROFILE_RUNTIME_API uint32_t EndFrame(FrameType::Type type = FrameType::CPU, int64_t timestamp = -1, uint64_t threadID = (uint64_t)-1);
PROFILE_RUNTIME_API bool IsActive(Mode::Type mode = Mode::INSTRUMENTATION_EVENTS);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct EventStorage;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PROFILE_RUNTIME_API bool RegisterFiber(uint64_t fiberId, EventStorage** slot);
PROFILE_RUNTIME_API bool RegisterThread(const char* name);
PROFILE_RUNTIME_API bool RegisterThread(const wchar_t* name);
PROFILE_RUNTIME_API bool UnRegisterThread(bool keepAlive);
PROFILE_RUNTIME_API EventStorage** GetEventStorageSlotForCurrentThread();
PROFILE_RUNTIME_API bool IsFiberStorage(EventStorage* fiberStorage);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ThreadMask
{
	enum Type
	{
		None	= 0,
		Main	= 1 << 0,
		GPU		= 1 << 1,
		IO		= 1 << 2,
		Idle	= 1 << 3,
		Render  = 1 << 4,
	};
};

PROFILE_RUNTIME_API EventStorage* RegisterStorage(const char* name, uint64_t threadID = uint64_t(-1), ThreadMask::Type type = ThreadMask::None);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct State
{
	enum Type
	{
		// Starting a new capture
		START_CAPTURE,

		// Stopping current capture
		STOP_CAPTURE,

		// Dumping capture to the GUI
		// Useful for attaching summary and screenshot to the capture
		DUMP_CAPTURE,

		// Cancel current capture
		CANCEL_CAPTURE,
	};
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sets a state change callback
typedef bool (*StateCallback)(State::Type state);
PROFILE_RUNTIME_API bool SetStateChangedCallback(StateCallback cb);

// Attaches a key-value pair to the capture's summary
// Example: AttachSummary("Version", "v12.0.1");
//			AttachSummary("Platform", "Windows");
//			AttachSummary("Config", "Release_x64");
//			AttachSummary("Settings", "Ultra");
//			AttachSummary("Map", "Atlantida");
//			AttachSummary("Position", "123.0,120.0,41.1");
//			AttachSummary("CPU", "Intel(R) Xeon(R) CPU E5410@2.33GHz");
//			AttachSummary("GPU", "NVIDIA GeForce GTX 980 Ti");
PROFILE_RUNTIME_API bool AttachSummary(const char* key, const char* value);

struct File
{
	enum Type
	{
		// Supported formats: PNG, JPEG, BMP, TIFF
		PROFILE_RUNTIME_IMAGE,
		
		// Text file
		PROFILE_RUNTIME_TEXT,

		// Any other type
		PROFILE_RUNTIME_OTHER,
	};
};
// Attaches a file to the current capture
PROFILE_RUNTIME_API bool AttachFile(File::Type type, const char* name, const uint8_t* data, uint32_t size);
PROFILE_RUNTIME_API bool AttachFile(File::Type type, const char* name, const char* path);
PROFILE_RUNTIME_API bool AttachFile(File::Type type, const char* name, const wchar_t* path);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct EventDescription;
struct Frame;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct EventTime
{
	static const int64_t INVALID_TIMESTAMP = (int64_t)-1;

	int64_t start;
	int64_t finish;

	PROFILE_RUNTIME_INLINE void Start() { start  = ProfileRuntime::GetHighPrecisionTime(); }
	PROFILE_RUNTIME_INLINE void Stop() 	{ finish = ProfileRuntime::GetHighPrecisionTime(); }
	PROFILE_RUNTIME_INLINE bool IsValid() const { return start < finish && start != INVALID_TIMESTAMP && finish != INVALID_TIMESTAMP;  }
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct EventData : public EventTime
{
	const EventDescription* description;

	bool operator<(const EventData& other) const
	{
		if (start != other.start)
			return start < other.start;

		// Reversed order for finish intervals (parent first)
		return  finish > other.finish;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PROFILE_RUNTIME_API SyncData : public EventTime
{
	uint64_t newThreadId;
	uint64_t oldThreadId;
	uint8_t core;
	int8_t reason;
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PROFILE_RUNTIME_API FiberSyncData : public EventTime
{
	uint64_t threadId;

	static void AttachToThread(EventStorage* storage, uint64_t threadId);
	static void DetachFromThread(EventStorage* storage);
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct TagData
{
	const EventDescription* description;
	int64_t timestamp;
	T data;
	TagData() {}
	TagData(const EventDescription& desc, T d) : description(&desc), timestamp(ProfileRuntime::GetHighPrecisionTime()), data(d) {}
	TagData(const EventDescription& desc, T d, int64_t t) : description(&desc), timestamp(t), data(d) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PROFILE_RUNTIME_API EventDescription
{
	enum Flags : uint8_t
	{
		IS_CUSTOM_NAME = 1 << 0,
		COPY_NAME_STRING = 1 << 1,
		COPY_FILENAME_STRING = 1 << 2,
	};

	const char* name;
	const char* file;
	uint32_t line;
	uint32_t index;
	uint32_t color;
	uint32_t filter;
	uint8_t flags;

	static EventDescription* Create(const char* eventName, const char* fileName, const unsigned long fileLine, const unsigned long eventColor = Color::Null, const unsigned long filter = 0, const uint8_t eventFlags = 0);
	static EventDescription* CreateShared(const char* eventName, const char* fileName = nullptr, const unsigned long fileLine = 0, const unsigned long eventColor = Color::Null, const unsigned long filter = 0);

	EventDescription();
private:
	friend class EventDescriptionBoard;
	EventDescription& operator=(const EventDescription&);
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PROFILE_RUNTIME_API Event
{
	EventData* data;

	static EventData* Start(const EventDescription& description);
	static void Stop(EventData& data);

	static void Push(const char* name);
	static void Push(const EventDescription& description);
	static void Pop();

	static void Add(EventStorage* storage, const EventDescription* description, int64_t timestampStart, int64_t timestampFinish);
	static void Push(EventStorage* storage, const EventDescription* description, int64_t timestampStart);
	static void Pop(EventStorage* storage, int64_t timestampStart);


	Event(const EventDescription& description)
	{
		data = Start(description);
	}

	~Event()
	{
		if (data)
			Stop(*data);
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PROFILE_RUNTIME_INLINE ProfileRuntime::EventDescription* CreateDescription(const char* functionName, const char* fileName, int fileLine, const char* eventName = nullptr, const ::ProfileRuntime::Category::Type category = ::ProfileRuntime::Category::None, uint8_t flags = 0)
{
	if (eventName != nullptr)
		flags |= ::ProfileRuntime::EventDescription::IS_CUSTOM_NAME;

	return ::ProfileRuntime::EventDescription::Create(eventName != nullptr ? eventName : functionName, fileName, (unsigned long)fileLine, ::ProfileRuntime::Category::GetColor(category), ::ProfileRuntime::Category::GetMask(category), flags);
}
PROFILE_RUNTIME_INLINE ProfileRuntime::EventDescription* CreateDescription(const char* functionName, const char* fileName, int fileLine, const ::ProfileRuntime::Category::Type category)
{
	return ::ProfileRuntime::EventDescription::Create(functionName, fileName, (unsigned long)fileLine, ::ProfileRuntime::Category::GetColor(category), ::ProfileRuntime::Category::GetMask(category));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PROFILE_RUNTIME_API GPUEvent
{
	EventData* data;

	static EventData* Start(const EventDescription& description);
	static void Stop(EventData& data);

	GPUEvent(const EventDescription& description)
	{
		data = Start(description);
	}

	~GPUEvent()
	{
		if (data)
			Stop(*data);
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PROFILE_RUNTIME_API Tag
{
	static void Attach(const EventDescription& description, float val);
	static void Attach(const EventDescription& description, int32_t val);
	static void Attach(const EventDescription& description, uint32_t val);
	static void Attach(const EventDescription& description, uint64_t val);
	static void Attach(const EventDescription& description, float val[3]);
	static void Attach(const EventDescription& description, const char* val);
	static void Attach(const EventDescription& description, const char* val, uint16_t length);

	// Derived
	static void Attach(const EventDescription& description, float x, float y, float z)
	{
		float p[3] = { x, y, z }; Attach(description, p);
	}

};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ThreadScope
{
    ThreadScope(const char* name)
	{
		RegisterThread(name);
	}

	ThreadScope(const wchar_t* name)
	{
		RegisterThread(name);
	}

	~ThreadScope()
	{
		UnRegisterThread(false);
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum PROFILE_RUNTIME_API GPUQueueType
{
	GPU_QUEUE_GRAPHICS,
	GPU_QUEUE_COMPUTE,
	GPU_QUEUE_TRANSFER,
	GPU_QUEUE_VSYNC,

	GPU_QUEUE_COUNT,
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PROFILE_RUNTIME_API GPUContext
{
	void* cmdBuffer;
	GPUQueueType queue;
	int node;
	GPUContext(void* c = nullptr, GPUQueueType q = GPU_QUEUE_GRAPHICS, int n = 0) : cmdBuffer(c), queue(q), node(n) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PROFILE_RUNTIME_API void InitGpuD3D12(ID3D12Device* device, ID3D12CommandQueue** cmdQueues, uint32_t numQueues);
PROFILE_RUNTIME_API void InitGpuVulkan(VkDevice* vkDevices, VkPhysicalDevice* vkPhysicalDevices, VkQueue* vkQueues, uint32_t* cmdQueuesFamily, uint32_t numQueues, const VulkanFunctions* functions);
PROFILE_RUNTIME_API void GpuFlip(void* swapChain);
PROFILE_RUNTIME_API GPUContext SetGpuContext(GPUContext context);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PROFILE_RUNTIME_API GPUContextScope
{
	GPUContext prevContext;

	GPUContextScope(ID3D12CommandList* cmdList, GPUQueueType queue = GPU_QUEUE_GRAPHICS, int node = 0)
	{
		prevContext = SetGpuContext(GPUContext(cmdList, queue, node));
	}

	GPUContextScope(VkCommandBuffer cmdBuffer, GPUQueueType queue = GPU_QUEUE_GRAPHICS, int node = 0)
	{
		prevContext = SetGpuContext(GPUContext(cmdBuffer, queue, node));
	}

	~GPUContextScope()
	{
		SetGpuContext(prevContext);
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PROFILE_RUNTIME_API const EventDescription* GetFrameDescription(FrameType::Type frame = FrameType::CPU);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef void* (*AllocateFn)(size_t);
typedef void  (*DeallocateFn)(void*);
typedef void  (*InitThreadCb)(void);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PROFILE_RUNTIME_API void SetAllocator(AllocateFn allocateFn, DeallocateFn deallocateFn, InitThreadCb initThreadCb);
PROFILE_RUNTIME_API void Shutdown();
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef void(*CaptureSaveChunkCb)(const char*,size_t);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PROFILE_RUNTIME_API bool StartCapture(Mode::Type mode = Mode::DEFAULT, int samplingFrequency = 1000, bool force = true);
PROFILE_RUNTIME_API bool StopCapture(bool force = true);
PROFILE_RUNTIME_API bool SaveCapture(CaptureSaveChunkCb dataCb, bool force = true);
PROFILE_RUNTIME_API bool SaveCapture(const char* path, bool force = true);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ProfileRuntimeApp
{
	const char* m_Name;
	ProfileRuntimeApp(const char* name) : m_Name(name) { StartCapture(); }
	~ProfileRuntimeApp() { StopCapture(); SaveCapture(m_Name); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}

#define PROFILE_RUNTIME_UNUSED(x) (void)(x)
// Workaround for gcc compiler
#define PROFILE_RUNTIME_VA_ARGS(...) , ##__VA_ARGS__

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scoped profiling event which automatically grabs current function name.
// Use this macro 95% of the time.
// Example A:
//		void Function()
//		{
//			PROFILE_RUNTIME_EVENT();
//			... code ...
//		}
//		or
//		void Function()
//		{
//			PROFILE_RUNTIME_EVENT("CustomFunctionName");
//			... code ...
//		}
// Notes:
//		ProfileRuntime captures full name of the function including name space and arguments.
//		Full name is usually shortened in the ProfileRuntime GUI in order to highlight the most important bits.
#define PROFILE_RUNTIME_EVENT(...)	 static ::ProfileRuntime::EventDescription* PROFILE_RUNTIME_CONCAT(autogen_description_, __LINE__) = nullptr; \
							 if (PROFILE_RUNTIME_CONCAT(autogen_description_, __LINE__) == nullptr) PROFILE_RUNTIME_CONCAT(autogen_description_, __LINE__) = ::ProfileRuntime::CreateDescription(PROFILE_RUNTIME_FUNC, __FILE__, __LINE__, ##__VA_ARGS__); \
							 ::ProfileRuntime::Event PROFILE_RUNTIME_CONCAT(autogen_event_, __LINE__)( *(PROFILE_RUNTIME_CONCAT(autogen_description_, __LINE__)) ); 

// Backward compatibility with previous versions of ProfileRuntime
//#if !defined(PROFILE)
//#define PROFILE PROFILE_RUNTIME_EVENT()
//#endif

// Scoped profiling macro with predefined color.
// Use this macro for high-level function calls (e.g. AI, Physics, Audio, Render etc.).
// Example:
//		void UpdateAI()
//		{
//			PROFILE_RUNTIME_CATEGORY("UpdateAI", ProfileRuntime::Category::AI);
//			... code ...
//		}
//	
//		Macro could automatically capture current function name:
//		void UpdateAI()
//		{
//			PROFILE_RUNTIME_CATEGORY(PROFILE_RUNTIME_FUNC, ProfileRuntime::Category::AI);
//			... code ...
//		}
#define PROFILE_RUNTIME_CATEGORY(NAME, CATEGORY)	PROFILE_RUNTIME_EVENT(NAME, CATEGORY)

// Profiling event for Main Loop update.
// You need to call this function in the beginning of the each new frame.
// Example:
//		while (true)
//		{
//			PROFILE_RUNTIME_FRAME("MainThread");
//			... code ...
//		}
#define PROFILE_RUNTIME_FRAME(FRAME_NAME, ...)	static ::ProfileRuntime::ThreadScope mainThreadScope(FRAME_NAME);		\
										PROFILE_RUNTIME_UNUSED(mainThreadScope);									\
										::ProfileRuntime::EndFrame(__VA_ARGS__);								\
										::ProfileRuntime::Update();												\
										uint32_t frameNumber = ::ProfileRuntime::BeginFrame(__VA_ARGS__);		\
										::ProfileRuntime::Event PROFILE_RUNTIME_CONCAT(autogen_event_, __LINE__)(*::ProfileRuntime::GetFrameDescription(__VA_ARGS__)); \
										PROFILE_RUNTIME_TAG("Frame", frameNumber);

#define PROFILE_RUNTIME_UPDATE()						::ProfileRuntime::Update();
#define PROFILE_RUNTIME_FRAME_FLIP(...)				::ProfileRuntime::EndFrame(__VA_ARGS__); ::ProfileRuntime::BeginFrame(__VA_ARGS__);

// Scoped event for categorized frame types.
// Example:
//	void UpdateFrame()
//	{
//		// Flip "Main/Update" frame
//		PROFILE_RUNTIME_FRAME_EVENT(ProfileRuntime::FrameType::CPU);
//
//		// Root category event 
//		PROFILE_RUNTIME_CATEGORY("UpdateFrame", ProfileRuntime::Category::GameLogic);
//
//		...
//  }
//
#define PROFILE_RUNTIME_FRAME_EVENT(FRAME_TYPE, ...)		::ProfileRuntime::EndFrame(FRAME_TYPE);									\
												switch (FRAME_TYPE) {											\
													case ProfileRuntime::FrameType::CPU:								\
														::ProfileRuntime::Update();										\
														break;													\
													default:													\
														break;													\
												}																\
												::ProfileRuntime::BeginFrame(FRAME_TYPE);								\
												::ProfileRuntime::Event PROFILE_RUNTIME_CONCAT(autogen_event_, __LINE__)(*::ProfileRuntime::GetFrameDescription(FRAME_TYPE));


// Thread registration macro.
// Example:
//		void WorkerThread(...)
//		{
//			PROFILE_RUNTIME_THREAD("Worker");
//			while (isRunning)
//			{
//				...
//			}
//		}
#define PROFILE_RUNTIME_THREAD(THREAD_NAME) ::ProfileRuntime::ThreadScope brofilerThreadScope(THREAD_NAME);	\
									 PROFILE_RUNTIME_UNUSED(brofilerThreadScope);					\


// Thread registration macros.
// Useful for integration with custom job-managers.
#define PROFILE_RUNTIME_START_THREAD(THREAD_NAME) ::ProfileRuntime::RegisterThread(THREAD_NAME);
#define PROFILE_RUNTIME_STOP_THREAD() ::ProfileRuntime::UnRegisterThread(false);

// Attaches a custom data-tag.
// Supported types: int32, uint32, uint64, vec3, string (cut to 32 characters)
// Example:
//		PROFILE_RUNTIME_TAG("PlayerName", name[index]);
//		PROFILE_RUNTIME_TAG("Health", 100);
//		PROFILE_RUNTIME_TAG("Score", 0x80000000u);
//		PROFILE_RUNTIME_TAG("Height(cm)", 176.3f);
//		PROFILE_RUNTIME_TAG("Address", (uint64)*this);
//		PROFILE_RUNTIME_TAG("Position", 123.0f, 456.0f, 789.0f);
#define PROFILE_RUNTIME_TAG(NAME, ...)		static ::ProfileRuntime::EventDescription* PROFILE_RUNTIME_CONCAT(autogen_tag_, __LINE__) = nullptr; \
									if (PROFILE_RUNTIME_CONCAT(autogen_tag_, __LINE__) == nullptr) PROFILE_RUNTIME_CONCAT(autogen_tag_, __LINE__) = ::ProfileRuntime::EventDescription::Create( NAME, __FILE__, __LINE__ ); \
									::ProfileRuntime::Tag::Attach(*PROFILE_RUNTIME_CONCAT(autogen_tag_, __LINE__), __VA_ARGS__); \

// Scoped macro with DYNAMIC name.
// ProfileRuntime holds a copy of the provided name.
// Each scope does a search in hashmap for the name.
// Please use variations with STATIC names where it's possible.
// Use this macro for quick prototyping or intergratoin with other profiling systems (e.g. UE4)
// Example:
//		const char* name = ... ;
//		PROFILE_RUNTIME_EVENT_DYNAMIC(name);
#define PROFILE_RUNTIME_EVENT_DYNAMIC(NAME)	PROFILE_RUNTIME_CUSTOM_EVENT(::ProfileRuntime::EventDescription::CreateShared(NAME, __FILE__, __LINE__));
// Push\Pop profiling macro with DYNAMIC name.
#define PROFILE_RUNTIME_PUSH_DYNAMIC(NAME)		::ProfileRuntime::Event::Push(NAME);		

// Push\Pop profiling macro with STATIC name.
// Please avoid using Push\Pop approach in favor for scoped macros.
// For backward compatibility with some engines.
// Example:
//		PROFILE_RUNTIME_PUSH("ScopeName");
//		...
//		PROFILE_RUNTIME_POP();
#define PROFILE_RUNTIME_PUSH(NAME)				static ::ProfileRuntime::EventDescription* PROFILE_RUNTIME_CONCAT(autogen_description_, __LINE__) = nullptr; \
										if (PROFILE_RUNTIME_CONCAT(autogen_description_, __LINE__) == nullptr) PROFILE_RUNTIME_CONCAT(autogen_description_, __LINE__) = ::ProfileRuntime::EventDescription::Create( NAME, __FILE__, __LINE__ ); \
										::ProfileRuntime::Event::Push(*PROFILE_RUNTIME_CONCAT(autogen_description_, __LINE__));		
#define PROFILE_RUNTIME_POP()					::ProfileRuntime::Event::Pop();


// Scoped macro with predefined ProfileRuntime::EventDescription.
// Use these events instead of DYNAMIC macros to minimize overhead.
// Common use-case: integrating ProfileRuntime with internal script languages (e.g. Lua, Actionscript(Scaleform), etc.).
// Example:
//		Generating EventDescription once during initialization:
//		ProfileRuntime::EventDescription* description = ProfileRuntime::EventDescription::CreateShared("FunctionName");
//
//		Then we could just use a pointer to cached description later for profiling:
//		PROFILE_RUNTIME_CUSTOM_EVENT(description);
#define PROFILE_RUNTIME_CUSTOM_EVENT(DESCRIPTION) 							::ProfileRuntime::Event						  PROFILE_RUNTIME_CONCAT(autogen_event_, __LINE__)( *DESCRIPTION ); \

// Registration of a custom EventStorage (e.g. GPU, IO, etc.)
// Use it to present any extra information on the timeline.
// Example:
//		ProfileRuntime::EventStorage* IOStorage = ProfileRuntime::RegisterStorage("I/O");
// Notes:
//		Registration of a new storage is thread-safe.
#define PROFILE_RUNTIME_STORAGE_REGISTER(STORAGE_NAME)														::ProfileRuntime::RegisterStorage(STORAGE_NAME);

// Adding events to the custom storage.
// Helps to integrate ProfileRuntime into already existing profiling systems (e.g. GPU Profiler, I/O profiler, etc.).
// Example:
//			//Registering a storage - should be done once during initialization
//			static ProfileRuntime::EventStorage* IOStorage = ProfileRuntime::RegisterStorage("I/O");
//
//			int64_t cpuTimestampStart = ProfileRuntime::GetHighPrecisionTime();
//			...
//			int64_t cpuTimestampFinish = ProfileRuntime::GetHighPrecisionTime();
//
//			//Creating a shared event-description
//			static ProfileRuntime::EventDescription* IORead = ProfileRuntime::EventDescription::CreateShared("IO Read");
// 
//			PROFILE_RUNTIME_STORAGE_EVENT(IOStorage, IORead, cpuTimestampStart, cpuTimestampFinish);
// Notes:
//		It's not thread-safe to add events to the same storage from multiple threads.
//		Please guarantee thread-safety on the higher level if access from multiple threads to the same storage is required.
#define PROFILE_RUNTIME_STORAGE_EVENT(STORAGE, DESCRIPTION, CPU_TIMESTAMP_START, CPU_TIMESTAMP_FINISH)	if (::ProfileRuntime::IsActive()) { ::ProfileRuntime::Event::Add(STORAGE, DESCRIPTION, CPU_TIMESTAMP_START, CPU_TIMESTAMP_FINISH); }
#define PROFILE_RUNTIME_STORAGE_PUSH(STORAGE, DESCRIPTION, CPU_TIMESTAMP_START)							if (::ProfileRuntime::IsActive()) { ::ProfileRuntime::Event::Push(STORAGE, DESCRIPTION, CPU_TIMESTAMP_START); }
#define PROFILE_RUNTIME_STORAGE_POP(STORAGE, CPU_TIMESTAMP_FINISH)										if (::ProfileRuntime::IsActive()) { ::ProfileRuntime::Event::Pop(STORAGE, CPU_TIMESTAMP_FINISH); }


// Registers state change callback
// If callback returns false - the call is repeated the next frame
#define PROFILE_RUNTIME_SET_STATE_CHANGED_CALLBACK(CALLBACK)			::ProfileRuntime::SetStateChangedCallback(CALLBACK);


// Registers custom memory allocator within ProfileRuntime core
// Example:
//		PROFILE_RUNTIME_SET_MEMORY_ALLOCATOR([](size_t size) -> void* { return operator new(size); }, [](void* p) { operator delete(p); }, nullptr);
// Params:
//		INIT_THREAD_CALLBACK - callback for internal ProfileRuntime threads (useful if you need to setup some TLS variables related to the memory allocator for your thread)
// Notes:
//		Should be called before the first call to PROFILE_RUNTIME_FRAME
//		Allocation and deallocation functions should be thread-safe - ProfileRuntime doesn't do any synchronization for these calls
#define PROFILE_RUNTIME_SET_MEMORY_ALLOCATOR(ALLOCATE_FUNCTION, DEALLOCATE_FUNCTION, INIT_THREAD_CALLBACK)				::ProfileRuntime::SetAllocator(ALLOCATE_FUNCTION, DEALLOCATE_FUNCTION, INIT_THREAD_CALLBACK);

// Shutdown
// Clears all the internal buffers allocated by ProfileRuntime
// Notes:
//		You shouldn't call any ProfileRuntime functions after shutting down the system (it might lead to the undefined behaviour)
#define PROFILE_RUNTIME_SHUTDOWN()																						::ProfileRuntime::Shutdown();

// GPU events
#define PROFILE_RUNTIME_GPU_INIT_D3D12(DEVICE, CMD_QUEUES, NUM_CMD_QUEUS)													::ProfileRuntime::InitGpuD3D12(DEVICE, CMD_QUEUES, NUM_CMD_QUEUS);
#define PROFILE_RUNTIME_GPU_INIT_VULKAN(DEVICES, PHYSICAL_DEVICES, CMD_QUEUES, CMD_QUEUES_FAMILY, NUM_CMD_QUEUS, FUNCTIONS)	::ProfileRuntime::InitGpuVulkan(DEVICES, PHYSICAL_DEVICES, CMD_QUEUES, CMD_QUEUES_FAMILY, NUM_CMD_QUEUS, FUNCTIONS);

// Setup GPU context:
// Params:
//		(CommandBuffer\CommandList, [Optional] ProfileRuntime::GPUQueue queue, [Optional] int NodeIndex)
// Examples:
//		PROFILE_RUNTIME_GPU_CONTEXT(cmdBuffer); - all PROFILE_RUNTIME_GPU_EVENT will use the same command buffer within the scope
//		PROFILE_RUNTIME_GPU_CONTEXT(cmdBuffer, ProfileRuntime::GPU_QUEUE_COMPUTE); - all events will use the same command buffer and queue for the scope 
//		PROFILE_RUNTIME_GPU_CONTEXT(cmdBuffer, ProfileRuntime::GPU_QUEUE_COMPUTE, gpuIndex); - all events will use the same command buffer and queue for the scope 
#define PROFILE_RUNTIME_GPU_CONTEXT(...)	 ::ProfileRuntime::GPUContextScope PROFILE_RUNTIME_CONCAT(gpu_autogen_context_, __LINE__)(__VA_ARGS__); \
									 (void)PROFILE_RUNTIME_CONCAT(gpu_autogen_context_, __LINE__);

#define PROFILE_RUNTIME_GPU_EVENT(NAME)	 PROFILE_RUNTIME_EVENT(NAME); \
									 static ::ProfileRuntime::EventDescription* PROFILE_RUNTIME_CONCAT(gpu_autogen_description_, __LINE__) = nullptr; \
									 if (PROFILE_RUNTIME_CONCAT(gpu_autogen_description_, __LINE__) == nullptr) PROFILE_RUNTIME_CONCAT(gpu_autogen_description_, __LINE__) = ::ProfileRuntime::EventDescription::Create( NAME, __FILE__, __LINE__ ); \
									 ::ProfileRuntime::GPUEvent PROFILE_RUNTIME_CONCAT(gpu_autogen_event_, __LINE__)( *(PROFILE_RUNTIME_CONCAT(gpu_autogen_description_, __LINE__)) ); \

#define PROFILE_RUNTIME_GPU_FLIP(SWAP_CHAIN)		::ProfileRuntime::GpuFlip(SWAP_CHAIN);

/////////////////////////////////////////////////////////////////////////////////
// [Automation][Startup]
/////////////////////////////////////////////////////////////////////////////////

// Starts a new capture
// Params:
//		[Optional] Mode::Type mode /*= Mode::DEFAULT*/
//		[Optional] int samplingFrequency /*= 1000*/
#define PROFILE_RUNTIME_START_CAPTURE(...)				::ProfileRuntime::StartCapture(__VA_ARGS__);

// Stops a new capture (Keeps data intact in the local buffers)
#define PROFILE_RUNTIME_STOP_CAPTURE(...)				::ProfileRuntime::StopCapture(__VA_ARGS__);

// Saves capture
// Params:
//		const char* FilePath - path to the capture
// or
//		CaptureSaveChunkCb dataCb - callback for saving chunks of data
// Example:
//		PROFILE_RUNTIME_SAVE_CAPTURE("ConsoleApp.opt");
#define PROFILE_RUNTIME_SAVE_CAPTURE(...)				::ProfileRuntime::SaveCapture(__VA_ARGS__);

// Generate a capture for the whole scope
// Params:
//		NAME - name of the application
// Examples:
//		int main() {
//			PROFILE_RUNTIME_APP("MyGame"); //ProfileRuntime will automatically save a capture in the working directory with the name "MyGame(2019-09-08.14-30-19).opt"	
//			...
//		}
#define PROFILE_RUNTIME_APP(NAME)			PROFILE_RUNTIME_THREAD(NAME); \
									::ProfileRuntime::ProfileRuntimeApp _profile_runtimeApp(NAME); \
									PROFILE_RUNTIME_UNUSED(_profile_runtimeApp);


#else
#define PROFILE_RUNTIME_EVENT(...)
#define PROFILE_RUNTIME_CATEGORY(NAME, CATEGORY)
#define PROFILE_RUNTIME_FRAME(NAME)
#define PROFILE_RUNTIME_THREAD(THREAD_NAME)
#define PROFILE_RUNTIME_START_THREAD(THREAD_NAME)
#define PROFILE_RUNTIME_STOP_THREAD()
#define PROFILE_RUNTIME_TAG(NAME, DATA)
#define PROFILE_RUNTIME_EVENT_DYNAMIC(NAME)	
#define PROFILE_RUNTIME_PUSH_DYNAMIC(NAME)		
#define PROFILE_RUNTIME_PUSH(NAME)				
#define PROFILE_RUNTIME_POP()		
#define PROFILE_RUNTIME_CUSTOM_EVENT(DESCRIPTION)
#define PROFILE_RUNTIME_STORAGE_REGISTER(STORAGE_NAME)
#define PROFILE_RUNTIME_STORAGE_EVENT(STORAGE, DESCRIPTION, CPU_TIMESTAMP_START, CPU_TIMESTAMP_FINISH)
#define PROFILE_RUNTIME_STORAGE_PUSH(STORAGE, DESCRIPTION, CPU_TIMESTAMP_START)
#define PROFILE_RUNTIME_STORAGE_POP(STORAGE, CPU_TIMESTAMP_FINISH)				
#define PROFILE_RUNTIME_SET_STATE_CHANGED_CALLBACK(CALLBACK)
#define PROFILE_RUNTIME_SET_MEMORY_ALLOCATOR(ALLOCATE_FUNCTION, DEALLOCATE_FUNCTION)	
#define PROFILE_RUNTIME_SHUTDOWN()
#define PROFILE_RUNTIME_GPU_INIT_D3D12(DEVICE, CMD_QUEUES, NUM_CMD_QUEUS)
#define PROFILE_RUNTIME_GPU_INIT_VULKAN(DEVICES, PHYSICAL_DEVICES, CMD_QUEUES, CMD_QUEUES_FAMILY, NUM_CMD_QUEUS, FUNCTIONS)
#define PROFILE_RUNTIME_GPU_CONTEXT(...)
#define PROFILE_RUNTIME_GPU_EVENT(NAME)
#define PROFILE_RUNTIME_GPU_FLIP(SWAP_CHAIN)
#define PROFILE_RUNTIME_UPDATE()
#define PROFILE_RUNTIME_FRAME_FLIP(...)
#define PROFILE_RUNTIME_FRAME_EVENT(FRAME_TYPE, ...)
#define PROFILE_RUNTIME_START_CAPTURE(...)
#define PROFILE_RUNTIME_STOP_CAPTURE()
#define PROFILE_RUNTIME_SAVE_CAPTURE(...)
#define PROFILE_RUNTIME_APP(NAME)
#endif
