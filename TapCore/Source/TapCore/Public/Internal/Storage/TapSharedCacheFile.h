// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

/** Stable sidecar lock and atomic publication, following Unity SharedCacheFile.
 * Synchronous UE callers make one lock attempt; contention never sleeps on the
 * game thread. Paths and ownership remain unchanged.
 */
class TAPCORE_API FTapSharedCacheFile
{
public:
	struct FLease { virtual ~FLease() = default; };
	static TUniquePtr<FLease> TryAcquire(const FString& Path);
	static bool Write(const FString& Path, const FString& Contents);
	/** Caller holds TryAcquire(Path) across the complete read-modify-write. */
	static bool WriteUnderLock(const FString& Path, const FString& Contents);
};
