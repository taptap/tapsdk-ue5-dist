// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Platform Interface for TapEvent
 * All platform-specific implementations must inherit from this interface
 */
class ITapEventPlatform
{
public:
	virtual ~ITapEventPlatform() = default;

	/**
	 * Initialize TapEvent
	 * @param bEnableLog Whether to enable logging
	 */
	virtual void Init(bool bEnableLog) = 0;

	/**
	 * Set user ID
	 * @param UserID User unique identifier
	 */
	virtual void SetUserID(const FString& UserID) = 0;

	/**
	 * Set user ID with properties
	 * @param UserID User unique identifier
	 * @param Properties JSON format user properties
	 */
	virtual void SetUserID(const FString& UserID, const FString& Properties) = 0;

	/**
	 * Clear current user
	 */
	virtual void ClearUser() = 0;

	/**
	 * Get device ID
	 * @return Device unique identifier
	 */
	virtual FString GetDeviceId() = 0;

	/**
	 * Log custom event
	 * @param Name Event name
	 * @param Properties JSON format event properties
	 */
	virtual void LogEvent(const FString& Name, const FString& Properties) = 0;

	/**
	 * Initialize device properties
	 * @param Properties JSON format device properties
	 */
	virtual void DeviceInitialize(const FString& Properties) = 0;

	/**
	 * Update device properties
	 * @param Properties JSON format device properties
	 */
	virtual void DeviceUpdate(const FString& Properties) = 0;

	/**
	 * Add device properties
	 * @param Properties JSON format device properties
	 */
	virtual void DeviceAdd(const FString& Properties) = 0;

	/**
	 * Initialize user properties
	 * @param Properties JSON format user properties
	 */
	virtual void UserInitialize(const FString& Properties) = 0;

	/**
	 * Update user properties
	 * @param Properties JSON format user properties
	 */
	virtual void UserUpdate(const FString& Properties) = 0;

	/**
	 * Add user properties
	 * @param Properties JSON format user properties
	 */
	virtual void UserAdd(const FString& Properties) = 0;

	/**
	 * Add common property (single)
	 * @param Key Property key
	 * @param Value Property value
	 */
	virtual void AddCommonProperty(const FString& Key, const FString& Value) = 0;

	/**
	 * Add common properties (batch)
	 * @param Properties JSON format common properties
	 */
	virtual void AddCommon(const FString& Properties) = 0;

	/**
	 * Clear single common property
	 * @param Key Property key
	 */
	virtual void ClearCommonProperty(const FString& Key) = 0;

	/**
	 * Clear multiple common properties
	 * @param Keys Property keys array
	 */
	virtual void ClearCommonProperties(const TArray<FString>& Keys) = 0;

	/**
	 * Clear all common properties
	 */
	virtual void ClearAllCommonProperties() = 0;

	/**
	 * Log purchase event
	 * @param OrderJson JSON string containing order information
	 *        Format: {"orderId":"xxx","productName":"xxx","amount":100,"currencyType":"CNY","paymentMethod":"alipay","properties":{...}}
	 */
	virtual void LogPurchasedEvent(const FString& OrderJson) = 0;

	/**
	 * Register dynamic properties callback
	 * @param Callback Callback function returning JSON format dynamic properties
	 */
	virtual void RegisterDynamicProperties(TFunction<FString()> Callback) = 0;

	/**
	 * Set OAID (Android advertising identifier)
	 * @param Value OAID value
	 */
	virtual void SetOAID(const FString& Value) = 0;

	/**
	 * Log device login event
	 */
	virtual void LogDeviceLoginEvent() = 0;
};
