// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace TapDBConstants
{
	// Server URLs
	static const FString SERVER_URL_CN = TEXT("https://e.tapdb.net");
	static const FString SERVER_URL_IO = TEXT("https://e.tapdb.ap-sg.tapapis.com");
	static const FString API_PATH = TEXT("/v2/batch");

	// Event types
	static const FString TYPE_TRACK = TEXT("track");
	static const FString TYPE_INITIALISE = TEXT("initialise");
	static const FString TYPE_UPDATE = TEXT("update");
	static const FString TYPE_ADD = TEXT("add");

	// Automatic event names
	static const FString EVENT_DEVICE_LOGIN = TEXT("device_login");
	static const FString EVENT_USER_LOGIN = TEXT("user_login");
	static const FString EVENT_PLAY_GAME = TEXT("play_game");
	static const FString EVENT_CHARGE = TEXT("charge");

	// Queue limits
	static constexpr int32 MAX_BATCH_SIZE = 200;
	static constexpr int32 SEND_TRIGGER_COUNT = 50;
	static constexpr float SEND_INTERVAL_SECONDS = 15.0f;

	// Retry
	static constexpr int32 MAX_RETRY = 3;
	static constexpr float RETRY_INTERVAL_SECONDS = 3.0f;

	// Play recorder interval (seconds)
	static constexpr float PLAY_RECORD_INTERVAL_SECONDS = 2.0f;

	// Property keys
	static const FString PROP_OS = TEXT("os");
	static const FString PROP_MODEL = TEXT("md");
	static const FString PROP_OS_VERSION = TEXT("sv");
	static const FString PROP_SDK_VERSION = TEXT("sdk_version");
	static const FString PROP_SESSION_UUID = TEXT("session_uuid");
	static const FString PROP_INSTALL_UUID = TEXT("install_uuid");
	static const FString PROP_PERSIST_UUID = TEXT("persist_uuid");
	static const FString PROP_DEVICE_ID5 = TEXT("device_id5");
	static const FString PROP_RAM = TEXT("ram");
	static const FString PROP_TOTAL_RAM = TEXT("total_ram");
	static const FString PROP_ROM = TEXT("rom");
	static const FString PROP_TOTAL_ROM = TEXT("total_rom");
	static const FString PROP_WIDTH = TEXT("width");
	static const FString PROP_HEIGHT = TEXT("height");
	static const FString PROP_NETWORK_TYPE = TEXT("network_type");
	static const FString PROP_PROVIDER = TEXT("provider");
	static const FString PROP_APP_VERSION = TEXT("app_version");
	static const FString PROP_CHANNEL = TEXT("channel");
	static const FString PROP_T_LOG_ID = TEXT("t_log_id");
	static const FString PROP_TIMESTAMP = TEXT("timestamp");
	static const FString PROP_EVENT_INDEX = TEXT("event_index");
	static const FString PROP_IS_AUTO_LOG = TEXT("is_automatically_log");
	static const FString PROP_SDK_LOCALE = TEXT("sdk_locale");
	static const FString PROP_LANG_SYSTEM = TEXT("lang_system");
	static const FString PROP_OPEN_ID = TEXT("open_id");
	static const FString PROP_HAS_USER = TEXT("has_user");
	static const FString PROP_DURATION = TEXT("duration");
	static const FString PROP_CPU = TEXT("cpu");
	static const FString PROP_OS_TYPE = TEXT("os_type");
	static const FString PROP_HARDWARE = TEXT("hardWare");

	// Storage keys (Prefs)
	static const FString PREF_DEVICE_ID = TEXT("tapdb_unique_id");
	static const FString PREF_PERSISTENT_ID = TEXT("tapdb_persist_id");
	static const FString PREF_INSTALLATION_ID = TEXT("tapdb_install_id");
	static const FString PREF_USER_ID = TEXT("tapdb_played_duration_user_id");
	static const FString PREF_PLAYED_DURATION = TEXT("tapdb_played_duration");

	// Validation
	static const FString USER_ID_PATTERN = TEXT("^[a-zA-Z0-9_+/=.,:]{1,160}$");
	static constexpr int32 MAX_PROPERTY_KEY_LENGTH = 256;
	static constexpr int32 MAX_PROPERTY_VALUE_LENGTH = 256;
	static constexpr int32 MAX_EVENT_NAME_LENGTH = 256;
}
