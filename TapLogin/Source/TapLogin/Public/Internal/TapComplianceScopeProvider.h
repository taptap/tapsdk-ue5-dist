// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapLogin.h"

using FTapComplianceScopeResolver = TFunction<TOptional<ETapLoginScope>()>;

/** Internal bridge used by TapCompliance to contribute its Desktop login scope. */
class TAPLOGIN_API FTapComplianceScopeProvider
{
public:
	static void Register(FTapComplianceScopeResolver Resolver);
	static void Unregister();
	static TOptional<ETapLoginScope> Resolve();
};
