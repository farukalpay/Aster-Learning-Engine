#ifndef ASTER_LINEAR_SETUP_INCLUDED

#include <cassert>
#include <cstddef>

#define ASTER_LINEAR_VERSION_MAJOR 0
#define ASTER_LINEAR_VERSION_MINOR 9
#define ASTER_LINEAR_VERSION_PATCH 9
#define ASTER_LINEAR_VERSION_REVISION 9
#define ASTER_LINEAR_VERSION 999
#define ASTER_LINEAR_VERSION_MESSAGE "ASTER_LINEAR: version 0.9.9.9"

#define ASTER_LINEAR_SETUP_INCLUDED ASTER_LINEAR_VERSION

///////////////////////////////////////////////////////////////////////////////////
// Active states

#define ASTER_LINEAR_DISABLE		0
#define ASTER_LINEAR_ENABLE		1

///////////////////////////////////////////////////////////////////////////////////
// Messages

#if defined(ASTER_LINEAR_FORCE_MESSAGES)
#	define ASTER_LINEAR_MESSAGES ASTER_LINEAR_ENABLE
#else
#	define ASTER_LINEAR_MESSAGES ASTER_LINEAR_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Detect the platform

#include "../simd/platform.h"

///////////////////////////////////////////////////////////////////////////////////
// Build model

#if defined(_M_ARM64) || defined(__LP64__) || defined(_M_X64) || defined(__ppc64__) || defined(__x86_64__)
#	define ASTER_LINEAR_MODEL	ASTER_LINEAR_MODEL_64
#elif defined(__i386__) || defined(__ppc__) || defined(__ILP32__) || defined(_M_ARM)
#	define ASTER_LINEAR_MODEL	ASTER_LINEAR_MODEL_32
#else
#	define ASTER_LINEAR_MODEL	ASTER_LINEAR_MODEL_32
#endif//

#if !defined(ASTER_LINEAR_MODEL) && ASTER_LINEAR_COMPILER != 0
#	error "ASTER_LINEAR_MODEL undefined, your compiler may not be supported by ASTER_LINEAR. Add #define ASTER_LINEAR_MODEL 0 to ignore this message."
#endif//ASTER_LINEAR_MODEL

///////////////////////////////////////////////////////////////////////////////////
// C++ Version

// User defines: ASTER_LINEAR_FORCE_CXX98, ASTER_LINEAR_FORCE_CXX03, ASTER_LINEAR_FORCE_CXX11, ASTER_LINEAR_FORCE_CXX14, ASTER_LINEAR_FORCE_CXX17, ASTER_LINEAR_FORCE_CXX2A

#define ASTER_LINEAR_LANG_CXX98_FLAG			(1 << 1)
#define ASTER_LINEAR_LANG_CXX03_FLAG			(1 << 2)
#define ASTER_LINEAR_LANG_CXX0X_FLAG			(1 << 3)
#define ASTER_LINEAR_LANG_CXX11_FLAG			(1 << 4)
#define ASTER_LINEAR_LANG_CXX14_FLAG			(1 << 5)
#define ASTER_LINEAR_LANG_CXX17_FLAG			(1 << 6)
#define ASTER_LINEAR_LANG_CXX2A_FLAG			(1 << 7)
#define ASTER_LINEAR_LANG_CXXMS_FLAG			(1 << 8)
#define ASTER_LINEAR_LANG_CXXGNU_FLAG		(1 << 9)

#define ASTER_LINEAR_LANG_CXX98			ASTER_LINEAR_LANG_CXX98_FLAG
#define ASTER_LINEAR_LANG_CXX03			(ASTER_LINEAR_LANG_CXX98 | ASTER_LINEAR_LANG_CXX03_FLAG)
#define ASTER_LINEAR_LANG_CXX0X			(ASTER_LINEAR_LANG_CXX03 | ASTER_LINEAR_LANG_CXX0X_FLAG)
#define ASTER_LINEAR_LANG_CXX11			(ASTER_LINEAR_LANG_CXX0X | ASTER_LINEAR_LANG_CXX11_FLAG)
#define ASTER_LINEAR_LANG_CXX14			(ASTER_LINEAR_LANG_CXX11 | ASTER_LINEAR_LANG_CXX14_FLAG)
#define ASTER_LINEAR_LANG_CXX17			(ASTER_LINEAR_LANG_CXX14 | ASTER_LINEAR_LANG_CXX17_FLAG)
#define ASTER_LINEAR_LANG_CXX2A			(ASTER_LINEAR_LANG_CXX17 | ASTER_LINEAR_LANG_CXX2A_FLAG)
#define ASTER_LINEAR_LANG_CXXMS			ASTER_LINEAR_LANG_CXXMS_FLAG
#define ASTER_LINEAR_LANG_CXXGNU			ASTER_LINEAR_LANG_CXXGNU_FLAG

#if (defined(_MSC_EXTENSIONS))
#	define ASTER_LINEAR_LANG_EXT ASTER_LINEAR_LANG_CXXMS_FLAG
#elif ((ASTER_LINEAR_COMPILER & (ASTER_LINEAR_COMPILER_CLANG | ASTER_LINEAR_COMPILER_GCC)) && (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SIMD_BIT))
#	define ASTER_LINEAR_LANG_EXT ASTER_LINEAR_LANG_CXXMS_FLAG
#else
#	define ASTER_LINEAR_LANG_EXT 0
#endif

#if (defined(ASTER_LINEAR_FORCE_CXX_UNKNOWN))
#	define ASTER_LINEAR_LANG 0
#elif defined(ASTER_LINEAR_FORCE_CXX2A)
#	define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX2A | ASTER_LINEAR_LANG_EXT)
#	define ASTER_LINEAR_LANG_STL11_FORCED
#elif defined(ASTER_LINEAR_FORCE_CXX17)
#	define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX17 | ASTER_LINEAR_LANG_EXT)
#	define ASTER_LINEAR_LANG_STL11_FORCED
#elif defined(ASTER_LINEAR_FORCE_CXX14)
#	define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX14 | ASTER_LINEAR_LANG_EXT)
#	define ASTER_LINEAR_LANG_STL11_FORCED
#elif defined(ASTER_LINEAR_FORCE_CXX11)
#	define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX11 | ASTER_LINEAR_LANG_EXT)
#	define ASTER_LINEAR_LANG_STL11_FORCED
#elif defined(ASTER_LINEAR_FORCE_CXX03)
#	define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX03 | ASTER_LINEAR_LANG_EXT)
#elif defined(ASTER_LINEAR_FORCE_CXX98)
#	define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX98 | ASTER_LINEAR_LANG_EXT)
#else
#	if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC && defined(_MSVC_LANG)
#		if ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC15_7
#			define ASTER_LINEAR_LANG_PLATFORM _MSVC_LANG
#		elif ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC15
#			if _MSVC_LANG > 201402L
#				define ASTER_LINEAR_LANG_PLATFORM 201402L
#			else
#				define ASTER_LINEAR_LANG_PLATFORM _MSVC_LANG
#			endif
#		else
#			define ASTER_LINEAR_LANG_PLATFORM 0
#		endif
#	else
#		define ASTER_LINEAR_LANG_PLATFORM 0
#	endif

