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

	// Create Alakazam Controller - uses auto-capture from player camera
	AlakazamController = CreateDefaultSubobject<UAlakazamController>(TEXT("AlakazamController"));
}

void AAlakazamDemoActor::BeginPlay()
{
	Super::BeginPlay();

	// Initialize brushes
	OriginalBrush = MakeShared<FSlateBrush>();
	StylizedBrush = MakeShared<FSlateBrush>();

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

	OriginalImageWidget.Reset();
	StylizedImageWidget.Reset();
	OriginalBrush.Reset();
	StylizedBrush.Reset();

	Super::EndPlay(EndPlayReason);
}

void AAlakazamDemoActor::CreateSideBySideWidget()
{
	// Set up original brush to use controller's capture render target
	if (AlakazamController && AlakazamController->CaptureRenderTarget)
	{
		OriginalBrush->SetResourceObject(AlakazamController->CaptureRenderTarget);
		OriginalBrush->ImageSize = FVector2D(CaptureWidth, CaptureHeight);
	}

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
					.Image(OriginalBrush.Get())
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
					.Image(StylizedBrush.Get())
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

void AAlakazamDemoActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update original brush to show controller's capture render target
	// The controller captures the scene, we just display it
	if (AlakazamController && AlakazamController->CaptureRenderTarget && OriginalBrush.IsValid())
	{
		OriginalBrush->SetResourceObject(AlakazamController->CaptureRenderTarget);

		// Force Slate to redraw by invalidating the image
		if (OriginalImageWidget.IsValid())
		{
			OriginalImageWidget->SetImage(OriginalBrush.Get());
		}
	}
}

void AAlakazamDemoActor::OnConnected()
{
	UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Connected! Starting stream..."));
	AlakazamController->StartStreaming();
}

void AAlakazamDemoActor::OnFrameReceived(UTexture2D* StylizedFrame)
{
	if (StylizedFrame && StylizedBrush.IsValid())
	{
		// Update the stylized brush with the new frame
		StylizedBrush->SetResourceObject(StylizedFrame);
		StylizedBrush->ImageSize = FVector2D(StylizedFrame->GetSizeX(), StylizedFrame->GetSizeY());

		// Force Slate to redraw
		if (StylizedImageWidget.IsValid())
		{
			StylizedImageWidget->SetImage(StylizedBrush.Get());
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
	UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Side-by-side mode: %s"), bSideBySide ? TEXT("ON") : TEXT("OFF"));
}
