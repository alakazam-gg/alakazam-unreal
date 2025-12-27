#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#if WITH_EDITOR

class SButton;
class SEditableTextBox;
class SCheckBox;

/**
 * Alakazam Setup Wizard
 *
 * First-time user experience wizard for configuring Alakazam Portal.
 */
class ALAKAZAMPORTAL_API SAlakazamSetupWizard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAlakazamSetupWizard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Show the wizard in a new window */
	static TSharedPtr<SWindow> Show();

	/** Check if wizard should be shown (first run) */
	static bool ShouldShowWizard();

private:
	enum class EWizardStep : uint8
	{
		Welcome,
		ApiKey,
		DataConsent,
		Complete
	};

	EWizardStep CurrentStep = EWizardStep::Welcome;

	// UI State
	FString ApiKeyInput;
	FString ValidationMessage;
	bool bValidationSuccess = false;
	bool bConsentTerms = false;
	bool bConsentAnalytics = false;
	bool bConsentTraining = false;
	bool bConsentCloud = false;

	// Cached widgets
	TSharedPtr<SEditableTextBox> ApiKeyTextBox;
	TWeakPtr<SWindow> OwnerWindow;

	// Content builders
	TSharedRef<SWidget> BuildWelcomeContent();
	TSharedRef<SWidget> BuildApiKeyContent();
	TSharedRef<SWidget> BuildDataConsentContent();
	TSharedRef<SWidget> BuildCompleteContent();

	// Navigation
	void GoToStep(EWizardStep Step);
	FReply OnNextClicked();
	FReply OnBackClicked();
	FReply OnValidateClicked();
	FReply OnCompleteClicked();
	FReply OnCloseClicked();

	// Helpers
	void OnApiKeyChanged(const FText& NewText);
	void OpenUrl(const FString& Url);
	FString MaskApiKey(const FString& Key) const;
};

#endif // WITH_EDITOR