#	if __cplusplus > 201703L || ASTER_LINEAR_LANG_PLATFORM > 201703L
#		define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX2A | ASTER_LINEAR_LANG_EXT)
#	elif __cplusplus == 201703L || ASTER_LINEAR_LANG_PLATFORM == 201703L
#		define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX17 | ASTER_LINEAR_LANG_EXT)
#	elif __cplusplus == 201402L || __cplusplus == 201406L || __cplusplus == 201500L || ASTER_LINEAR_LANG_PLATFORM == 201402L
#		define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX14 | ASTER_LINEAR_LANG_EXT)
#	elif __cplusplus == 201103L || ASTER_LINEAR_LANG_PLATFORM == 201103L
#		define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX11 | ASTER_LINEAR_LANG_EXT)
#	elif defined(__INTEL_CXX11_MODE__) || defined(_MSC_VER) || defined(__GXX_EXPERIMENTAL_CXX0X__)
#		define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX0X | ASTER_LINEAR_LANG_EXT)
#	elif __cplusplus == 199711L
#		define ASTER_LINEAR_LANG (ASTER_LINEAR_LANG_CXX98 | ASTER_LINEAR_LANG_EXT)
#	else
#		define ASTER_LINEAR_LANG (0 | ASTER_LINEAR_LANG_EXT)
#	endif
#endif

///////////////////////////////////////////////////////////////////////////////////
// Has of C++ features

// http://clang.llvm.org/cxx_status.html
// http://gcc.gnu.org/projects/cxx0x.html
// http://msdn.microsoft.com/en-us/library/vstudio/hh567368(v=vs.120).aspx

// Android has multiple STLs but C++11 STL detection doesn't always work #284 #564
#if ASTER_LINEAR_PLATFORM == ASTER_LINEAR_PLATFORM_ANDROID && !defined(ASTER_LINEAR_LANG_STL11_FORCED)
#	define ASTER_LINEAR_HAS_CXX11_STL 0
#elif (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA_RTC) == ASTER_LINEAR_COMPILER_CUDA_RTC
#	define ASTER_LINEAR_HAS_CXX11_STL 0
#elif (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP)
#	define ASTER_LINEAR_HAS_CXX11_STL 0
#elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	if (defined(_LIBCPP_VERSION) || (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG) || defined(ASTER_LINEAR_LANG_STL11_FORCED))
#		define ASTER_LINEAR_HAS_CXX11_STL 1
#	else
#		define ASTER_LINEAR_HAS_CXX11_STL 0
#	endif
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_CXX11_STL 1
#else
#	define ASTER_LINEAR_HAS_CXX11_STL ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_GCC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_GCC48)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC12)) || \
		((ASTER_LINEAR_PLATFORM != ASTER_LINEAR_PLATFORM_WINDOWS) && (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_INTEL15))))
#endif

// N1720
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_STATIC_ASSERT __has_feature(cxx_static_assert)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_STATIC_ASSERT 1
#else
#	define ASTER_LINEAR_HAS_STATIC_ASSERT ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP))))
#endif

// N1988
#if ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_EXTENDED_INTEGER_TYPE 1
#else
#	define ASTER_LINEAR_HAS_EXTENDED_INTEGER_TYPE (\
		((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC)) || \
		((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP)))
#endif

// N2672 Initializer lists http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2672.htm
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_INITIALIZER_LISTS __has_feature(cxx_generalized_initializers)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_INITIALIZER_LISTS 1
#else
#	define ASTER_LINEAR_HAS_INITIALIZER_LISTS ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC15)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_INTEL14)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP))))
#endif

// N2544 Unrestricted unions http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2544.pdf
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_UNRESTRICTED_UNIONS __has_feature(cxx_unrestricted_unions)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_UNRESTRICTED_UNIONS 1
#else
#	define ASTER_LINEAR_HAS_UNRESTRICTED_UNIONS (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		(ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP)))
#endif

// N2346
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_DEFAULTED_FUNCTIONS __has_feature(cxx_defaulted_functions)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_DEFAULTED_FUNCTIONS 1
#else
#	define ASTER_LINEAR_HAS_DEFAULTED_FUNCTIONS ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC12)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL)) || \
		(ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP)))
#endif

// N2118
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_RVALUE_REFERENCES __has_feature(cxx_rvalue_references)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_RVALUE_REFERENCES 1
#else
#	define ASTER_LINEAR_HAS_RVALUE_REFERENCES ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP))))
#endif

// N2437 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2437.pdf
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_EXPLICIT_CONVERSION_OPERATORS __has_feature(cxx_explicit_conversions)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_EXPLICIT_CONVERSION_OPERATORS 1
#else
#	define ASTER_LINEAR_HAS_EXPLICIT_CONVERSION_OPERATORS ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_INTEL14)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC12)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP))))
#endif

// N2258 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2258.pdf
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_TEMPLATE_ALIASES __has_feature(cxx_alias_templates)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_TEMPLATE_ALIASES 1
#else
#	define ASTER_LINEAR_HAS_TEMPLATE_ALIASES ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC12)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP))))
#endif

// N2930 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2930.html
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_RANGE_FOR __has_feature(cxx_range_for)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_RANGE_FOR 1
#else
#	define ASTER_LINEAR_HAS_RANGE_FOR ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP))))
#endif

// N2341 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2341.pdf
#if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#	define ASTER_LINEAR_HAS_ALIGNOF __has_feature(cxx_alignas)
#elif ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_ALIGNOF 1
#else
#	define ASTER_LINEAR_HAS_ALIGNOF ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_INTEL15)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC14)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP))))
#endif

