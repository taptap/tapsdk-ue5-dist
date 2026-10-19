// Copyright TapTap. All Rights Reserved.
// C interface for taptap_api.dll — mirrors Unity TapClientBridge + TapLicenseClientBridge
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ── Launcher verification ──────────────────────────────────────────────────

// Returns true if the game needs to be restarted via TapTap launcher.
// Call first. If true → call RequestExit, do not call TapSDK_Init.
bool  TapSDK_RestartAppIfNecessary(const char* clientId);

// Initializes the SDK with the game's public key.
// Returns ETapLauncherInitResult value (0 = OK).
// errMsg must be a caller-allocated buffer of at least 1024 bytes.
int   TapSDK_Init(char* errMsg, const char* pubKey);

// Releases all SDK resources. Call on shutdown.
void  TapSDK_Shutdown();

// Fills clientId buffer (caller-allocated, at least 256 bytes).
// Returns true on success.
bool  TapSDK_GetClientID(char* clientId);

// Fills openId buffer (caller-allocated, at least 256 bytes).
// Returns true on success.
bool  TapUser_GetOpenID(char* openId);

// Processes pending native callbacks. Must be called every frame on the game thread.
void  TapSDK_RunCallbacks();

// Registers a native callback for the given eventId.
// Callback signature: void fn(int eventId, void* userData)
typedef void (*TapCallbackFn)(int eventId, void* userData);
void  TapSDK_RegisterCallback(int callbackId, TapCallbackFn callback);
void  TapSDK_UnregisterCallback(int callbackId, TapCallbackFn callback);

// ── User authorization (Unity TapLoginClientBridge) ─────────────────────────

// Result of TapUser_AsyncAuthorize_internal.
// 0 = unknown, 1 = request started, 2 = failed, 3 = already in flight.
typedef enum TapUser_AsyncAuthorize_Result {
    TapUser_AsyncAuthorize_Unknown   = 0,
    TapUser_AsyncAuthorize_OK        = 1,
    TapUser_AsyncAuthorize_Failed    = 2,
    TapUser_AsyncAuthorize_InFlight  = 3,
} TapUser_AsyncAuthorize_Result;

// Starts authorization in the TapTap PC client. This is an internal API used by
// the Unity Standalone login bridge; all strings are UTF-8 and the call is cdecl.
int TapUser_AsyncAuthorize_internal(
    const char* scopeStrings,
    const char* responseType,
    const char* redirectUri,
    const char* codeChallenge,
    const char* state,
    const char* codeChallengeMethod,
    const char* versionCode,
    const char* sdkUa,
    const char* info);

// EventId 2001: authorization finished. The callback URI is owned by the DLL
// and is valid only while the native callback is executing.
#pragma pack(push, 8)
typedef struct {
    int  is_cancel;
    char callback_uri[1024];
} TapAuthorizeFinishedInternal;
#pragma pack(pop)

// ── License / DLC ──────────────────────────────────────────────────────────

// Returns true if the current user owns the game.
bool  TapApps_IsOwned();

// Returns true if the current user owns the given DLC SKU.
bool  TapDLC_IsOwned(const char* skuId);

// Opens the store page for the given DLC SKU.
// Returns true if the store was opened successfully.
bool  TapDLC_ShowStore(const char* skuId);

// ── Callback payload structs (passed as userData in TapCallbackFn) ─────────

// EventId 4001: GamePlayableStatusChanged
// Fired when the game license ownership changes (purchase / refund).
#pragma pack(push, 1)
typedef struct {
    unsigned char is_playable; // 1 = owned, 0 = not owned
} TapGamePlayableStatusChanged;

// EventId 4002: DLCPlayableStatusChanged
// Fired when a DLC ownership changes (purchase / refund).
typedef struct {
    char          dlc_id[32]; // null-terminated DLC SKU id
    unsigned char is_playable; // 1 = owned, 0 = not owned
} TapDLCPlayableStatusChanged;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif
