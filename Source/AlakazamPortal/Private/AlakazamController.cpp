#include "AlakazamController.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Base64.h"

UAlakazamController::UAlakazamController()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Pre-allocate buffers
	ReceiveBuffer.SetNumUninitialized(1024 * 1024); // 1MB for received frames
}

void UAlakazamController::BeginPlay()
{
	Super::BeginPlay();

	// Load WebSockets module
	FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets");

	SetupCapture();
}

void UAlakazamController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Disconnect();
	Super::EndPlay(EndPlayReason);
}

void UAlakazamController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Capture and send frames at target FPS
	if (bIsStreaming && State == EAlakazamState::Ready)
	{
		FrameTimer += DeltaTime;
		float FrameInterval = 1.0f / TargetFPS;

		if (FrameTimer >= FrameInterval)
		{
			FrameTimer -= FrameInterval;
			CaptureAndSendFrame();
		}
	}

	// FPS counter
	FPSTimer += DeltaTime;
	if (FPSTimer >= 1.0f)
	{
		CurrentFPS = FPSFrameCount / FPSTimer;
		FPSFrameCount = 0;
		FPSTimer = 0.0f;
	}
}

void UAlakazamController::SetupCapture()
{
	// Create render target for capture
	CaptureRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	CaptureRenderTarget->InitCustomFormat(CaptureWidth, CaptureHeight, PF_B8G8R8A8, false);
	CaptureRenderTarget->UpdateResourceImmediate();

	// Create output texture
	OutputTexture = UTexture2D::CreateTransient(CaptureWidth, CaptureHeight, PF_B8G8R8A8);
	OutputTexture->UpdateResource();

	// Auto-create scene capture for player camera mode
	if (bCaptureFromPlayerCamera)
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			AutoSceneCapture = NewObject<USceneCaptureComponent2D>(Owner, TEXT("AlakazamAutoCapture"));
			AutoSceneCapture->RegisterComponent();
			AutoSceneCapture->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			AutoSceneCapture->TextureTarget = CaptureRenderTarget;
			AutoSceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			AutoSceneCapture->bCaptureEveryFrame = false; // We'll capture manually
			AutoSceneCapture->bCaptureOnMovement = false;
			AutoSceneCapture->bAlwaysPersistRenderingState = true;

			UE_LOG(LogTemp, Log, TEXT("Alakazam: Created auto scene capture for player camera"));
		}
	}
	else if (SceneCaptureComponent)
	{
		// Use manually assigned scene capture
		SceneCaptureComponent->TextureTarget = CaptureRenderTarget;
	}

	UE_LOG(LogTemp, Log, TEXT("Alakazam: Capture setup complete (%dx%d)"), CaptureWidth, CaptureHeight);
}