// N2235 Generalized Constant Expressions http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2235.pdf
// N3652 Extended Constant Expressions http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3652.html
#if (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SIMD_BIT) // Compiler SIMD intrinsics don't support constexpr...
#	define ASTER_LINEAR_HAS_CONSTEXPR 0
#elif (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG)
#	define ASTER_LINEAR_HAS_CONSTEXPR __has_feature(cxx_relaxed_constexpr)
#elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX14_FLAG)
#	define ASTER_LINEAR_HAS_CONSTEXPR 1
#else
#	define ASTER_LINEAR_HAS_CONSTEXPR ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && ASTER_LINEAR_HAS_INITIALIZER_LISTS && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_INTEL17)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC15))))
#endif

#if ASTER_LINEAR_HAS_CONSTEXPR
#	define ASTER_LINEAR_CONSTEXPR constexpr
#else
#	define ASTER_LINEAR_CONSTEXPR
#endif

//
#if ASTER_LINEAR_HAS_CONSTEXPR
# if (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG)
#	if __has_feature(cxx_if_constexpr)
#		define ASTER_LINEAR_HAS_IF_CONSTEXPR 1
#	else
# 		define ASTER_LINEAR_HAS_IF_CONSTEXPR 0
#	endif
# elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX17_FLAG)
# 	define ASTER_LINEAR_HAS_IF_CONSTEXPR 1
# else
# 	define ASTER_LINEAR_HAS_IF_CONSTEXPR 0
# endif
#else
#	define ASTER_LINEAR_HAS_IF_CONSTEXPR 0
#endif

#if ASTER_LINEAR_HAS_IF_CONSTEXPR
# 	define ASTER_LINEAR_IF_CONSTEXPR if constexpr
#else
#	define ASTER_LINEAR_IF_CONSTEXPR if
#endif

//
#if ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_ASSIGNABLE 1
#else
#	define ASTER_LINEAR_HAS_ASSIGNABLE ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC15)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_GCC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_GCC49))))
#endif

//
#define ASTER_LINEAR_HAS_TRIVIAL_QUERIES 0

//
#if ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG
#	define ASTER_LINEAR_HAS_MAKE_SIGNED 1
#else
#	define ASTER_LINEAR_HAS_MAKE_SIGNED ((ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC12)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP))))
#endif

//
#if defined(ASTER_LINEAR_FORCE_INTRINSICS)
#	define ASTER_LINEAR_HAS_BITSCAN_WINDOWS ((ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_WINDOWS) && (\
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL)) || \
		((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) && (ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_VC14) && (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_X86_BIT))))
#else
#	define ASTER_LINEAR_HAS_BITSCAN_WINDOWS 0
#endif

///////////////////////////////////////////////////////////////////////////////////
// OpenMP
#ifdef _OPENMP
#	if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_GCC
#		if ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_GCC61
#			define ASTER_LINEAR_HAS_OPENMP 45
#		elif ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_GCC49
#			define ASTER_LINEAR_HAS_OPENMP 40
#		elif ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_GCC47
#			define ASTER_LINEAR_HAS_OPENMP 31
#		else
#			define ASTER_LINEAR_HAS_OPENMP 0
#		endif
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#		if ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_CLANG38
#			define ASTER_LINEAR_HAS_OPENMP 31
#		else
#			define ASTER_LINEAR_HAS_OPENMP 0
#		endif
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC
#		define ASTER_LINEAR_HAS_OPENMP 20
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL
#		if ASTER_LINEAR_COMPILER >= ASTER_LINEAR_COMPILER_INTEL16
#			define ASTER_LINEAR_HAS_OPENMP 40
#		else
#			define ASTER_LINEAR_HAS_OPENMP 0
#		endif
#	else
#		define ASTER_LINEAR_HAS_OPENMP 0
#	endif
#else
#	define ASTER_LINEAR_HAS_OPENMP 0
#endif

///////////////////////////////////////////////////////////////////////////////////
// nullptr

#if ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG
#	define ASTER_LINEAR_CONFIG_NULLPTR ASTER_LINEAR_ENABLE
#else
#	define ASTER_LINEAR_CONFIG_NULLPTR ASTER_LINEAR_DISABLE
#endif

#if ASTER_LINEAR_CONFIG_NULLPTR == ASTER_LINEAR_ENABLE
#	define ASTER_LINEAR_NULLPTR nullptr
#else
#	define ASTER_LINEAR_NULLPTR 0
#endif

///////////////////////////////////////////////////////////////////////////////////
// Static assert

#if ASTER_LINEAR_HAS_STATIC_ASSERT
#	define ASTER_LINEAR_STATIC_ASSERT(x, message) static_assert(x, message)
#elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC
#	define ASTER_LINEAR_STATIC_ASSERT(x, message) typedef char __CASSERT__##__LINE__[(x) ? 1 : -1]
#else
#	define ASTER_LINEAR_STATIC_ASSERT(x, message) assert(x)
#endif//ASTER_LINEAR_LANG

///////////////////////////////////////////////////////////////////////////////////
// Qualifiers

#if (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA) || (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP)
#	define ASTER_LINEAR_CUDA_FUNC_DEF __device__ __host__
#	define ASTER_LINEAR_CUDA_FUNC_DECL __device__ __host__
#else
#	define ASTER_LINEAR_CUDA_FUNC_DEF
#	define ASTER_LINEAR_CUDA_FUNC_DECL
#endif

#if defined(ASTER_LINEAR_FORCE_INLINE)
#	if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC
#		define ASTER_LINEAR_INLINE __forceinline
#		define ASTER_LINEAR_NEVER_INLINE __declspec((noinline))
#	elif ASTER_LINEAR_COMPILER & (ASTER_LINEAR_COMPILER_GCC | ASTER_LINEAR_COMPILER_CLANG)
#		define ASTER_LINEAR_INLINE inline __attribute__((__always_inline__))
#		define ASTER_LINEAR_NEVER_INLINE __attribute__((__noinline__))
#	elif (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA) || (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP)
#		define ASTER_LINEAR_INLINE __forceinline__
#		define ASTER_LINEAR_NEVER_INLINE __noinline__
#	else
#		define ASTER_LINEAR_INLINE inline
#		define ASTER_LINEAR_NEVER_INLINE
#	endif//ASTER_LINEAR_COMPILER
#else
#	define ASTER_LINEAR_INLINE inline
#	define ASTER_LINEAR_NEVER_INLINE
#endif//defined(ASTER_LINEAR_FORCE_INLINE)

