// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
#include "TapCoreModule.h"

/**
 * TapSDK Initialization Task Interface
 * All initialization tasks must inherit from this interface
 */
class TAPCORE_API ITapInitTask
{
public:
	virtual ~ITapInitTask() = default;

	/** Task unique identifier (class name) */
	virtual FName GetTaskID() const = 0;

	/** Dependencies task list */
	virtual TArray<FName> GetDependencies() const = 0;

	/** Priority (lower number = higher priority) */
	virtual int32 GetPriority() const = 0;

	/** Can execute in background thread */
	virtual bool CanExecuteInBackground() const { return false; }

	/** Only execute in development builds */
	virtual bool IsDevOnly() const { return false; }

	/** Execute initialization */
	virtual void Execute(const FTapTapSdkOptions& Config) = 0;

	/** Display name for logging */
	virtual FString GetDisplayName() const { return GetTaskID().ToString(); }
};

// Forward declaration
class FTapInitTaskRegistry;

/**
 * Task Configuration Helper
 */
struct FTapTaskConfig
{
	int32 Priority = 100;
	TArray<FName> Dependencies;
	bool bCanRunInBackground = false;
	bool bIsDevOnly = false;
};

/**
 * Auto Register Helper
 */
template<typename T>
struct FTapInitTaskAutoRegister
{
	FTapInitTaskAutoRegister();
};

/**
 * Initialization Task Declaration Macro
 *
 * Usage Example:
 *
 * // In .h file
 * DECLARE_TAP_INIT_TASK(FTapLoginInitTask)
 *
 * // In .cpp file
 * IMPLEMENT_TAP_INIT_TASK(FTapLoginInitTask)
 *     .WithPriority(10)
 *     .DependsOn(TEXT("FTapGIDInitTask"), TEXT("FTapDBInitTask"))
 *     .CanRunInBackground(false);
 *
 * void FTapLoginInitTask::Execute(const FTapTapSdkOptions& Config)
 * {
 *     // Your initialization code here
 * }
 */
#define DECLARE_TAP_INIT_TASK(ClassName) \
	class ClassName final : public ITapInitTask \
	{ \
	private: \
		static FTapTaskConfig TaskConfig; \
		static ClassName StaticInstance; \
		static FTapInitTaskAutoRegister<ClassName> AutoRegister; \
		\
	public: \
		virtual FName GetTaskID() const override { return #ClassName; } \
		virtual TArray<FName> GetDependencies() const override { return TaskConfig.Dependencies; } \
		virtual int32 GetPriority() const override { return TaskConfig.Priority; } \
		virtual bool CanExecuteInBackground() const override { return TaskConfig.bCanRunInBackground; } \
		virtual bool IsDevOnly() const override { return TaskConfig.bIsDevOnly; } \
		virtual void Execute(const FTapTapSdkOptions& Config) override; \
		\
		static ClassName& WithPriority(int32 Priority) \
		{ \
			TaskConfig.Priority = Priority; \
			return StaticInstance; \
		} \
		\
		template<typename... Args> \
		static ClassName& DependsOn(Args... Deps) \
		{ \
			TaskConfig.Dependencies = {FName(Deps)...}; \
			return StaticInstance; \
		} \
		\
		static ClassName& CanRunInBackground(bool bCan) \
		{ \
			TaskConfig.bCanRunInBackground = bCan; \
			return StaticInstance; \
		} \
		\
		static ClassName& DevOnly(bool bDevOnly) \
		{ \
			TaskConfig.bIsDevOnly = bDevOnly; \
			return StaticInstance; \
		} \
	};

/**
 * Initialization Task Implementation Macro
 */
#define IMPLEMENT_TAP_INIT_TASK(ClassName) \
	FTapTaskConfig ClassName::TaskConfig; \
	ClassName ClassName::StaticInstance; \
	FTapInitTaskAutoRegister<ClassName> ClassName::AutoRegister; \
	ClassName
