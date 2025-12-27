#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AlakazamAuth.generated.h"

USTRUCT(BlueprintType)
struct FAlakazamUsageInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam")
	int32 SecondsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam")
	int32 SecondsLimit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam")
	int32 SecondsRemaining = 0;

	float GetUsagePercent() const
	{
		return SecondsLimit > 0 ? (SecondsUsed / (float)SecondsLimit) * 100.0f : 100.0f;
	}

	bool ShouldWarn() const { return GetUsagePercent() >= 80.0f; }
	bool IsOverLimit() const { return SecondsUsed >= SecondsLimit; }

	FString GetFormattedUsed() const { return FormatTime(SecondsUsed); }
	FString GetFormattedLimit() const { return FormatTime(SecondsLimit); }
	FString GetFormattedRemaining() const { return FormatTime(SecondsRemaining); }

	static FString FormatTime(int32 Seconds)
	{
		if (Seconds < 60)
			return FString::Printf(TEXT("%ds"), Seconds);
		if (Seconds < 3600)
			return FString::Printf(TEXT("%dm %ds"), Seconds / 60, Seconds % 60);
		return FString::Printf(TEXT("%dh %dm"), Seconds / 3600, (Seconds % 3600) / 60);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUsageWarning, const FAlakazamUsageInfo&, UsageInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUsageLimitReached, const FAlakazamUsageInfo&, UsageInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAuthFailed, const FString&, ErrorMessage);

/**
 * Alakazam Auth Manager
 *
 * Handles API key storage, validation, and usage tracking.
 * Access via UAlakazamAuth::Get().
 */
UCLASS()
class ALAKAZAMPORTAL_API UAlakazamAuth : public UObject
{
	GENERATED_BODY()

public:
	UAlakazamAuth();

	/** Get the singleton instance */
	static UAlakazamAuth* Get();

	// === API Key ===

	/** Check if an API key is stored */
	UFUNCTION(BlueprintPure, Category = "Alakazam|Auth")
	bool HasApiKey() const;

	/** Get the stored API key */
	UFUNCTION(BlueprintPure, Category = "Alakazam|Auth")
	FString GetApiKey() const;

	/** Store an API key */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Auth")
	void SetApiKey(const FString& ApiKey);

	/** Clear the stored API key */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Auth")
	void ClearApiKey();

	// === Setup Status ===

	/** Check if first-time setup is complete */
	UFUNCTION(BlueprintPure, Category = "Alakazam|Auth")
	bool IsSetupComplete() const;

	/** Mark setup as complete */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Auth")
	void SetSetupComplete(bool bComplete);

	// === Consent Preferences ===

	UPROPERTY(BlueprintReadWrite, Category = "Alakazam|Consent")
	bool bShareUsageAnalytics = false;

	UPROPERTY(BlueprintReadWrite, Category = "Alakazam|Consent")
	bool bShareDataForTraining = false;

	UPROPERTY(BlueprintReadWrite, Category = "Alakazam|Consent")
	bool bStoreCapturesOnline = false;

	/** Save consent preferences to config */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Consent")
	void SaveConsentPreferences();

	/** Load consent preferences from config */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Consent")
	void LoadConsentPreferences();

	// === Usage Tracking ===

	/** Current usage information */
	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|Usage")
	FAlakazamUsageInfo CurrentUsage;

	/** Update usage info from server response */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Usage")
	void UpdateUsage(int32 SecondsUsed, int32 SecondsLimit, int32 SecondsRemaining);

	/** Handle server warning */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Usage")
	void HandleWarning(const FString& Warning);

	/** Handle auth failure */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Usage")
	void HandleAuthFailed(const FString& Message);

	// === Events ===

	/** Fired when usage reaches 80% or more */
	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnUsageWarning OnUsageWarning;

	/** Fired when usage reaches 100% */
	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnUsageLimitReached OnUsageLimitReached;

	/** Fired when authentication fails */
	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnAuthFailed OnAuthFailed;

	// === Helpers ===

	/** Open the Alakazam account page in browser */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Auth")
	static void OpenAccountPage();

	/** Open the Alakazam signup page in browser */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Auth")
	static void OpenSignupPage();

private:
	static UAlakazamAuth* Instance;

	static const FString ConfigSection;
	static const FString ApiKeyKey;
	static const FString SetupCompleteKey;
	static const FString ConsentAnalyticsKey;
	static const FString ConsentTrainingKey;
	static const FString ConsentCloudKey;
};