#define ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CUDA_FUNC_DECL
#define ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CUDA_FUNC_DEF ASTER_LINEAR_INLINE

///////////////////////////////////////////////////////////////////////////////////
// Swizzle operators

// User defines: ASTER_LINEAR_FORCE_SWIZZLE

#define ASTER_LINEAR_SWIZZLE_DISABLED		0
#define ASTER_LINEAR_SWIZZLE_OPERATOR		1
#define ASTER_LINEAR_SWIZZLE_FUNCTION		2

#if defined(ASTER_LINEAR_SWIZZLE)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_SWIZZLE is deprecated, use ASTER_LINEAR_FORCE_SWIZZLE instead.")
#	define ASTER_LINEAR_FORCE_SWIZZLE
#endif

#if defined(ASTER_LINEAR_FORCE_SWIZZLE) && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXXMS_FLAG) && !defined(ASTER_LINEAR_FORCE_XYZW_ONLY)
#	define ASTER_LINEAR_CONFIG_SWIZZLE ASTER_LINEAR_SWIZZLE_OPERATOR
#elif defined(ASTER_LINEAR_FORCE_SWIZZLE)
#	define ASTER_LINEAR_CONFIG_SWIZZLE ASTER_LINEAR_SWIZZLE_FUNCTION
#else
#	define ASTER_LINEAR_CONFIG_SWIZZLE ASTER_LINEAR_SWIZZLE_DISABLED
#endif

///////////////////////////////////////////////////////////////////////////////////
// Allows using not basic types as genType

// #define ASTER_LINEAR_FORCE_UNRESTRICTED_GENTYPE

#ifdef ASTER_LINEAR_FORCE_UNRESTRICTED_GENTYPE
#	define ASTER_LINEAR_CONFIG_UNRESTRICTED_GENTYPE ASTER_LINEAR_ENABLE
#else
#	define ASTER_LINEAR_CONFIG_UNRESTRICTED_GENTYPE ASTER_LINEAR_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Clip control, define ASTER_LINEAR_FORCE_DEPTH_ZERO_TO_ONE before including ASTER_LINEAR
// to use a clip space between 0 to 1.
// Coordinate system, define ASTER_LINEAR_FORCE_LEFT_HANDED before including ASTER_LINEAR
// to use left handed coordinate system by default.

#define ASTER_LINEAR_CLIP_CONTROL_ZO_BIT		(1 << 0) // ZERO_TO_ONE
#define ASTER_LINEAR_CLIP_CONTROL_NO_BIT		(1 << 1) // NEGATIVE_ONE_TO_ONE
#define ASTER_LINEAR_CLIP_CONTROL_LH_BIT		(1 << 2) // LEFT_HANDED, For DirectX, Metal, Vulkan
#define ASTER_LINEAR_CLIP_CONTROL_RH_BIT		(1 << 3) // RIGHT_HANDED, For DesktopGraphics, default in ASTER_LINEAR

#define ASTER_LINEAR_CLIP_CONTROL_LH_ZO (ASTER_LINEAR_CLIP_CONTROL_LH_BIT | ASTER_LINEAR_CLIP_CONTROL_ZO_BIT)
#define ASTER_LINEAR_CLIP_CONTROL_LH_NO (ASTER_LINEAR_CLIP_CONTROL_LH_BIT | ASTER_LINEAR_CLIP_CONTROL_NO_BIT)
#define ASTER_LINEAR_CLIP_CONTROL_RH_ZO (ASTER_LINEAR_CLIP_CONTROL_RH_BIT | ASTER_LINEAR_CLIP_CONTROL_ZO_BIT)
#define ASTER_LINEAR_CLIP_CONTROL_RH_NO (ASTER_LINEAR_CLIP_CONTROL_RH_BIT | ASTER_LINEAR_CLIP_CONTROL_NO_BIT)

#ifdef ASTER_LINEAR_FORCE_DEPTH_ZERO_TO_ONE
#	ifdef ASTER_LINEAR_FORCE_LEFT_HANDED
#		define ASTER_LINEAR_CONFIG_CLIP_CONTROL ASTER_LINEAR_CLIP_CONTROL_LH_ZO
#	else
#		define ASTER_LINEAR_CONFIG_CLIP_CONTROL ASTER_LINEAR_CLIP_CONTROL_RH_ZO
#	endif
#else
#	ifdef ASTER_LINEAR_FORCE_LEFT_HANDED
#		define ASTER_LINEAR_CONFIG_CLIP_CONTROL ASTER_LINEAR_CLIP_CONTROL_LH_NO
#	else
#		define ASTER_LINEAR_CONFIG_CLIP_CONTROL ASTER_LINEAR_CLIP_CONTROL_RH_NO
#	endif
#endif

///////////////////////////////////////////////////////////////////////////////////
// Qualifiers

#if (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC) || ((ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL) && (ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_WINDOWS))
#	define ASTER_LINEAR_DEPRECATED __declspec(deprecated)
#	define ASTER_LINEAR_ALIGNED_TYPEDEF(type, name, alignment) typedef __declspec(align(alignment)) type name
#elif ASTER_LINEAR_COMPILER & (ASTER_LINEAR_COMPILER_GCC | ASTER_LINEAR_COMPILER_CLANG | ASTER_LINEAR_COMPILER_INTEL)
#	define ASTER_LINEAR_DEPRECATED __attribute__((__deprecated__))
#	define ASTER_LINEAR_ALIGNED_TYPEDEF(type, name, alignment) typedef type name __attribute__((aligned(alignment)))
#elif (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA) || (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP)
#	define ASTER_LINEAR_DEPRECATED
#	define ASTER_LINEAR_ALIGNED_TYPEDEF(type, name, alignment) typedef type name __align__(x)
#else
#	define ASTER_LINEAR_DEPRECATED
#	define ASTER_LINEAR_ALIGNED_TYPEDEF(type, name, alignment) typedef type name
#endif

///////////////////////////////////////////////////////////////////////////////////

#ifdef ASTER_LINEAR_FORCE_EXPLICIT_CTOR
#	define ASTER_LINEAR_EXPLICIT explicit
#else
#	define ASTER_LINEAR_EXPLICIT
#endif

///////////////////////////////////////////////////////////////////////////////////
// SYCL

#if ASTER_LINEAR_COMPILER==ASTER_LINEAR_COMPILER_SYCL

#include <CL/sycl.hpp>
#include <limits>

