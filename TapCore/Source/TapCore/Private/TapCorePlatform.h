#pragma once
#include "CoreMinimal.h"
// Platform-specific includes (conditionally compiled)
#if PLATFORM_ANDROID
	#include "Internal/Platform/TapCoreAndroid.h"
#elif PLATFORM_IOS
	#include "Platform/TapCoreiOS.h"
#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	#include "Platform/TapCoreDesktop.h"
#endif

// =============================================================================
// Platform Type Alias for Convenience
// =============================================================================

#if PLATFORM_ANDROID
	/** Type alias for current platform implementation */
	using FTapCorePlatformImpl = FTapCoreAndroid;
	#define TAP_PLATFORM_NAME TEXT("Android")
#elif PLATFORM_IOS
	/** Type alias for current platform implementation */
	using FTapCorePlatformImpl = FTapCoreiOS;
	#define TAP_PLATFORM_NAME TEXT("iOS")
#elif PLATFORM_WINDOWS
	/** Type alias for current platform implementation */
	using FTapCorePlatformImpl = FTapCoreDesktop;
	#define TAP_PLATFORM_NAME TEXT("Windows")
#elif PLATFORM_MAC
	/** Type alias for current platform implementation */
	using FTapCorePlatformImpl = FTapCoreDesktop;
	#define TAP_PLATFORM_NAME TEXT("Mac")
#elif PLATFORM_LINUX
	/** Type alias for current platform implementation */
	using FTapCorePlatformImpl = FTapCoreDesktop;
	#define TAP_PLATFORM_NAME TEXT("Linux")
#else
	#error "Unsupported platform for TapSDK"
#endif

// =============================================================================
// Compile-time Platform Detection Helpers
// =============================================================================

namespace TapPlatform
{
	/** Check if current platform is mobile (Android or iOS) */
	constexpr bool IsMobile()
	{
		#if PLATFORM_ANDROID || PLATFORM_IOS
			return true;
		#else
			return false;
		#endif
	}

	/** Check if current platform is desktop */
	constexpr bool IsDesktop()
	{
		#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
			return true;
		#else
			return false;
		#endif
	}

	/** Check if current platform is Android */
	constexpr bool IsAndroid()
	{
		#if PLATFORM_ANDROID
			return true;
		#else
			return false;
		#endif
	}

	/** Check if current platform is iOS */
	constexpr bool IsIOS()
	{
		#if PLATFORM_IOS
			return true;
		#else
			return false;
		#endif
	}

	/** Get platform name as compile-time constant */
	inline const TCHAR* GetName()
	{
		return TAP_PLATFORM_NAME;
	}
}
