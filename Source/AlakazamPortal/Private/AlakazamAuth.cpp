#include "AlakazamAuth.h"
#include "AlakazamSettings.h"
#include "HAL/PlatformProcess.h"

UAlakazamAuth* UAlakazamAuth::Instance = nullptr;

const FString UAlakazamAuth::ConfigSection = TEXT("AlakazamPortal");
const FString UAlakazamAuth::ApiKeyKey = TEXT("ApiKey");
const FString UAlakazamAuth::SetupCompleteKey = TEXT("SetupComplete");
const FString UAlakazamAuth::ConsentAnalyticsKey = TEXT("ConsentAnalytics");
const FString UAlakazamAuth::ConsentTrainingKey = TEXT("ConsentTraining");
const FString UAlakazamAuth::ConsentCloudKey = TEXT("ConsentCloud");

UAlakazamAuth::UAlakazamAuth()
{
	LoadConsentPreferences();
}

UAlakazamAuth* UAlakazamAuth::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UAlakazamAuth>();
		Instance->AddToRoot(); // Prevent garbage collection
	}
	return Instance;
}

bool UAlakazamAuth::HasApiKey() const
{
	return !GetApiKey().IsEmpty();
}

FString UAlakazamAuth::GetApiKey() const
{
	// Read from UAlakazamSettings (persists to DefaultGame.ini via UDeveloperSettings)
	return UAlakazamSettings::Get()->ApiKey;
}

void UAlakazamAuth::SetApiKey(const FString& ApiKey)
{
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	Settings->ApiKey = ApiKey;
	Settings->TryUpdateDefaultConfigFile();
	UE_LOG(LogTemp, Log, TEXT("Alakazam: API key saved to config"));
}

void UAlakazamAuth::ClearApiKey()
{
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	Settings->ApiKey = TEXT("");
	Settings->TryUpdateDefaultConfigFile();
	UE_LOG(LogTemp, Log, TEXT("Alakazam: API key cleared"));
}

bool UAlakazamAuth::IsSetupComplete() const
{
	return UAlakazamSettings::Get()->bSetupComplete;
}

void UAlakazamAuth::SetSetupComplete(bool bComplete)
{
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	Settings->bSetupComplete = bComplete;
	Settings->TryUpdateDefaultConfigFile();
}

void UAlakazamAuth::SaveConsentPreferences()
{
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	Settings->bShareUsageAnalytics = bShareUsageAnalytics;
	Settings->bShareDataForTraining = bShareDataForTraining;
	Settings->bStoreCapturesOnline = bStoreCapturesOnline;
	Settings->TryUpdateDefaultConfigFile();
	UE_LOG(LogTemp, Log, TEXT("Alakazam: Consent preferences saved to config"));
}

void UAlakazamAuth::LoadConsentPreferences()
{
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	bShareUsageAnalytics = Settings->bShareUsageAnalytics;
	bShareDataForTraining = Settings->bShareDataForTraining;
	bStoreCapturesOnline = Settings->bStoreCapturesOnline;
}

void UAlakazamAuth::UpdateUsage(int32 SecondsUsed, int32 SecondsLimit, int32 SecondsRemaining)
{
	CurrentUsage.SecondsUsed = SecondsUsed;
	CurrentUsage.SecondsLimit = SecondsLimit;
	CurrentUsage.SecondsRemaining = SecondsRemaining;

	UE_LOG(LogTemp, Log, TEXT("Alakazam: Usage updated: %d/%ds (%.0f%%)"),
		SecondsUsed, SecondsLimit, CurrentUsage.GetUsagePercent());

	if (CurrentUsage.IsOverLimit())
	{
		OnUsageLimitReached.Broadcast(CurrentUsage);
	}
	else if (CurrentUsage.ShouldWarn())
	{
		OnUsageWarning.Broadcast(CurrentUsage);
	}
}

void UAlakazamAuth::HandleWarning(const FString& Warning)
{
	UE_LOG(LogTemp, Warning, TEXT("Alakazam: Server warning: %s"), *Warning);
	OnUsageWarning.Broadcast(CurrentUsage);
}

void UAlakazamAuth::HandleAuthFailed(const FString& Message)
{
	UE_LOG(LogTemp, Error, TEXT("Alakazam: Authentication failed: %s"), *Message);
	OnAuthFailed.Broadcast(Message);
}

void UAlakazamAuth::OpenAccountPage()
{
	FPlatformProcess::LaunchURL(TEXT("https://alakazam.ai/account"), nullptr, nullptr);
}

void UAlakazamAuth::OpenSignupPage()
{
	FPlatformProcess::LaunchURL(TEXT("https://alakazam.ai/signup"), nullptr, nullptr);
}