namespace aster_linear {
namespace std {
	// Import SYCL's functions into the namespace aster_linear::std to force their usages.
	// It's important to use the math built-in function (sin, exp, ...)
	// of SYCL instead the std ones.
	using namespace cl::sycl;

	///////////////////////////////////////////////////////////////////////////////
	// Import some "harmless" std's stuffs used by aster_linear into
	// the new aster_linear::std namespace.
	template<typename T>
	using numeric_limits = ::std::numeric_limits<T>;

	using ::std::size_t;

	using ::std::uint8_t;
	using ::std::uint16_t;
	using ::std::uint32_t;
	using ::std::uint64_t;

	using ::std::int8_t;
	using ::std::int16_t;
	using ::std::int32_t;
	using ::std::int64_t;

	using ::std::make_unsigned;
	///////////////////////////////////////////////////////////////////////////////
} //namespace std
} //namespace aster_linear

#endif

///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////
// Length type: all length functions returns a length_t type.
// When ASTER_LINEAR_FORCE_SIZE_T_LENGTH is defined, length_t is a typedef of size_t otherwise
// length_t is a typedef of int like GLSL defines it.

#define ASTER_LINEAR_LENGTH_INT		1
#define ASTER_LINEAR_LENGTH_SIZE_T	2

#ifdef ASTER_LINEAR_FORCE_SIZE_T_LENGTH
#	define ASTER_LINEAR_CONFIG_LENGTH_TYPE		ASTER_LINEAR_LENGTH_SIZE_T
#else
#	define ASTER_LINEAR_CONFIG_LENGTH_TYPE		ASTER_LINEAR_LENGTH_INT
#endif

namespace aster_linear
{
	using std::size_t;
#	if ASTER_LINEAR_CONFIG_LENGTH_TYPE == ASTER_LINEAR_LENGTH_SIZE_T
		typedef size_t length_t;
#	else
		typedef int length_t;
#	endif
}//namespace aster_linear

///////////////////////////////////////////////////////////////////////////////////
// constexpr

#if ASTER_LINEAR_HAS_CONSTEXPR
#	define ASTER_LINEAR_CONFIG_CONSTEXP ASTER_LINEAR_ENABLE

	namespace aster_linear
	{
		template<typename T, std::size_t N>
		constexpr std::size_t countof(T const (&)[N])
		{
			return N;
		}
	}//namespace aster_linear
#	define ASTER_LINEAR_COUNTOF(arr) aster_linear::countof(arr)
#elif defined(_MSC_VER)
#	define ASTER_LINEAR_CONFIG_CONSTEXP ASTER_LINEAR_DISABLE

#	define ASTER_LINEAR_COUNTOF(arr) _countof(arr)
#else
#	define ASTER_LINEAR_CONFIG_CONSTEXP ASTER_LINEAR_DISABLE

#	define ASTER_LINEAR_COUNTOF(arr) sizeof(arr) / sizeof(arr[0])
#endif

///////////////////////////////////////////////////////////////////////////////////
// uint

namespace aster_linear{
namespace detail
{
	template<typename T>
	struct is_int
	{
		enum test {value = 0};
	};

	template<>
	struct is_int<unsigned int>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<signed int>
	{
		enum test {value = ~0};
	};
}//namespace detail

	typedef unsigned int	uint;
}//namespace aster_linear

///////////////////////////////////////////////////////////////////////////////////
// 64-bit int

#if ASTER_LINEAR_HAS_EXTENDED_INTEGER_TYPE
#	include <cstdint>
#endif

namespace aster_linear{
namespace detail
{
#	if ASTER_LINEAR_HAS_EXTENDED_INTEGER_TYPE
		typedef std::uint64_t						uint64;
		typedef std::int64_t						int64;
#	elif (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) // C99 detected, 64 bit types available
		typedef uint64_t							uint64;
		typedef int64_t								int64;
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC
		typedef unsigned __int64					uint64;
		typedef signed __int64						int64;
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_GCC
#		pragma GCC diagnostic ignored "-Wlong-long"
		__extension__ typedef unsigned long long	uint64;
		__extension__ typedef signed long long		int64;
#	elif (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG)
#		pragma clang diagnostic ignored "-Wc++11-long-long"
		typedef unsigned long long					uint64;
		typedef signed long long					int64;
#	else//unknown compiler
		typedef unsigned long long					uint64;
		typedef signed long long					int64;
#	endif
}//namespace detail
}//namespace aster_linear

///////////////////////////////////////////////////////////////////////////////////
// make_unsigned

#if ASTER_LINEAR_HAS_MAKE_SIGNED
#	include <type_traits>

namespace aster_linear{
namespace detail
{
	using std::make_unsigned;
}//namespace detail
}//namespace aster_linear

#else

namespace aster_linear{
namespace detail
{
	template<typename genType>
	struct make_unsigned
	{};

	template<>
	struct make_unsigned<char>
	{
		typedef unsigned char type;
	};

	template<>
	struct make_unsigned<signed char>
	{
		typedef unsigned char type;
	};

	template<>
	struct make_unsigned<short>
	{
		typedef unsigned short type;
	};

	template<>
	struct make_unsigned<int>
	{
		typedef unsigned int type;
	};

	template<>
	struct make_unsigned<long>
	{
		typedef unsigned long type;
	};

	template<>
	struct make_unsigned<int64>
	{
		typedef uint64 type;
	};

	template<>
	struct make_unsigned<unsigned char>
	{
		typedef unsigned char type;
	};

	template<>
	struct make_unsigned<unsigned short>
	{
		typedef unsigned short type;
	};

	template<>
	struct make_unsigned<unsigned int>
	{
		typedef unsigned int type;
	};

	template<>
	struct make_unsigned<unsigned long>
	{
		typedef unsigned long type;
	};

	template<>
	struct make_unsigned<uint64>
	{
		typedef uint64 type;
	};
}//namespace detail
}//namespace aster_linear
#endif

///////////////////////////////////////////////////////////////////////////////////
// Only use x, y, z, w as vector type components

#ifdef ASTER_LINEAR_FORCE_XYZW_ONLY
#	define ASTER_LINEAR_CONFIG_XYZW_ONLY ASTER_LINEAR_ENABLE
#else
#	define ASTER_LINEAR_CONFIG_XYZW_ONLY ASTER_LINEAR_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Configure the use of defaulted initialized types

#define ASTER_LINEAR_CTOR_INIT_DISABLE		0
#define ASTER_LINEAR_CTOR_INITIALIZER_LIST	1
#define ASTER_LINEAR_CTOR_INITIALISATION		2

