// Copyright TapTap. All Rights Reserved.

#include "Internal/TapComplianceScopeProvider.h"

namespace
{
	FCriticalSection GComplianceScopeLock;
	FTapComplianceScopeResolver GComplianceScopeResolver;
}

void FTapComplianceScopeProvider::Register(FTapComplianceScopeResolver Resolver)
{
	FScopeLock Lock(&GComplianceScopeLock);
	GComplianceScopeResolver = MoveTemp(Resolver);
}

void FTapComplianceScopeProvider::Unregister()
{
	FScopeLock Lock(&GComplianceScopeLock);
	GComplianceScopeResolver = nullptr;
}

TOptional<ETapLoginScope> FTapComplianceScopeProvider::Resolve()
{
	FTapComplianceScopeResolver Resolver;
	{
		FScopeLock Lock(&GComplianceScopeLock);
		Resolver = GComplianceScopeResolver;
	}
	return Resolver ? Resolver() : TOptional<ETapLoginScope>();
}
