// Copyright TapTap. All Rights Reserved.

#include "Init/TapLoginInitTask.h"
#include "TapLoginModule.h"
#include "Internal/TapLoginInternal.h"
#include "Internal/Init/TapInitTaskRegistry.h"

// 手动实现静态成员（避免使用有问题的宏）
FTapTaskConfig FTapLoginInitTask::TaskConfig;
FTapLoginInitTask FTapLoginInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapLoginInitTask> FTapLoginInitTask::AutoRegister;

// 配置任务属性（使用静态初始化）
namespace
{
	struct FTaskConfigInitializer
	{
		FTaskConfigInitializer()
		{
			// 使用公有静态方法配置任务
			FTapLoginInitTask::WithPriority(20);  // 优先级20，在TapCore之后执行
			FTapLoginInitTask::CanRunInBackground(false);
		}
	};
	static FTaskConfigInitializer GTaskConfigInit;
}

void FTapLoginInitTask::Execute(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("TapLogin Initialization Started"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));

	// 打印配置信息
	UE_LOG(LogTapLogin, Log, TEXT("ClientID: %s"), *Config.ClientID);
	UE_LOG(LogTapLogin, Log, TEXT("Region: %s"), Config.Region == ETapRegion::CN ? TEXT("CN") : TEXT("Global"));
	UE_LOG(LogTapLogin, Log, TEXT("EnableLog: %s"), Config.bEnableLog ? TEXT("true") : TEXT("false"));

	// 打印版本信息
	FString LoginVersion = FTapLoginInternal::GetVersion();
	UE_LOG(LogTapLogin, Log, TEXT("TapLogin Version: %s"), *LoginVersion);

	// 执行登录模块初始化
	UE_LOG(LogTapLogin, Log, TEXT("Calling TapLogin::Initialize()..."));

	FTapLoginInternal::Initialize();

	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("TapLogin Initialization Completed"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
}
