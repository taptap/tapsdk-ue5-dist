// Copyright TapTap. All Rights Reserved.
#pragma once

// No engine dependency: the production file locking and publication protocol
// is also exercised by the standalone subprocess tests.
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <string>
#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace TapSharedCacheFileNative
{
inline std::filesystem::path NativePath(const std::string& Path)
{
#if defined(__cpp_char8_t)
	auto Result = std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(Path.data()), Path.size()));
#else
	auto Result = std::filesystem::u8path(Path);
#endif
#if defined(_WIN32)
	Result.make_preferred();
	const auto& Wide = Result.native();
	// UE's Windows file layer supports extended paths; native operations must
	// preserve that support when the project/Saved path exceeds MAX_PATH.
	if (Wide.size() >= MAX_PATH && Result.is_absolute() && Wide.compare(0, 4, L"\\\\?\\") != 0)
	{
		return std::filesystem::path(Wide.compare(0, 2, L"\\\\") == 0
			? L"\\\\?\\UNC\\" + Wide.substr(2) : L"\\\\?\\" + Wide);
	}
#endif
	return Result;
}

inline bool MakeDirectories(const std::string& Path)
{
	std::error_code Error;
	std::filesystem::create_directories(NativePath(Path), Error);
	return !Error;
}

enum class EAcquire { Acquired, Busy, Error };

class FLease
{
public:
	FLease() = default;
	FLease(const FLease&) = delete;
	FLease& operator=(const FLease&) = delete;
	~FLease()
	{
#if defined(_WIN32)
		if (Handle != INVALID_HANDLE_VALUE) CloseHandle(Handle);
#else
		if (Handle >= 0) close(Handle);
#endif
	}

	EAcquire Acquire(const std::string& Path)
	{
#if defined(_WIN32)
		Handle = CreateFileW(NativePath(Path).c_str(), GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (Handle == INVALID_HANDLE_VALUE) return EAcquire::Error;
		OVERLAPPED Position = {};
		if (LockFileEx(Handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &Position))
			return EAcquire::Acquired;
		const DWORD Error = GetLastError();
		CloseHandle(Handle);
		Handle = INVALID_HANDLE_VALUE;
		return Error == ERROR_LOCK_VIOLATION ? EAcquire::Busy : EAcquire::Error;
#else
		Handle = open(Path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
		if (Handle < 0) return EAcquire::Error;
		if (flock(Handle, LOCK_EX | LOCK_NB) == 0) return EAcquire::Acquired;
		const int Error = errno;
		close(Handle);
		Handle = -1;
		return Error == EWOULDBLOCK || Error == EAGAIN ? EAcquire::Busy : EAcquire::Error;
#endif
	}

private:
#if defined(_WIN32)
	HANDLE Handle = INVALID_HANDLE_VALUE;
#else
	int Handle = -1;
#endif
};

// Publish only a complete file. Failure leaves the previous file intact.
inline bool Write(const std::string& Path, const std::string& Contents)
{
	static std::atomic<unsigned long long> Sequence{0};
	const auto Target = NativePath(Path);
	std::error_code Error;
	std::filesystem::create_directories(Target.parent_path(), Error);
	if (Error) return false;
#if defined(_WIN32)
	const auto ProcessId = GetCurrentProcessId();
#else
	const auto ProcessId = getpid();
#endif
	for (int Attempt = 0; Attempt < 32; ++Attempt)
	{
		const auto Temp = NativePath(Path + ".tmp." + std::to_string(ProcessId) + "." + std::to_string(Sequence++));
#if defined(_WIN32)
		HANDLE File = CreateFileW(Temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (File == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() == ERROR_FILE_EXISTS) continue;
			return false;
		}
		bool Success = true;
		size_t Offset = 0;
		while (Offset < Contents.size())
		{
			DWORD Written = 0;
			const DWORD Size = static_cast<DWORD>((Contents.size() - Offset) > 1048576 ? 1048576 : Contents.size() - Offset);
			if (!WriteFile(File, Contents.data() + Offset, Size, &Written, nullptr) || Written == 0)
			{ Success = false; break; }
			Offset += Written;
		}
		// Match the normal Windows cache write policy: close the completed file
		// before rename, without adding a synchronous disk flush to every event.
		Success = CloseHandle(File) != 0 && Success;
		Success = Success && MoveFileExW(Temp.c_str(), Target.c_str(),
			MOVEFILE_REPLACE_EXISTING) != 0;
		DeleteFileW(Temp.c_str());
#else
		const int File = open(Temp.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
		if (File < 0)
		{
			if (errno == EEXIST) continue;
			return false;
		}
		bool Success = true;
		size_t Offset = 0;
		while (Offset < Contents.size())
		{
			const ssize_t Written = write(File, Contents.data() + Offset, Contents.size() - Offset);
			if (Written < 0 && errno == EINTR) continue;
			if (Written <= 0) { Success = false; break; }
			Offset += static_cast<size_t>(Written);
		}
		Success = fsync(File) == 0 && Success;
		Success = close(File) == 0 && Success;
		Success = Success && rename(Temp.c_str(), Target.c_str()) == 0;
		unlink(Temp.c_str());
#endif
		return Success;
	}
	return false;
}
}
