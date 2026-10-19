// Copyright TapTap. All Rights Reserved.
//
// C header for cloudsave_sdk native library used on Win64 / Mac / Linux desktop targets.
// Signatures mirror Common/CloudSaveSDK/src/cloudsave_sdk.h.
// All pointer return values must be released via TapSdkCloudSaveFreeMemory.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- Logging ---------------------------------------------------------------

typedef void (*TapSdkCppLogWriterDelegate)(
    int         logLevel,
    const char* codeLocation,
    const char* logTag,
    const char* logMessage);

void TapSdkCloudSaveInitLogger(TapSdkCppLogWriterDelegate logWriter);
void TapSdkCloudSaveSetLogLevel(int logLevel, int logToConsole);

// ---- Init / Token ----------------------------------------------------------

// Returns 0 on success, -1 on failure.
int  TapSdkCloudSaveInitialize(const char* cfg);

// Returns 0 on success, -1 on JSON failure. Pass "{}" or null to clear.
int  TapSdkCloudSaveUpdateAccessToken(const char* token);

// Free memory returned by any of the CloudSave* APIs below.
void TapSdkCloudSaveFreeMemory(void* ptr);

// ---- Business APIs ---------------------------------------------------------
// Each returns a UTF-8 JSON string allocated by the SDK; caller must FreeMemory.
// All accept `extraParams` (Tap Miniapp only); UE5 layer passes "" or null.

char* TapSdkCloudSaveCreateArchive(
    const char*          metadata,
    const unsigned char* archiveData,
    int                  archiveDataSize,
    const unsigned char* coverData,
    int                  coverDataSize,
    const char*          extraParams);

char* TapSdkCloudSaveUpdateArchive(
    const char*          archiveUUID,
    const char*          metadata,
    const unsigned char* archiveData,
    int                  archiveDataSize,
    const unsigned char* coverData,
    int                  coverDataSize,
    const char*          extraParams);

char* TapSdkCloudSaveDeleteArchive(
    const char* archiveUUID,
    const char* extraParams);

char* TapSdkCloudSaveGetArchiveList(
    const char* extraParams);

// On success outSize >= 0 and the buffer holds raw bytes.
// On failure outSize < 0 and the buffer holds a UTF-8 JSON error payload.
unsigned char* TapSdkCloudSaveGetArchiveData(
    const char* archiveUUID,
    const char* archiveFileID,
    int64_t*    outSize,
    const char* extraParams);

unsigned char* TapSdkCloudSaveGetArchiveCover(
    const char* archiveUUID,
    const char* archiveFileID,
    int64_t*    outSize,
    const char* extraParams);

// ---- Version / Diagnostics (optional, returns static UTF-8 string) ---------

const char* TapSdkCloudSaveVersion(void);
const char* TapSdkCloudSaveGitCommit(void);

#ifdef __cplusplus
}
#endif
