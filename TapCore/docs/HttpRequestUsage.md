# TapSDK HTTP Request Tool - Usage Guide

## Overview

The TapCore module provides a powerful HTTP request utility (`FTapHttpRequest`) with built-in support for common parameters, request signing, and JSON handling. This tool simplifies network communication in UE5 projects.

## Features

- ✅ RESTful API support (GET, POST, PUT, DELETE)
- ✅ Automatic JSON serialization/deserialization
- ✅ Async callback-based design
- ✅ Global default headers
- ✅ **Common parameters (公参)** - Automatic injection of TapSDK standard headers and query params
- ✅ **Request signing** - HMAC-SHA256 signature generation for security
- ✅ Device identification and platform detection
- ✅ Configurable timeout
- ✅ Detailed logging

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Basic Usage](#basic-usage)
3. [Common Parameters](#common-parameters)
4. [Request Signing](#request-signing)
5. [Advanced Features](#advanced-features)
6. [Best Practices](#best-practices)
7. [API Reference](#api-reference)

---

## Quick Start

### Basic GET Request

```cpp
#include "TapHttpRequest.h"

void UMyClass::FetchUserData()
{
    FString URL = TEXT("https://api.example.com/user/123");

    FTapHttpRequest::Get(URL, FTapHttpRequestCompleteDelegate::CreateUObject(
        this, &UMyClass::OnUserDataReceived));
}

void UMyClass::OnUserDataReceived(const FTapHttpResult& Result)
{
    if (Result.IsSuccess())
    {
        UE_LOG(LogTemp, Log, TEXT("Response: %s"), *Result.Content);

        // Access JSON data
        if (Result.JsonObject.IsValid())
        {
            FString UserName = Result.JsonObject->GetStringField(TEXT("name"));
            int32 UserLevel = Result.JsonObject->GetIntegerField(TEXT("level"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Request failed: %s"), *Result.ErrorMessage);
    }
}
```

### Basic POST Request

```cpp
void UMyClass::CreateUser()
{
    FString URL = TEXT("https://api.example.com/users");

    // Create JSON object
    TSharedPtr<FJsonObject> JsonData = MakeShared<FJsonObject>();
    JsonData->SetStringField(TEXT("name"), TEXT("Alice"));
    JsonData->SetNumberField(TEXT("age"), 25);

    FTapHttpRequest::PostJson(URL, JsonData, FTapHttpRequestCompleteDelegate::CreateUObject(
        this, &UMyClass::OnUserCreated));
}
```

---

## Basic Usage

### HTTP Methods

#### GET - Retrieve Data

```cpp
FTapHttpRequest::Get(
    TEXT("https://api.example.com/data"),
    FTapHttpRequestCompleteDelegate::CreateLambda([](const FTapHttpResult& Result)
    {
        if (Result.IsSuccess())
        {
            // Handle response
        }
    })
);
```

#### POST - Submit Data (JSON String)

```cpp
FString JsonBody = TEXT("{\"key\":\"value\"}");
FTapHttpRequest::Post(
    TEXT("https://api.example.com/submit"),
    JsonBody,
    Callback
);
```

#### POST - Submit Data (JSON Object)

```cpp
TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
JsonObject->SetStringField(TEXT("username"), TEXT("player1"));
JsonObject->SetNumberField(TEXT("score"), 1000);

FTapHttpRequest::PostJson(
    TEXT("https://api.example.com/score"),
    JsonObject,
    Callback
);
```

#### PUT - Update Data

```cpp
FString JsonBody = TEXT("{\"status\":\"active\"}");
FTapHttpRequest::Put(
    TEXT("https://api.example.com/user/123"),
    JsonBody,
    Callback
);
```

#### DELETE - Remove Data

```cpp
FTapHttpRequest::Delete(
    TEXT("https://api.example.com/user/123"),
    Callback
);
```

#### Custom Request with Headers

```cpp
TMap<FString, FString> Headers;
Headers.Add(TEXT("X-Custom-Header"), TEXT("CustomValue"));
Headers.Add(TEXT("Authorization"), TEXT("Bearer token123"));

FTapHttpRequest::Request(
    TEXT("PATCH"),
    TEXT("https://api.example.com/resource"),
    Headers,
    TEXT("{\"field\":\"value\"}"),
    Callback,
    60.0f  // 60 seconds timeout
);
```

### Working with Response

```cpp
void OnRequestComplete(const FTapHttpResult& Result)
{
    // Check success
    if (Result.IsSuccess())
    {
        UE_LOG(LogTemp, Log, TEXT("Status: %d"), Result.StatusCode);

        // Get raw content
        FString Content = Result.Content;

        // Access JSON object
        if (Result.JsonObject.IsValid())
        {
            // String field
            FString Name = Result.JsonObject->GetStringField(TEXT("name"));

            // Number field
            int32 Count = Result.JsonObject->GetIntegerField(TEXT("count"));
            double Price = Result.JsonObject->GetNumberField(TEXT("price"));

            // Boolean field
            bool IsActive = Result.JsonObject->GetBoolField(TEXT("active"));

            // Array field
            TArray<TSharedPtr<FJsonValue>> Items = Result.JsonObject->GetArrayField(TEXT("items"));

            // Nested object
            TSharedPtr<FJsonObject> UserObject = Result.JsonObject->GetObjectField(TEXT("user"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed: %s (Status: %d)"),
            *Result.ErrorMessage, Result.StatusCode);
    }
}
```

---

## Common Parameters

Common parameters (公参) are automatically added to all HTTP requests when enabled. They provide standard information about the SDK, device, and platform.

### Automatic Initialization

Common parameters are **automatically initialized** when you call `FTapTapSDK::Init()`:

```cpp
FTapTapSdkOptions Config;
Config.ClientID = TEXT("your_client_id");
Config.ClientToken = TEXT("your_client_token");
FTapTapSDK::Init(Config);

// Common parameters are now enabled automatically!
```

### Common Headers Added

The following headers are automatically added to all requests:

| Header | Description | Example |
|--------|-------------|---------|
| `X-Tap-PN` | Package name | `TapSDK` |
| `X-Tap-Device-Id` | Unique device ID | `550e8400-e29b-41d4-a716-446655440000` |
| `X-Tap-Platform` | Platform name | `Windows`, `Mac`, `iOS`, `Android` |
| `X-Tap-SDK-Module` | Current module name | `TapCore`, `TapLogin` |
| `X-Tap-SDK-Module-Version` | Module version | `1.0.0` |
| `X-Tap-SDK-Artifact` | SDK type | `UE5` |
| `X-Tap-Ts` | Unix timestamp | `1704096000` |
| `X-Tap-Nonce` | Random string (10 chars) | `a8f3Kx9mQ2` |
| `X-Tap-Lang` | Language code | `en_US`, `zh_CN` |
| `User-Agent` | User agent string | `TapSDK-UE5/1.0.0` |

### Common Query Parameters Added

| Parameter | Description |
|-----------|-------------|
| `client_id` | Client ID from config |

### Optional Parameters

You can set optional parameters that will be included when available:

#### Game User ID

```cpp
// Set game user ID (added to X-Tap-SDK-Game-User-Id header)
FTapHttpCommonParams::SetGameUserID(TEXT("user_12345"));

// Clear game user ID
FTapHttpCommonParams::ClearGameUserID();
```

#### Authorization Token

```cpp
// Set authorization token (added to Authorization header)
FTapHttpCommonParams::SetAuthorizationToken(TEXT("Bearer eyJhbGc..."));

// Clear authorization token
FTapHttpCommonParams::ClearAuthorizationToken();
```

### Manual Control

If you need to disable common parameters for specific modules:

```cpp
// Disable common parameters
FTapHttpRequest::SetEnableCommonParams(false);

// Re-enable later
FTapHttpRequest::SetEnableCommonParams(true);
```

### Module-Specific Configuration

Different modules can set their own module information:

```cpp
// In TapLogin module
FTapHttpRequest::SetModuleInfo(TEXT("TapLogin"), TEXT("1.2.0"));

// In TapAchievement module
FTapHttpRequest::SetModuleInfo(TEXT("TapAchievement"), TEXT("1.1.0"));
```

---

## Request Signing

Request signing adds a cryptographic signature to verify request integrity and authenticity.

### How It Works

1. The SDK collects request information: Method, URL, Headers, Body
2. Constructs a signature string using this format:
   ```
   METHOD\n
   PATH_AND_QUERY\n
   SORTED_X_TAP_HEADERS\n
   BODY\n
   ```
3. Generates HMAC-SHA256 hash using ClientToken as the key
4. Encodes the hash as Base64
5. Adds the signature as `X-Tap-Sign` header

### Automatic Signing

Signing is **automatically enabled** when you initialize TapSDK:

```cpp
FTapTapSDK::Init(Config);  // Signing is now enabled
```

### Manual Control

```cpp
// Disable signing
FTapHttpRequest::SetEnableSigning(false);

// Re-enable signing
FTapHttpRequest::SetEnableSigning(true);
```

### Security Notes

- ⚠️ **ClientToken must be kept secret** - Never expose it in client code
- ✅ Signing helps prevent request tampering
- ✅ Server can verify the signature using the same algorithm
- ⚠️ Signing requires common parameters to be enabled

---

## Advanced Features

### Default Headers

Set headers that apply to all requests:

```cpp
// Add a default header
FTapHttpRequest::AddDefaultHeader(TEXT("X-Game-Version"), TEXT("1.5.0"));
FTapHttpRequest::AddDefaultHeader(TEXT("X-API-Key"), TEXT("your_api_key"));

// Set multiple default headers at once
TMap<FString, FString> DefaultHeaders;
DefaultHeaders.Add(TEXT("X-Game-Version"), TEXT("1.5.0"));
DefaultHeaders.Add(TEXT("X-API-Key"), TEXT("your_api_key"));
FTapHttpRequest::SetDefaultHeaders(DefaultHeaders);

// Clear all default headers
FTapHttpRequest::ClearDefaultHeaders();
```

### Header Priority

Headers are merged in this order (later overwrites earlier):

1. **Common headers** (from common parameters)
2. **Default headers** (set via SetDefaultHeaders/AddDefaultHeader)
3. **Custom headers** (passed to individual request)

```cpp
// Example showing header priority
FTapHttpRequest::AddDefaultHeader(TEXT("X-Custom"), TEXT("Default"));

TMap<FString, FString> CustomHeaders;
CustomHeaders.Add(TEXT("X-Custom"), TEXT("Override"));  // This will override

FTapHttpRequest::Request(TEXT("GET"), URL, CustomHeaders, TEXT(""), Callback);
// Result: X-Custom = "Override"
```

### Custom Timeout

```cpp
// 60 seconds timeout
FTapHttpRequest::Get(URL, Callback, 60.0f);

// 10 seconds timeout for quick requests
FTapHttpRequest::Post(URL, JsonBody, Callback, 10.0f);
```

### Lambda Callbacks

```cpp
FTapHttpRequest::Get(URL, FTapHttpRequestCompleteDelegate::CreateLambda(
    [](const FTapHttpResult& Result)
    {
        if (Result.IsSuccess())
        {
            UE_LOG(LogTemp, Log, TEXT("Success: %s"), *Result.Content);
        }
    }
));
```

### UObject Method Callbacks

```cpp
FTapHttpRequest::Get(URL, FTapHttpRequestCompleteDelegate::CreateUObject(
    this, &UMyClass::OnRequestComplete));
```

### Static Function Callbacks

```cpp
static void OnRequestComplete(const FTapHttpResult& Result) { /* ... */ }

FTapHttpRequest::Get(URL, FTapHttpRequestCompleteDelegate::CreateStatic(
    &UMyClass::OnRequestComplete));
```

---

## Best Practices

### 1. Always Check IsSuccess()

```cpp
void OnResponse(const FTapHttpResult& Result)
{
    if (Result.IsSuccess())
    {
        // Handle successful response (2xx status code)
    }
    else
    {
        // Handle error
        UE_LOG(LogTemp, Warning, TEXT("HTTP %d: %s"),
            Result.StatusCode, *Result.ErrorMessage);
    }
}
```

### 2. Validate JSON Before Access

```cpp
if (Result.JsonObject.IsValid())
{
    // Safe to access fields
    if (Result.JsonObject->HasField(TEXT("data")))
    {
        FString Data = Result.JsonObject->GetStringField(TEXT("data"));
    }
}
```

### 3. Use Appropriate Timeouts

```cpp
// Short timeout for latency-sensitive requests
FTapHttpRequest::Get(QuickURL, Callback, 5.0f);

// Longer timeout for file uploads or slow endpoints
FTapHttpRequest::Post(UploadURL, LargeData, Callback, 120.0f);
```

### 4. Handle Network Errors Gracefully

```cpp
void OnResponse(const FTapHttpResult& Result)
{
    if (!Result.bSuccess)
    {
        // Network error (no response received)
        ShowErrorMessage(TEXT("Network error. Please check your connection."));
        return;
    }

    if (Result.StatusCode == 401)
    {
        // Unauthorized - need to re-login
        HandleAuthenticationFailure();
    }
    else if (Result.StatusCode >= 500)
    {
        // Server error
        ShowErrorMessage(TEXT("Server is temporarily unavailable. Please try again later."));
    }
    else if (Result.IsSuccess())
    {
        // Success - process data
        ProcessData(Result);
    }
}
```

### 5. Clean Up Resources

```cpp
// Lambda captures should use weak pointers for UObjects
TWeakObjectPtr<UMyClass> WeakThis(this);
FTapHttpRequest::Get(URL, FTapHttpRequestCompleteDelegate::CreateLambda(
    [WeakThis](const FTapHttpResult& Result)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->HandleResponse(Result);
        }
    }
));
```

### 6. Log Appropriately

The SDK provides detailed logging via `LogTapHttp`. Control verbosity in your project settings:

- `Log` - Basic request/response info
- `Verbose` - Full request/response details including headers and body
- `Warning` - Errors and issues
- `Error` - Critical failures

---

## API Reference

### FTapHttpResult

Response structure containing request result data.

```cpp
struct FTapHttpResult
{
    bool bSuccess;                          // Network request succeeded
    int32 StatusCode;                        // HTTP status code
    FString Content;                         // Response body as string
    TSharedPtr<FJsonObject> JsonObject;     // Parsed JSON (if valid)
    FString ErrorMessage;                    // Error description

    bool IsSuccess() const;                  // Returns true if 2xx status
};
```

### FTapHttpRequest

Main HTTP request utility class.

#### Static Methods

```cpp
// GET request
static void Get(
    const FString& URL,
    const FTapHttpRequestCompleteDelegate& OnComplete,
    float Timeout = 30.0f
);

// POST request (JSON string)
static void Post(
    const FString& URL,
    const FString& JsonBody,
    const FTapHttpRequestCompleteDelegate& OnComplete,
    float Timeout = 30.0f
);

// POST request (JSON object)
static void PostJson(
    const FString& URL,
    const TSharedPtr<FJsonObject>& JsonObject,
    const FTapHttpRequestCompleteDelegate& OnComplete,
    float Timeout = 30.0f
);

// PUT request
static void Put(
    const FString& URL,
    const FString& JsonBody,
    const FTapHttpRequestCompleteDelegate& OnComplete,
    float Timeout = 30.0f
);

// DELETE request
static void Delete(
    const FString& URL,
    const FTapHttpRequestCompleteDelegate& OnComplete,
    float Timeout = 30.0f
);

// Custom request
static void Request(
    const FString& Method,
    const FString& URL,
    const TMap<FString, FString>& Headers,
    const FString& Body,
    const FTapHttpRequestCompleteDelegate& OnComplete,
    float Timeout = 30.0f
);

// Default headers
static void SetDefaultHeaders(const TMap<FString, FString>& Headers);
static void AddDefaultHeader(const FString& Key, const FString& Value);
static void ClearDefaultHeaders();

// Common parameters control
static void SetEnableCommonParams(bool bEnable);
static void SetModuleInfo(const FString& ModuleName, const FString& ModuleVersion);
static void SetEnableSigning(bool bEnable);
```

### FTapHttpCommonParams

Common parameters manager.

```cpp
// Initialization (called automatically by FTapTapSDK::Init)
static void Initialize(const FString& ClientID, const FString& ClientToken);

// Get common parameters
static TMap<FString, FString> GetCommonHeaders(
    const FString& ModuleName,
    const FString& ModuleVersion,
    bool bEnableAuthorization = false
);
static TMap<FString, FString> GetCommonQueryParams();

// Signature generation
static void AddSignature(
    const FString& Method,
    const FString& URL,
    TMap<FString, FString>& Headers,
    const FString& Body = TEXT("")
);

// Optional parameters
static void SetGameUserID(const FString& UserID);
static void ClearGameUserID();
static void SetAuthorizationToken(const FString& Token);
static void ClearAuthorizationToken();

// Device info
static FString GetDeviceID();
static FString GetSDKVersion();
```

---

## Examples

### Complete Login Flow

```cpp
void ULoginManager::Login(const FString& Username, const FString& Password)
{
    FString URL = TEXT("https://api.taptap.com/v1/auth/login");

    TSharedPtr<FJsonObject> LoginData = MakeShared<FJsonObject>();
    LoginData->SetStringField(TEXT("username"), Username);
    LoginData->SetStringField(TEXT("password"), Password);

    FTapHttpRequest::PostJson(URL, LoginData,
        FTapHttpRequestCompleteDelegate::CreateUObject(this, &ULoginManager::OnLoginResponse));
}

void ULoginManager::OnLoginResponse(const FTapHttpResult& Result)
{
    if (Result.IsSuccess() && Result.JsonObject.IsValid())
    {
        FString AccessToken = Result.JsonObject->GetStringField(TEXT("access_token"));
        FString RefreshToken = Result.JsonObject->GetStringField(TEXT("refresh_token"));

        // Store tokens
        SaveTokens(AccessToken, RefreshToken);

        // Set authorization for future requests
        FTapHttpCommonParams::SetAuthorizationToken(
            FString::Printf(TEXT("Bearer %s"), *AccessToken));

        // Notify login success
        OnLoginSucceeded.Broadcast();
    }
    else
    {
        OnLoginFailed.Broadcast(Result.ErrorMessage);
    }
}
```

### Fetch Leaderboard

```cpp
void ULeaderboardManager::FetchLeaderboard()
{
    FString URL = TEXT("https://api.taptap.com/v1/leaderboard?limit=10");

    FTapHttpRequest::Get(URL,
        FTapHttpRequestCompleteDelegate::CreateUObject(this, &ULeaderboardManager::OnLeaderboardReceived));
}

void ULeaderboardManager::OnLeaderboardReceived(const FTapHttpResult& Result)
{
    if (Result.IsSuccess() && Result.JsonObject.IsValid())
    {
        TArray<TSharedPtr<FJsonValue>> Entries = Result.JsonObject->GetArrayField(TEXT("entries"));

        for (const TSharedPtr<FJsonValue>& Entry : Entries)
        {
            TSharedPtr<FJsonObject> EntryObj = Entry->AsObject();
            FString PlayerName = EntryObj->GetStringField(TEXT("name"));
            int32 Score = EntryObj->GetIntegerField(TEXT("score"));

            // Display in UI
            AddLeaderboardEntry(PlayerName, Score);
        }
    }
}
```

### Upload Player Progress

```cpp
void UProgressManager::SaveProgress(int32 Level, int32 Score)
{
    FString URL = TEXT("https://api.taptap.com/v1/progress");

    TSharedPtr<FJsonObject> ProgressData = MakeShared<FJsonObject>();
    ProgressData->SetNumberField(TEXT("level"), Level);
    ProgressData->SetNumberField(TEXT("score"), Score);
    ProgressData->SetStringField(TEXT("timestamp"),
        FDateTime::UtcNow().ToIso8601());

    FTapHttpRequest::PostJson(URL, ProgressData,
        FTapHttpRequestCompleteDelegate::CreateUObject(this, &UProgressManager::OnProgressSaved));
}

void UProgressManager::OnProgressSaved(const FTapHttpResult& Result)
{
    if (Result.IsSuccess())
    {
        UE_LOG(LogTemp, Log, TEXT("Progress saved successfully"));
        ShowNotification(TEXT("Progress saved!"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to save progress: %s"), *Result.ErrorMessage);
        ShowNotification(TEXT("Failed to save progress. Will retry later."));

        // Queue for retry
        QueueProgressForRetry();
    }
}
```

---

## Troubleshooting

### Request Fails Immediately

- Check that HTTP module is enabled in your project
- Verify URL is valid and reachable
- Check firewall settings

### JSON Parsing Fails

- Verify response content type is `application/json`
- Check that response body is valid JSON
- Use Result.Content to inspect raw response

### Signature Verification Fails

- Ensure ClientToken is correct
- Verify server is using the same signature algorithm
- Check that all X-Tap-* headers are included in signature calculation

### Common Parameters Not Working

- Verify `FTapTapSDK::Init()` was called
- Check that `SetEnableCommonParams(true)` is set
- Ensure ClientID and ClientToken are not empty

---

## Support

For more information, see:
- TapSDK Documentation: https://developer.taptap.com
- GitHub Issues: https://github.com/taptap/TapSDK-UE5/issues
- API Reference: `TapCore/Public/TapHttpRequest.h`