#if defined(ASTER_LINEAR_FORCE_CTOR_INIT) && ASTER_LINEAR_HAS_INITIALIZER_LISTS
#	define ASTER_LINEAR_CONFIG_CTOR_INIT ASTER_LINEAR_CTOR_INITIALIZER_LIST
#elif defined(ASTER_LINEAR_FORCE_CTOR_INIT) && !ASTER_LINEAR_HAS_INITIALIZER_LISTS
#	define ASTER_LINEAR_CONFIG_CTOR_INIT ASTER_LINEAR_CTOR_INITIALISATION
#else
#	define ASTER_LINEAR_CONFIG_CTOR_INIT ASTER_LINEAR_CTOR_INIT_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Use SIMD instruction sets

#if ASTER_LINEAR_HAS_ALIGNOF && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXXMS_FLAG) && (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SIMD_BIT)
#	define ASTER_LINEAR_CONFIG_SIMD ASTER_LINEAR_ENABLE
#else
#	define ASTER_LINEAR_CONFIG_SIMD ASTER_LINEAR_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Configure the use of defaulted function

#if ASTER_LINEAR_HAS_DEFAULTED_FUNCTIONS
#	define ASTER_LINEAR_CONFIG_DEFAULTED_FUNCTIONS ASTER_LINEAR_ENABLE
#	define ASTER_LINEAR_DEFAULT = default
#else
#	define ASTER_LINEAR_CONFIG_DEFAULTED_FUNCTIONS ASTER_LINEAR_DISABLE
#	define ASTER_LINEAR_DEFAULT
#endif

#if ASTER_LINEAR_CONFIG_CTOR_INIT == ASTER_LINEAR_CTOR_INIT_DISABLE && ASTER_LINEAR_CONFIG_DEFAULTED_FUNCTIONS == ASTER_LINEAR_ENABLE
#	define ASTER_LINEAR_CONFIG_DEFAULTED_DEFAULT_CTOR ASTER_LINEAR_ENABLE
#	define ASTER_LINEAR_DEFAULT_CTOR ASTER_LINEAR_DEFAULT
#else
#	define ASTER_LINEAR_CONFIG_DEFAULTED_DEFAULT_CTOR ASTER_LINEAR_DISABLE
#	define ASTER_LINEAR_DEFAULT_CTOR
#endif

///////////////////////////////////////////////////////////////////////////////////
// Configure the use of aligned gentypes

#ifdef ASTER_LINEAR_FORCE_ALIGNED // Legacy define
#	define ASTER_LINEAR_FORCE_DEFAULT_ALIGNED_GENTYPES
#endif

#ifdef ASTER_LINEAR_FORCE_DEFAULT_ALIGNED_GENTYPES
#	define ASTER_LINEAR_FORCE_ALIGNED_GENTYPES
#endif

#if ASTER_LINEAR_HAS_ALIGNOF && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXXMS_FLAG) && (defined(ASTER_LINEAR_FORCE_ALIGNED_GENTYPES) || (ASTER_LINEAR_CONFIG_SIMD == ASTER_LINEAR_ENABLE))
#	define ASTER_LINEAR_CONFIG_ALIGNED_GENTYPES ASTER_LINEAR_ENABLE
#else
#	define ASTER_LINEAR_CONFIG_ALIGNED_GENTYPES ASTER_LINEAR_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Configure the use of anonymous structure as implementation detail

#if ((ASTER_LINEAR_CONFIG_SIMD == ASTER_LINEAR_ENABLE) || (ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR) || (ASTER_LINEAR_CONFIG_ALIGNED_GENTYPES == ASTER_LINEAR_ENABLE))
#	define ASTER_LINEAR_CONFIG_ANONYMOUS_STRUCT ASTER_LINEAR_ENABLE
#else
#	define ASTER_LINEAR_CONFIG_ANONYMOUS_STRUCT ASTER_LINEAR_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Silent warnings

#ifdef ASTER_LINEAR_FORCE_SILENT_WARNINGS
#	define ASTER_LINEAR_SILENT_WARNINGS ASTER_LINEAR_ENABLE
#else
#	define ASTER_LINEAR_SILENT_WARNINGS ASTER_LINEAR_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Precision

#define ASTER_LINEAR_HIGHP		1
#define ASTER_LINEAR_MEDIUMP		2
#define ASTER_LINEAR_LOWP		3

#if defined(ASTER_LINEAR_FORCE_PRECISION_HIGHP_BOOL) || defined(ASTER_LINEAR_PRECISION_HIGHP_BOOL)
#	define ASTER_LINEAR_CONFIG_PRECISION_BOOL		ASTER_LINEAR_HIGHP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_MEDIUMP_BOOL) || defined(ASTER_LINEAR_PRECISION_MEDIUMP_BOOL)
#	define ASTER_LINEAR_CONFIG_PRECISION_BOOL		ASTER_LINEAR_MEDIUMP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_LOWP_BOOL) || defined(ASTER_LINEAR_PRECISION_LOWP_BOOL)
#	define ASTER_LINEAR_CONFIG_PRECISION_BOOL		ASTER_LINEAR_LOWP
#else
#	define ASTER_LINEAR_CONFIG_PRECISION_BOOL		ASTER_LINEAR_HIGHP
#endif

#if defined(ASTER_LINEAR_FORCE_PRECISION_HIGHP_INT) || defined(ASTER_LINEAR_PRECISION_HIGHP_INT)
#	define ASTER_LINEAR_CONFIG_PRECISION_INT			ASTER_LINEAR_HIGHP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_MEDIUMP_INT) || defined(ASTER_LINEAR_PRECISION_MEDIUMP_INT)
#	define ASTER_LINEAR_CONFIG_PRECISION_INT			ASTER_LINEAR_MEDIUMP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_LOWP_INT) || defined(ASTER_LINEAR_PRECISION_LOWP_INT)
#	define ASTER_LINEAR_CONFIG_PRECISION_INT			ASTER_LINEAR_LOWP
#else
#	define ASTER_LINEAR_CONFIG_PRECISION_INT			ASTER_LINEAR_HIGHP
#endif

#if defined(ASTER_LINEAR_FORCE_PRECISION_HIGHP_UINT) || defined(ASTER_LINEAR_PRECISION_HIGHP_UINT)
#	define ASTER_LINEAR_CONFIG_PRECISION_UINT		ASTER_LINEAR_HIGHP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_MEDIUMP_UINT) || defined(ASTER_LINEAR_PRECISION_MEDIUMP_UINT)
#	define ASTER_LINEAR_CONFIG_PRECISION_UINT		ASTER_LINEAR_MEDIUMP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_LOWP_UINT) || defined(ASTER_LINEAR_PRECISION_LOWP_UINT)
#	define ASTER_LINEAR_CONFIG_PRECISION_UINT		ASTER_LINEAR_LOWP
#else
#	define ASTER_LINEAR_CONFIG_PRECISION_UINT		ASTER_LINEAR_HIGHP
#endif

#if defined(ASTER_LINEAR_FORCE_PRECISION_HIGHP_FLOAT) || defined(ASTER_LINEAR_PRECISION_HIGHP_FLOAT)
#	define ASTER_LINEAR_CONFIG_PRECISION_FLOAT		ASTER_LINEAR_HIGHP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_MEDIUMP_FLOAT) || defined(ASTER_LINEAR_PRECISION_MEDIUMP_FLOAT)
#	define ASTER_LINEAR_CONFIG_PRECISION_FLOAT		ASTER_LINEAR_MEDIUMP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_LOWP_FLOAT) || defined(ASTER_LINEAR_PRECISION_LOWP_FLOAT)
#	define ASTER_LINEAR_CONFIG_PRECISION_FLOAT		ASTER_LINEAR_LOWP
#else
#	define ASTER_LINEAR_CONFIG_PRECISION_FLOAT		ASTER_LINEAR_HIGHP
#endif

#if defined(ASTER_LINEAR_FORCE_PRECISION_HIGHP_DOUBLE) || defined(ASTER_LINEAR_PRECISION_HIGHP_DOUBLE)
#	define ASTER_LINEAR_CONFIG_PRECISION_DOUBLE		ASTER_LINEAR_HIGHP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_MEDIUMP_DOUBLE) || defined(ASTER_LINEAR_PRECISION_MEDIUMP_DOUBLE)
#	define ASTER_LINEAR_CONFIG_PRECISION_DOUBLE		ASTER_LINEAR_MEDIUMP
#elif defined(ASTER_LINEAR_FORCE_PRECISION_LOWP_DOUBLE) || defined(ASTER_LINEAR_PRECISION_LOWP_DOUBLE)
#	define ASTER_LINEAR_CONFIG_PRECISION_DOUBLE		ASTER_LINEAR_LOWP
#else
#	define ASTER_LINEAR_CONFIG_PRECISION_DOUBLE		ASTER_LINEAR_HIGHP
#endif

///////////////////////////////////////////////////////////////////////////////////
// Check inclusions of different versions of ASTER_LINEAR

#elif ((ASTER_LINEAR_SETUP_INCLUDED != ASTER_LINEAR_VERSION) && !defined(ASTER_LINEAR_FORCE_IGNORE_VERSION))
#	error "ASTER_LINEAR error: A different version of ASTER_LINEAR is already included. Define ASTER_LINEAR_FORCE_IGNORE_VERSION before including ASTER_LINEAR headers to ignore this error."
#elif ASTER_LINEAR_SETUP_INCLUDED == ASTER_LINEAR_VERSION

///////////////////////////////////////////////////////////////////////////////////
// Messages

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_MESSAGE_DISPLAYED)
#	define ASTER_LINEAR_MESSAGE_DISPLAYED
#		define ASTER_LINEAR_STR_HELPER(x) #x
#		define ASTER_LINEAR_STR(x) ASTER_LINEAR_STR_HELPER(x)

	// Report ASTER_LINEAR version
#		pragma message (ASTER_LINEAR_STR(ASTER_LINEAR_VERSION_MESSAGE))

	// Report C++ language
#	if (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX2A_FLAG) && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_EXT)
#		pragma message("ASTER_LINEAR: C++ 2A with extensions")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX2A_FLAG)
#		pragma message("ASTER_LINEAR: C++ 2A")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX17_FLAG) && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_EXT)
#		pragma message("ASTER_LINEAR: C++ 17 with extensions")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX17_FLAG)
#		pragma message("ASTER_LINEAR: C++ 17")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX14_FLAG) && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_EXT)
#		pragma message("ASTER_LINEAR: C++ 14 with extensions")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX14_FLAG)
#		pragma message("ASTER_LINEAR: C++ 14")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG) && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_EXT)
#		pragma message("ASTER_LINEAR: C++ 11 with extensions")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX11_FLAG)
#		pragma message("ASTER_LINEAR: C++ 11")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG) && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_EXT)
#		pragma message("ASTER_LINEAR: C++ 0x with extensions")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX0X_FLAG)
#		pragma message("ASTER_LINEAR: C++ 0x")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX03_FLAG) && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_EXT)
#		pragma message("ASTER_LINEAR: C++ 03 with extensions")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX03_FLAG)
#		pragma message("ASTER_LINEAR: C++ 03")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX98_FLAG) && (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_EXT)
#		pragma message("ASTER_LINEAR: C++ 98 with extensions")
#	elif (ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXX98_FLAG)
#		pragma message("ASTER_LINEAR: C++ 98")
#	else
#		pragma message("ASTER_LINEAR: C++ language undetected")
#	endif//ASTER_LINEAR_LANG

	// Report compiler detection
#	if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA
#		pragma message("ASTER_LINEAR: CUDA compiler detected")
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP
#		pragma message("ASTER_LINEAR: HIP compiler detected")
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC
#		pragma message("ASTER_LINEAR: Visual C++ compiler detected")
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#		pragma message("ASTER_LINEAR: Clang compiler detected")
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_INTEL
#		pragma message("ASTER_LINEAR: Intel Compiler detected")
#	elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_GCC
#		pragma message("ASTER_LINEAR: GCC compiler detected")
#	else
#		pragma message("ASTER_LINEAR: Compiler not detected")
#	endif

	// Report build target
#	if (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_AVX2_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: x86 64 bits with AVX2 instruction set build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_AVX2_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: x86 32 bits with AVX2 instruction set build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_AVX_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: x86 64 bits with AVX instruction set build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_AVX_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: x86 32 bits with AVX instruction set build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE42_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: x86 64 bits with SSE4.2 instruction set build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE42_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: x86 32 bits with SSE4.2 instruction set build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE41_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: x86 64 bits with SSE4.1 instruction set build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE41_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: x86 32 bits with SSE4.1 instruction set build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSSE3_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: x86 64 bits with SSSE3 instruction set build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSSE3_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: x86 32 bits with SSSE3 instruction set build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE3_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: x86 64 bits with SSE3 instruction set build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE3_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: x86 32 bits with SSE3 instruction set build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: x86 64 bits with SSE2 instruction set build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: x86 32 bits with SSE2 instruction set build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_X86_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: x86 64 bits build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_X86_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: x86 32 bits build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_NEON_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: ARM 64 bits with Neon instruction set build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_NEON_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: ARM 32 bits with Neon instruction set build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_ARM_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: ARM 64 bits build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_ARM_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: ARM 32 bits build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_MIPS_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: MIPS 64 bits build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_MIPS_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: MIPS 32 bits build target")

#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_PPC_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_64)
#		pragma message("ASTER_LINEAR: PowerPC 64 bits build target")
#	elif (ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_PPC_BIT) && (ASTER_LINEAR_MODEL == ASTER_LINEAR_MODEL_32)
#		pragma message("ASTER_LINEAR: PowerPC 32 bits build target")
#	else
#		pragma message("ASTER_LINEAR: Unknown build target")
#	endif//ASTER_LINEAR_ARCH

	// Report platform name
#	if(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_QNXNTO)
#		pragma message("ASTER_LINEAR: QNX platform detected")
//#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_IOS)
//#		pragma message("ASTER_LINEAR: iOS platform detected")
#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_APPLE)
#		pragma message("ASTER_LINEAR: Apple platform detected")
#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_WINCE)
#		pragma message("ASTER_LINEAR: WinCE platform detected")
#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_WINDOWS)
#		pragma message("ASTER_LINEAR: Windows platform detected")
#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_CHROME_NACL)
#		pragma message("ASTER_LINEAR: Native Client detected")
#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_ANDROID)
#		pragma message("ASTER_LINEAR: Android platform detected")
#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_LINUX)
#		pragma message("ASTER_LINEAR: Linux platform detected")
#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_UNIX)
#		pragma message("ASTER_LINEAR: UNIX platform detected")
#	elif(ASTER_LINEAR_PLATFORM & ASTER_LINEAR_PLATFORM_UNKNOWN)
#		pragma message("ASTER_LINEAR: platform unknown")
#	else
#		pragma message("ASTER_LINEAR: platform not detected")
#	endif

	// Report whether only xyzw component are used
#	if defined ASTER_LINEAR_FORCE_XYZW_ONLY
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_XYZW_ONLY is defined. Only x, y, z and w component are available in vector type. This define disables swizzle operators and SIMD instruction sets.")
#	endif

	// Report swizzle operator support
#	if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_SWIZZLE is defined, swizzling operators enabled.")
#	elif ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_FUNCTION
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_SWIZZLE is defined, swizzling functions enabled. Enable compiler C++ language extensions to enable swizzle operators.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_SWIZZLE is undefined. swizzling functions or operators are disabled.")
#	endif

	// Report .length() type
#	if ASTER_LINEAR_CONFIG_LENGTH_TYPE == ASTER_LINEAR_LENGTH_SIZE_T
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_SIZE_T_LENGTH is defined. .length() returns a aster_linear::length_t, a typedef of std::size_t.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_SIZE_T_LENGTH is undefined. .length() returns a aster_linear::length_t, a typedef of int following GLSL.")
#	endif

#	if ASTER_LINEAR_CONFIG_UNRESTRICTED_GENTYPE == ASTER_LINEAR_ENABLE
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_UNRESTRICTED_GENTYPE is defined. Removes GLSL restrictions on valid function genTypes.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_UNRESTRICTED_GENTYPE is undefined. Follows strictly GLSL on valid function genTypes.")
#	endif

#	if ASTER_LINEAR_SILENT_WARNINGS == ASTER_LINEAR_ENABLE
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_SILENT_WARNINGS is defined. Ignores C++ warnings from using C++ language extensions.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_SILENT_WARNINGS is undefined. Shows C++ warnings from using C++ language extensions.")
#	endif

#	ifdef ASTER_LINEAR_FORCE_SINGLE_ONLY
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_SINGLE_ONLY is defined. Using only single precision floating-point types.")
#	endif

#	if defined(ASTER_LINEAR_FORCE_ALIGNED_GENTYPES) && (ASTER_LINEAR_CONFIG_ALIGNED_GENTYPES == ASTER_LINEAR_ENABLE)
#		undef ASTER_LINEAR_FORCE_ALIGNED_GENTYPES
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_ALIGNED_GENTYPES is defined, allowing aligned types. This prevents the use of C++ constexpr.")
#	elif defined(ASTER_LINEAR_FORCE_ALIGNED_GENTYPES) && (ASTER_LINEAR_CONFIG_ALIGNED_GENTYPES == ASTER_LINEAR_DISABLE)
#		undef ASTER_LINEAR_FORCE_ALIGNED_GENTYPES
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_ALIGNED_GENTYPES is defined but is disabled. It requires C++11 and language extensions.")
#	endif

#	if defined(ASTER_LINEAR_FORCE_DEFAULT_ALIGNED_GENTYPES)
#		if ASTER_LINEAR_CONFIG_ALIGNED_GENTYPES == ASTER_LINEAR_DISABLE
#			undef ASTER_LINEAR_FORCE_DEFAULT_ALIGNED_GENTYPES
#			pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_DEFAULT_ALIGNED_GENTYPES is defined but is disabled. It requires C++11 and language extensions.")
#		elif ASTER_LINEAR_CONFIG_ALIGNED_GENTYPES == ASTER_LINEAR_ENABLE
#			pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_DEFAULT_ALIGNED_GENTYPES is defined. All gentypes (e.g. vec3) will be aligned and padded by default.")
#		endif
#	endif

#	if ASTER_LINEAR_CONFIG_CLIP_CONTROL & ASTER_LINEAR_CLIP_CONTROL_ZO_BIT
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_DEPTH_ZERO_TO_ONE is defined. Using zero to one depth clip space.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_DEPTH_ZERO_TO_ONE is undefined. Using negative one to one depth clip space.")
#	endif

#	if ASTER_LINEAR_CONFIG_CLIP_CONTROL & ASTER_LINEAR_CLIP_CONTROL_LH_BIT
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_LEFT_HANDED is defined. Using left handed coordinate system.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_FORCE_LEFT_HANDED is undefined. Using right handed coordinate system.")
#	endif
#endif//ASTER_LINEAR_MESSAGES

#endif//ASTER_LINEAR_SETUP_INCLUDED
