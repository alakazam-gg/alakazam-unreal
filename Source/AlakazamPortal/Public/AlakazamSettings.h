#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AlakazamSettings.generated.h"

/**
 * Alakazam Portal Settings
 *
 * Configure your API key and preferences here.
 * Access via Project Settings > Plugins > Alakazam Portal
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Alakazam Portal"))
class ALAKAZAMPORTAL_API UAlakazamSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAlakazamSettings();

	/** Get the singleton settings instance */
	static UAlakazamSettings* Get();

	// === API Configuration ===

	/** Your Alakazam API key. Get one at https://alakazam.ai/account */
	UPROPERTY(config, EditAnywhere, Category = "API", meta = (DisplayName = "API Key"))
	FString ApiKey;

	/** Default server URL */
	UPROPERTY(config, EditAnywhere, Category = "API", meta = (DisplayName = "Server URL"))
	FString ServerUrl = TEXT("ws://127.0.0.1:9001");

	// === Data & Privacy ===

	/** Share anonymous usage analytics to help improve Alakazam */
	UPROPERTY(config, EditAnywhere, Category = "Data & Privacy")
	bool bShareUsageAnalytics = false;

	/** Allow using your stylized outputs to improve AI models */
	UPROPERTY(config, EditAnywhere, Category = "Data & Privacy")
	bool bShareDataForTraining = false;

	/** Store captures online for access from web dashboard */
	UPROPERTY(config, EditAnywhere, Category = "Data & Privacy")
	bool bStoreCapturesOnline = false;

	/** Whether the initial setup wizard has been completed */
	UPROPERTY(config)
	bool bSetupComplete = false;

	/** Whether the user has accepted the Terms of Service */
	UPROPERTY(config)
	bool bAcceptedTerms = false;

	// === UDeveloperSettings Interface ===

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("Alakazam Portal"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override { return NSLOCTEXT("AlakazamSettings", "SectionText", "Alakazam Portal"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("AlakazamSettings", "SectionDesc", "Configure Alakazam Portal API key and preferences"); }
#endif
};
