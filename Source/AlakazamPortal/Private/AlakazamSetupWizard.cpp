#include "AlakazamSetupWizard.h"

#if WITH_EDITOR

#include "AlakazamSettings.h"
#include "AlakazamAuth.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"

#define LOCTEXT_NAMESPACE "AlakazamSetupWizard"

void SAlakazamSetupWizard::Construct(const FArguments& InArgs)
{
	// Load existing values
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	ApiKeyInput = Settings->ApiKey;
	bConsentTerms = Settings->bAcceptedTerms;
	bConsentAnalytics = Settings->bShareUsageAnalytics;
	bConsentTraining = Settings->bShareDataForTraining;
	bConsentCloud = Settings->bStoreCapturesOnline;

	// Skip to complete if already set up
	if (Settings->bSetupComplete && !Settings->ApiKey.IsEmpty())
	{
		CurrentStep = EWizardStep::Complete;
	}

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(450)
		.MinDesiredHeight(400)
		.Padding(20)
		[
			SNew(SVerticalBox)

			// Content area (changes per step)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						// This rebuilds when step changes
						SNew(SBox)
						[
							BuildWelcomeContent()
						]
					]
				]
			]
		]
	];
}

TSharedPtr<SWindow> SAlakazamSetupWizard::Show()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Alakazam Setup"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(true);

	TSharedRef<SAlakazamSetupWizard> Wizard = SNew(SAlakazamSetupWizard);
	Wizard->OwnerWindow = Window;

	Window->SetContent(Wizard);

	FSlateApplication::Get().AddWindow(Window);

	return Window;
}

bool SAlakazamSetupWizard::ShouldShowWizard()
{
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	return !Settings->bSetupComplete || Settings->ApiKey.IsEmpty();
}

TSharedRef<SWidget> SAlakazamSetupWizard::BuildWelcomeContent()
{
	return SNew(SVerticalBox)

	// Header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 20)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("WelcomeTitle", "Welcome to Alakazam Portal"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
		.Justification(ETextJustify::Center)
	]

	// Description
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 20)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("WelcomeDesc",
			"Alakazam Portal lets you explore visual concepts on your greybox scenes in real-time. "
			"Transform your 3D scenes with AI-powered stylization.\n\n"
			"To get started, you'll need an API key from your Alakazam account."))
		.AutoWrapText(true)
	]

	// Get Started button
	+ SVerticalBox::Slot()
	.AutoHeight()
	.HAlign(HAlign_Center)
	.Padding(0, 20, 0, 20)
	[
		SNew(SButton)
		.Text(LOCTEXT("GetStarted", "Get Started"))
		.OnClicked(this, &SAlakazamSetupWizard::OnNextClicked)
	]

	// Signup link
	+ SVerticalBox::Slot()
	.AutoHeight()
	.HAlign(HAlign_Center)
	[
		SNew(SHyperlink)
		.Text(LOCTEXT("SignupLink", "Don't have an account? Sign up"))
		.OnNavigate_Lambda([this]() { OpenUrl(TEXT("https://alakazam.ai/signup")); })
	];
}

TSharedRef<SWidget> SAlakazamSetupWizard::BuildApiKeyContent()
{
	return SNew(SVerticalBox)

	// Header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 20)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ApiKeyTitle", "Enter Your API Key"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
		.Justification(ETextJustify::Center)
	]

	// Description
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 15)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ApiKeyDesc", "Enter the API key from your Alakazam account. You can find this in your account dashboard."))
		.AutoWrapText(true)
	]

	// API Key label
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 5)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ApiKeyLabel", "API Key:"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	]

	// API Key input
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SAssignNew(ApiKeyTextBox, SEditableTextBox)
		.Text(FText::FromString(ApiKeyInput))
		.OnTextChanged(this, &SAlakazamSetupWizard::OnApiKeyChanged)
	]

	// Validation message
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 15)
	[
		SNew(STextBlock)
		.Text(FText::FromString(ValidationMessage))
		.ColorAndOpacity(bValidationSuccess ? FLinearColor::Green : FLinearColor::Red)
		.Visibility(ValidationMessage.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
	]

	// Account dashboard link
	+ SVerticalBox::Slot()
	.AutoHeight()
	.HAlign(HAlign_Center)
	.Padding(0, 0, 0, 20)
	[
		SNew(SHyperlink)
		.Text(LOCTEXT("DashboardLink", "Open Account Dashboard"))
		.OnNavigate_Lambda([this]() { OpenUrl(TEXT("https://alakazam.ai/account")); })
	]

	// Buttons
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Back", "Back"))
			.OnClicked(this, &SAlakazamSetupWizard::OnBackClicked)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("ValidateContinue", "Validate & Continue"))
			.OnClicked(this, &SAlakazamSetupWizard::OnValidateClicked)
			.IsEnabled_Lambda([this]() { return !ApiKeyInput.IsEmpty(); })
		]
	];
}

TSharedRef<SWidget> SAlakazamSetupWizard::BuildDataConsentContent()
{
	return SNew(SVerticalBox)

	// Header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 20)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ConsentTitle", "Data & Privacy Settings"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
		.Justification(ETextJustify::Center)
	]

	// Description
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 15)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ConsentDesc", "Choose how you'd like to share data with Alakazam. We never store your proprietary game assets without your explicit consent."))
		.AutoWrapText(true)
	]

	// Terms checkbox
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked(bConsentTerms ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bConsentTerms = (State == ECheckBoxState::Checked); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(5, 0, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TermsCheck", "I agree to the Terms of Service and Privacy Policy"))
			.AutoWrapText(true)
		]
	]

	// Terms link
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(25, 0, 0, 15)
	[
		SNew(SHyperlink)
		.Text(LOCTEXT("TermsLink", "View Terms of Service"))
		.OnNavigate_Lambda([this]() { OpenUrl(TEXT("https://alakazam.ai/terms")); })
	]

	// Optional section header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 10, 0, 10)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("OptionalHeader", "Optional Data Sharing:"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	]

	// Analytics checkbox
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SNew(SCheckBox)
			.IsChecked(bConsentAnalytics ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bConsentAnalytics = (State == ECheckBoxState::Checked); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(5, 0, 0, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AnalyticsTitle", "Share usage analytics"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AnalyticsDesc", "Help improve Alakazam by sharing anonymous usage data."))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
	]

	// Training checkbox
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SNew(SCheckBox)
			.IsChecked(bConsentTraining ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bConsentTraining = (State == ECheckBoxState::Checked); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(5, 0, 0, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TrainingTitle", "Allow model training with my stylized outputs"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TrainingDesc", "We may use your stylized outputs (not original assets) to improve AI models."))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
	]

	// Cloud checkbox
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 20)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SNew(SCheckBox)
			.IsChecked(bConsentCloud ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bConsentCloud = (State == ECheckBoxState::Checked); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(5, 0, 0, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CloudTitle", "Store captures online"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CloudDesc", "Save your stylized captures to access from the web dashboard."))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
	]

	// Buttons
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Back", "Back"))
			.OnClicked(this, &SAlakazamSetupWizard::OnBackClicked)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("CompleteSetup", "Complete Setup"))
			.OnClicked(this, &SAlakazamSetupWizard::OnCompleteClicked)
			.IsEnabled_Lambda([this]() { return bConsentTerms; })
		]
	];
}

TSharedRef<SWidget> SAlakazamSetupWizard::BuildCompleteContent()
{
	UAlakazamAuth* Auth = UAlakazamAuth::Get();
	FAlakazamUsageInfo Usage = Auth->CurrentUsage;

	return SNew(SVerticalBox)

	// Header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 20)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("CompleteTitle", "Setup Complete!"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
		.Justification(ETextJustify::Center)
	]

	// Description
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 20)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("CompleteDesc",
			"You're all set! Alakazam Portal is ready to use.\n\n"
			"To get started:\n"
			"1. Add an AlakazamController component to any actor\n"
			"2. Add an AlakazamDemoActor for a quick demo\n"
			"3. Enter Play mode and start stylizing!"))
		.AutoWrapText(true)
	]

	// Buttons
	+ SVerticalBox::Slot()
	.AutoHeight()
	.HAlign(HAlign_Center)
	.Padding(0, 0, 0, 20)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 10, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("OpenDocs", "Open Documentation"))
			.OnClicked_Lambda([this]() { OpenUrl(TEXT("https://alakazam.ai/docs")); return FReply::Handled(); })
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Close", "Close"))
			.OnClicked(this, &SAlakazamSetupWizard::OnCloseClicked)
		]
	]

	// Settings section
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 20, 0, 0)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("QuickSettings", "Quick Settings"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 10, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock).Text(LOCTEXT("ApiKeySettingLabel", "API Key: "))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(STextBlock).Text(FText::FromString(MaskApiKey(UAlakazamSettings::Get()->ApiKey)))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Change", "Change"))
			.OnClicked_Lambda([this]() { GoToStep(EWizardStep::ApiKey); return FReply::Handled(); })
		]
	];
}

void SAlakazamSetupWizard::GoToStep(EWizardStep Step)
{
	CurrentStep = Step;

	TSharedRef<SWidget> NewContent = SNullWidget::NullWidget;

	switch (CurrentStep)
	{
	case EWizardStep::Welcome:
		NewContent = BuildWelcomeContent();
		break;
	case EWizardStep::ApiKey:
		NewContent = BuildApiKeyContent();
		break;
	case EWizardStep::DataConsent:
		NewContent = BuildDataConsentContent();
		break;
	case EWizardStep::Complete:
		NewContent = BuildCompleteContent();
		break;
	}

	// Rebuild the widget
	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(450)
		.MinDesiredHeight(400)
		.Padding(20)
		[
			NewContent
		]
	];
}

FReply SAlakazamSetupWizard::OnNextClicked()
{
	GoToStep(EWizardStep::ApiKey);
	return FReply::Handled();
}

FReply SAlakazamSetupWizard::OnBackClicked()
{
	switch (CurrentStep)
	{
	case EWizardStep::ApiKey:
		GoToStep(EWizardStep::Welcome);
		break;
	case EWizardStep::DataConsent:
		GoToStep(EWizardStep::ApiKey);
		break;
	default:
		break;
	}
	return FReply::Handled();
}

FReply SAlakazamSetupWizard::OnValidateClicked()
{
	// Basic validation
	if (ApiKeyInput.IsEmpty())
	{
		ValidationMessage = TEXT("Please enter an API key");
		bValidationSuccess = false;
		GoToStep(EWizardStep::ApiKey); // Refresh
		return FReply::Handled();
	}

	if (!ApiKeyInput.StartsWith(TEXT("ak_")) && ApiKeyInput.Len() < 10)
	{
		ValidationMessage = TEXT("Invalid API key format. Keys should start with 'ak_'");
		bValidationSuccess = false;
		GoToStep(EWizardStep::ApiKey); // Refresh
		return FReply::Handled();
	}

	// Save the key
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	Settings->ApiKey = ApiKeyInput;
	Settings->TryUpdateDefaultConfigFile();

	// Also update AlakazamAuth
	UAlakazamAuth::Get()->SetApiKey(ApiKeyInput);

	ValidationMessage = TEXT("API key saved!");
	bValidationSuccess = true;

	// Move to consent
	GoToStep(EWizardStep::DataConsent);
	return FReply::Handled();
}

FReply SAlakazamSetupWizard::OnCompleteClicked()
{
	// Save preferences
	UAlakazamSettings* Settings = UAlakazamSettings::Get();
	Settings->bAcceptedTerms = bConsentTerms;
	Settings->bShareUsageAnalytics = bConsentAnalytics;
	Settings->bShareDataForTraining = bConsentTraining;
	Settings->bStoreCapturesOnline = bConsentCloud;
	Settings->bSetupComplete = true;
	Settings->TryUpdateDefaultConfigFile();

	// Also update AlakazamAuth
	UAlakazamAuth* Auth = UAlakazamAuth::Get();
	Auth->bShareUsageAnalytics = bConsentAnalytics;
	Auth->bShareDataForTraining = bConsentTraining;
	Auth->bStoreCapturesOnline = bConsentCloud;
	Auth->SetSetupComplete(true);
	Auth->SaveConsentPreferences();

	GoToStep(EWizardStep::Complete);
	return FReply::Handled();
}

FReply SAlakazamSetupWizard::OnCloseClicked()
{
	if (TSharedPtr<SWindow> Window = OwnerWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

void SAlakazamSetupWizard::OnApiKeyChanged(const FText& NewText)
{
	ApiKeyInput = NewText.ToString();
}

void SAlakazamSetupWizard::OpenUrl(const FString& Url)
{
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

FString SAlakazamSetupWizard::MaskApiKey(const FString& Key) const
{
	if (Key.IsEmpty() || Key.Len() < 8)
	{
		return TEXT("Not set");
	}
	return Key.Left(4) + TEXT("****") + Key.Right(4);
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
