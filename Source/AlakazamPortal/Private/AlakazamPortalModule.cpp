#include "AlakazamPortalModule.h"

#define LOCTEXT_NAMESPACE "FAlakazamPortalModule"

void FAlakazamPortalModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("Alakazam Portal: Module started"));
}

void FAlakazamPortalModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("Alakazam Portal: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAlakazamPortalModule, AlakazamPortal)
