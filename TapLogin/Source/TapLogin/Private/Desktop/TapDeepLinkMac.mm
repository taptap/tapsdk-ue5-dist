// Copyright TapTap. All Rights Reserved.

#include "TapDeepLinkHandler.h"
#include "Internal/TapLogPrivacy.h"

#if PLATFORM_MAC

#include "TapLoginModule.h"
#import <Cocoa/Cocoa.h>

// =============================================================================
// Mac: 使用 NSAppleEventManager 注册 kAEGetURL 处理器
// 当系统打开 open-taptap-{clientId}://authorize?code=xxx 时触发
// =============================================================================

// ObjC helper 类，转发 Apple Event 到 C++ handler
@interface TapDeepLinkAppleEventHandler : NSObject
- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;
@end

@implementation TapDeepLinkAppleEventHandler

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	NSString *urlString = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
	if (urlString)
	{
		FString URL(urlString);
		UE_LOG(LogTapLogin, Log, TEXT("[DeepLink Mac] Apple Event kAEGetURL: %s"), *TapLogPrivacy::Url(URL, true));
		FTapDeepLinkHandler::Get().HandleURL(URL);
	}
}

@end

void FTapDeepLinkHandler::RegisterAppleEventHandler()
{
	if (AppleEventHandler != nil) return;

	AppleEventHandler = [[TapDeepLinkAppleEventHandler alloc] init];

	[[NSAppleEventManager sharedAppleEventManager]
		setEventHandler:(TapDeepLinkAppleEventHandler *)AppleEventHandler
		andSelector:@selector(handleGetURLEvent:withReplyEvent:)
		forEventClass:kInternetEventClass
		andEventID:kAEGetURL];

	UE_LOG(LogTapLogin, Log, TEXT("[DeepLink Mac] Registered kAEGetURL Apple Event handler"));
}

void FTapDeepLinkHandler::UnregisterAppleEventHandler()
{
	if (AppleEventHandler == nil) return;

	[[NSAppleEventManager sharedAppleEventManager]
		removeEventHandlerForEventClass:kInternetEventClass
		andEventID:kAEGetURL];

	AppleEventHandler = nil;

	UE_LOG(LogTapLogin, Log, TEXT("[DeepLink Mac] Unregistered kAEGetURL Apple Event handler"));
}

#endif // PLATFORM_MAC