void UAlakazamController::Connect()
{
	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("Alakazam: Already connected"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Alakazam: Connecting to %s"), *ServerUrl);
	State = EAlakazamState::Connecting;

	// Create WebSocket
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(ServerUrl, TEXT(""));

	// Bind events
	WebSocket->OnConnected().AddLambda([this]()
	{
		UE_LOG(LogTemp, Log, TEXT("Alakazam: WebSocket connected, sending auth..."));
		State = EAlakazamState::Authenticating;

		// Send auth message
		TSharedPtr<FJsonObject> AuthMsg = MakeShareable(new FJsonObject);
		AuthMsg->SetStringField(TEXT("type"), TEXT("auth"));
		AuthMsg->SetStringField(TEXT("prompt"), Prompt);
		AuthMsg->SetBoolField(TEXT("enhance"), bEnhancePrompt);

		FString AuthStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&AuthStr);
		FJsonSerializer::Serialize(AuthMsg.ToSharedRef(), Writer);

		WebSocket->Send(AuthStr);
	});

	WebSocket->OnConnectionError().AddLambda([this](const FString& Error)
	{
		UE_LOG(LogTemp, Error, TEXT("Alakazam: Connection error: %s"), *Error);
		State = EAlakazamState::Error;
		OnError.Broadcast(Error);
	});

	WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean)
	{
		UE_LOG(LogTemp, Log, TEXT("Alakazam: Connection closed: %s"), *Reason);
		State = EAlakazamState::Disconnected;
	});

	WebSocket->OnMessage().AddLambda([this](const FString& Message)
	{
		// Parse JSON message
		TSharedPtr<FJsonObject> JsonMsg;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

		if (FJsonSerializer::Deserialize(Reader, JsonMsg) && JsonMsg.IsValid())
		{
			FString Type = JsonMsg->GetStringField(TEXT("type"));

			if (Type == TEXT("ready"))
			{
				SessionId = JsonMsg->GetStringField(TEXT("session_id"));
				UE_LOG(LogTemp, Log, TEXT("Alakazam: Ready! Session: %s"), *SessionId);
				State = EAlakazamState::Ready;
				OnConnected.Broadcast();
			}
			else if (Type == TEXT("error"))
			{
				FString ErrorMsg = JsonMsg->GetStringField(TEXT("message"));
				UE_LOG(LogTemp, Error, TEXT("Alakazam: Server error: %s"), *ErrorMsg);
				State = EAlakazamState::Error;
				OnError.Broadcast(ErrorMsg);
			}
		}
	});

	WebSocket->OnRawMessage().AddLambda([this](const void* Data, SIZE_T Size, SIZE_T BytesRemaining)
	{
		// Accumulate fragmented binary data
		if (Size > 0)
		{
			const uint8* ByteData = static_cast<const uint8*>(Data);
			ReceiveBuffer.Append(ByteData, Size);
		}

		// Process only when full message is received
		if (BytesRemaining == 0 && ReceiveBuffer.Num() > 0)
		{
			// Skip JSON text messages (start with '{')
			if (ReceiveBuffer[0] != 0x7B)
			{
				ProcessReceivedFrame(ReceiveBuffer.GetData(), ReceiveBuffer.Num());
			}
			ReceiveBuffer.Reset();
		}
	});

	WebSocket->Connect();
}

void UAlakazamController::Disconnect()
{
	if (WebSocket.IsValid())
	{
		WebSocket->Close();
		WebSocket.Reset();
	}

	State = EAlakazamState::Disconnected;
	bIsStreaming = false;
	FramesSent = 0;
	FramesReceived = 0;

	UE_LOG(LogTemp, Log, TEXT("Alakazam: Disconnected"));
}

void UAlakazamController::SetPrompt(const FString& NewPrompt)
{
	Prompt = NewPrompt;

	if (WebSocket.IsValid() && WebSocket->IsConnected() && State == EAlakazamState::Ready)
	{
		TSharedPtr<FJsonObject> PromptMsg = MakeShareable(new FJsonObject);
		PromptMsg->SetStringField(TEXT("type"), TEXT("prompt"));
		PromptMsg->SetStringField(TEXT("prompt"), Prompt);
		PromptMsg->SetBoolField(TEXT("enhance"), bEnhancePrompt);

		FString PromptStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PromptStr);
		FJsonSerializer::Serialize(PromptMsg.ToSharedRef(), Writer);

		WebSocket->Send(PromptStr);
		UE_LOG(LogTemp, Log, TEXT("Alakazam: Prompt updated: %s"), *Prompt);
	}
}

