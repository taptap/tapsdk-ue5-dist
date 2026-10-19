#pragma once

#include "CoreMinimal.h"

/** Module lifecycle and error delivery; not part of the game developer facade. */
class TAPLICENSE_API FTapLicenseInternal
{
public:
	static void SetupCallbacks();
	static void Initialize();
	static void Shutdown();
	static void ReportError(const FString& ErrorMessage);
};
