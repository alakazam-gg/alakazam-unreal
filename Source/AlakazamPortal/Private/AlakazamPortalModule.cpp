#include "AlakazamPortalModule.h"
#include "AlakazamSettings.h"

#if WITH_EDITOR
#include "AlakazamSetupWizard.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Containers/Ticker.h"
#endif

#define LOCTEXT_NAMESPACE "FAlakazamPortalModule"

#if WITH_EDITOR
// Static ticker delegate handle for FTUE
static FTSTicker::FDelegateHandle GSetupWizardTickerHandle;
static int32 GSetupWizardTickCount = 0;
#endif

void FAlakazamPortalModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("Alakazam Portal: Module started"));

#if WITH_EDITOR
	// Register menu extension after engine init
	FCoreDelegates::OnPostEngineInit.AddLambda([]()
	{
		// Register "Alakazam" menu in the main menu bar
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
			FToolMenuSection& Section = Menu->AddSection("Alakazam", LOCTEXT("AlakazamMenu", "Alakazam"));

			// Add Alakazam submenu
			Section.AddSubMenu(
				"AlakazamSubMenu",
				LOCTEXT("AlakazamMenuLabel", "Alakazam"),
				LOCTEXT("AlakazamMenuTooltip", "Alakazam Portal tools"),
				FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
				{
					FToolMenuSection& SubSection = SubMenu->AddSection("AlakazamSection", LOCTEXT("AlakazamSection", "Alakazam"));

					// Setup Wizard
					SubSection.AddMenuEntry(
						"SetupWizard",
						LOCTEXT("SetupWizardLabel", "Setup Wizard"),
						LOCTEXT("SetupWizardTooltip", "Open the Alakazam setup wizard"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([]()
						{
							SAlakazamSetupWizard::Show();
						}))
					);

					// Account Settings (opens browser)
					SubSection.AddMenuEntry(
						"AccountSettings",
						LOCTEXT("AccountSettingsLabel", "Account Settings"),
						LOCTEXT("AccountSettingsTooltip", "Open Alakazam account settings in browser"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([]()
						{
							FPlatformProcess::LaunchURL(TEXT("https://alakazam.ai/account"), nullptr, nullptr);
						}))
					);

					// Documentation
					SubSection.AddMenuEntry(
						"Documentation",
						LOCTEXT("DocumentationLabel", "Documentation"),
						LOCTEXT("DocumentationTooltip", "Open Alakazam documentation"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([]()
						{
							FPlatformProcess::LaunchURL(TEXT("https://alakazam.ai/docs"), nullptr, nullptr);
						}))
					);
				})
			);
		}));

		// Show setup wizard on first run using ticker (more reliable than timer)
		if (GIsEditor && !IsRunningCommandlet())
		{
			GSetupWizardTickCount = 0;
			GSetupWizardTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([](float DeltaTime) -> bool
				{
					GSetupWizardTickCount++;
					// Wait ~2 seconds (assuming 60fps, ~120 ticks)
					if (GSetupWizardTickCount < 120)
					{
						return true; // Keep ticking
					}

					// Check and show wizard
					if (SAlakazamSetupWizard::ShouldShowWizard())
					{
						UE_LOG(LogTemp, Log, TEXT("Alakazam Portal: Showing setup wizard (first run)"));
						SAlakazamSetupWizard::Show();
					}

					return false; // Stop ticking
				})
			);
		}
	});
#endif
}

void FAlakazamPortalModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("Alakazam Portal: Module shutdown"));

#if WITH_EDITOR
	// Clean up ticker if still running
	if (GSetupWizardTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GSetupWizardTickerHandle);
		GSetupWizardTickerHandle.Reset();
	}

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAlakazamPortalModule, AlakazamPortal)