void UAlakazamController::StartStreaming()
{
	if (State == EAlakazamState::Ready)
	{
		bIsStreaming = true;
		UE_LOG(LogTemp, Log, TEXT("Alakazam: Streaming started"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Alakazam: Cannot start streaming - not ready"));
	}
}

void UAlakazamController::StopStreaming()
{
	bIsStreaming = false;
	UE_LOG(LogTemp, Log, TEXT("Alakazam: Streaming stopped"));
}

bool UAlakazamController::IsConnected() const
{
	return WebSocket.IsValid() && WebSocket->IsConnected();
}

bool UAlakazamController::IsReady() const
{
	return State == EAlakazamState::Ready;
}

void UAlakazamController::SyncCaptureWithPlayerCamera()
{
	if (!bCaptureFromPlayerCamera || !AutoSceneCapture) return;

	// Get player camera manager
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	if (!CameraManager) return;

	// Sync position and rotation with player camera
	FVector CameraLocation = CameraManager->GetCameraLocation();
	FRotator CameraRotation = CameraManager->GetCameraRotation();

	AutoSceneCapture->SetWorldLocationAndRotation(CameraLocation, CameraRotation);

	// Sync FOV
	AutoSceneCapture->FOVAngle = CameraManager->GetFOVAngle();
}

void UAlakazamController::CaptureAndSendFrame()
{
	if (!WebSocket.IsValid() || !WebSocket->IsConnected() || !CaptureRenderTarget) return;

	// Sync capture component with player camera before capturing
	if (bCaptureFromPlayerCamera)
	{
		SyncCaptureWithPlayerCamera();

		// Trigger manual capture
		if (AutoSceneCapture)
		{
			AutoSceneCapture->CaptureScene();
		}
	}

	// Read pixels from render target
	FTextureRenderTargetResource* RenderTargetResource = CaptureRenderTarget->GameThread_GetRenderTargetResource();
	if (!RenderTargetResource) return;

	TArray<FColor> Pixels;
	RenderTargetResource->ReadPixels(Pixels);

	if (Pixels.Num() == 0) return;

	// Encode as JPEG
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), CaptureWidth, CaptureHeight, ERGBFormat::BGRA, 8))
	{
		TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(JpegQuality);
		if (CompressedData.Num() > 0)
		{
			// Send as binary WebSocket message
			WebSocket->Send(CompressedData.GetData(), CompressedData.Num(), true);
			FramesSent++;

			if (FramesSent <= 5 || FramesSent % 100 == 0)
			{
				UE_LOG(LogTemp, Log, TEXT("Alakazam: Sent frame %d (%lld bytes)"), FramesSent, CompressedData.Num());
			}
		}
	}
}

void UAlakazamController::ProcessReceivedFrame(const void* Data, SIZE_T Size)
{
	if (Size < 8) return;

	const uint8* Bytes = static_cast<const uint8*>(Data);

	// Detect image format from magic bytes
	EImageFormat Format = EImageFormat::Invalid;
	if (Bytes[0] == 0xFF && Bytes[1] == 0xD8)
	{
		Format = EImageFormat::JPEG;
	}
	else if (Bytes[0] == 0x89 && Bytes[1] == 0x50 && Bytes[2] == 0x4E && Bytes[3] == 0x47)
	{
		Format = EImageFormat::PNG;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Alakazam: Unknown image format (magic: 0x%02X 0x%02X 0x%02X 0x%02X)"),
			Bytes[0], Bytes[1], Bytes[2], Bytes[3]);
		return;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

	if (ImageWrapper->SetCompressed(Data, Size))
	{
		TArray<uint8> RawData;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
		{
			// Update output texture
			if (OutputTexture)
			{
				FTexture2DMipMap& Mip = OutputTexture->GetPlatformData()->Mips[0];
				void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
				Mip.BulkData.Unlock();
				OutputTexture->UpdateResource();

				OnFrameReceived.Broadcast(OutputTexture);
			}

			FramesReceived++;
			FPSFrameCount++;

			if (FramesReceived <= 5 || FramesReceived % 100 == 0)
			{
				UE_LOG(LogTemp, Log, TEXT("Alakazam: Received frame %d (%d bytes, %s)"),
					FramesReceived, (int32)Size, Format == EImageFormat::JPEG ? TEXT("JPEG") : TEXT("PNG"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Alakazam: Failed to decompress %s frame (%d bytes)"),
			Format == EImageFormat::JPEG ? TEXT("JPEG") : TEXT("PNG"), (int32)Size);
	}
}
