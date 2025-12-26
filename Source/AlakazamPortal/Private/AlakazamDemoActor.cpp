#include "AlakazamDemoActor.h"
#include "AlakazamController.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SViewport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"

AAlakazamDemoActor::AAlakazamDemoActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Create Alakazam Controller
	AlakazamController = CreateDefaultSubobject<UAlakazamController>(TEXT("AlakazamController"));

	// Create scene capture for original view (separate from controller's capture)
	OriginalSceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("OriginalSceneCapture"));
	OriginalSceneCapture->SetupAttachment(RootComponent);
	OriginalSceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	OriginalSceneCapture->bCaptureEveryFrame = false;
	OriginalSceneCapture->bCaptureOnMovement = false;
	OriginalSceneCapture->bAlwaysPersistRenderingState = true;
}

void AAlakazamDemoActor::BeginPlay()
{
	Super::BeginPlay();

	// Create render target for original view
	OriginalRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	OriginalRenderTarget->InitCustomFormat(CaptureWidth, CaptureHeight, PF_B8G8R8A8, false);
	OriginalRenderTarget->UpdateResourceImmediate();
	OriginalSceneCapture->TextureTarget = OriginalRenderTarget;

	// Create the side-by-side display widget
	CreateSideBySideWidget();

	// Bind events
	AlakazamController->OnConnected.AddDynamic(this, &AAlakazamDemoActor::OnConnected);
	AlakazamController->OnFrameReceived.AddDynamic(this, &AAlakazamDemoActor::OnFrameReceived);
	AlakazamController->OnError.AddDynamic(this, &AAlakazamDemoActor::OnError);

	// Auto-connect if enabled
	if (bAutoConnect)
	{
		UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Auto-connecting..."));
		AlakazamController->Connect();
	}
}

void AAlakazamDemoActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Remove the widget from viewport
	if (DisplayWidget.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(DisplayWidget.ToSharedRef());
		}
		DisplayWidget.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AAlakazamDemoActor::CreateSideBySideWidget()
{
	// Initialize brushes
	OriginalBrush.SetResourceObject(OriginalRenderTarget);
	OriginalBrush.ImageSize = FVector2D(CaptureWidth, CaptureHeight);

	// Create the side-by-side layout using Slate
	DisplayWidget = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			// Left side - Original view
			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(OriginalImageWidget, SImage)
					.Image(&OriginalBrush)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				.Padding(0, 20, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("ORIGINAL")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.ShadowOffset(FVector2D(2, 2))
					.ShadowColorAndOpacity(FLinearColor::Black)
				]
			]
			// Right side - Stylized view
			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(StylizedImageWidget, SImage)
					.Image(&StylizedBrush)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				.Padding(0, 20, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("AI STYLIZED")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.ShadowOffset(FVector2D(2, 2))
					.ShadowColorAndOpacity(FLinearColor::Black)
				]
			]
		];

	// Add to viewport
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(DisplayWidget.ToSharedRef(), 100);
	}

	UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Created side-by-side display widget"));
}

void AAlakazamDemoActor::SyncCaptureWithPlayerCamera(USceneCaptureComponent2D* Capture)
{
	if (!Capture) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	if (!CameraManager) return;

	FVector CameraLocation = CameraManager->GetCameraLocation();
	FRotator CameraRotation = CameraManager->GetCameraRotation();

	Capture->SetWorldLocationAndRotation(CameraLocation, CameraRotation);
	Capture->FOVAngle = CameraManager->GetFOVAngle();
}

void AAlakazamDemoActor::UpdateOriginalCapture()
{
	// Sync with player camera and capture
	SyncCaptureWithPlayerCamera(OriginalSceneCapture);
	OriginalSceneCapture->CaptureScene();
}

void AAlakazamDemoActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update original view capture each frame
	UpdateOriginalCapture();

	// Update the brush to reflect the render target
	if (OriginalRenderTarget)
	{
		OriginalBrush.SetResourceObject(OriginalRenderTarget);
	}
}

void AAlakazamDemoActor::OnConnected()
{
	UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Connected! Starting stream..."));
	AlakazamController->StartStreaming();
}

void AAlakazamDemoActor::OnFrameReceived(UTexture2D* StylizedFrame)
{
	if (StylizedFrame)
	{
		// Update the stylized brush with the new frame
		StylizedBrush.SetResourceObject(StylizedFrame);
		StylizedBrush.ImageSize = FVector2D(StylizedFrame->GetSizeX(), StylizedFrame->GetSizeY());

		if (StylizedImageWidget.IsValid())
		{
			StylizedImageWidget->SetImage(&StylizedBrush);
		}

		static int32 LogCount = 0;
		if (LogCount < 3)
		{
			UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Received stylized frame (%dx%d)"),
				StylizedFrame->GetSizeX(), StylizedFrame->GetSizeY());
			LogCount++;
		}
	}
}

void AAlakazamDemoActor::OnError(const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("Alakazam Demo: %s"), *ErrorMessage);
}

void AAlakazamDemoActor::ToggleSideBySide()
{
	bSideBySide = !bSideBySide;

	// TODO: Implement toggle between side-by-side and fullscreen stylized
	UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Side-by-side mode: %s"), bSideBySide ? TEXT("ON") : TEXT("OFF"));
}
