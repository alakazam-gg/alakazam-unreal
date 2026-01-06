#include "AlakazamController.h"
#include "AlakazamAuth.h"
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
#include "RenderGraphUtils.h"
#include "RHICommandList.h"

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

	// DON'T setup capture here - wait until streaming actually starts
	// This allows extraction-only connections without capture overhead
}

void UAlakazamController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Disconnect();
	Super::EndPlay(EndPlayReason);
}

void UAlakazamController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Process any pending async readback
	ProcessAsyncReadback();

	// Capture and send frames at target FPS
	if (bIsStreaming && State == EAlakazamState::Ready && !bReadbackPending)
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
	if (bCaptureSetupDone) return; // Already set up

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

	bCaptureSetupDone = true;
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

		// Check for API key
		UAlakazamAuth* Auth = UAlakazamAuth::Get();
		FString ApiKey = Auth->GetApiKey();
		if (ApiKey.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Alakazam: No API key configured"));
			State = EAlakazamState::Error;
			Auth->HandleAuthFailed(TEXT("No API key configured. Configure in Project Settings > Plugins > Alakazam Portal."));
			OnError.Broadcast(TEXT("No API key configured"));
			WebSocket->Close();
			return;
		}

		// Send auth message with API key
		TSharedPtr<FJsonObject> AuthMsg = MakeShareable(new FJsonObject);
		AuthMsg->SetStringField(TEXT("type"), TEXT("auth"));
		AuthMsg->SetStringField(TEXT("prompt"), Prompt);
		AuthMsg->SetStringField(TEXT("api_key"), ApiKey);
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

				// Process usage info
				const TSharedPtr<FJsonObject>* UsageObj;
				if (JsonMsg->TryGetObjectField(TEXT("usage"), UsageObj))
				{
					int32 SecondsUsed = (*UsageObj)->GetIntegerField(TEXT("seconds_used"));
					int32 SecondsLimit = (*UsageObj)->GetIntegerField(TEXT("seconds_limit"));
					int32 SecondsRemaining = (*UsageObj)->GetIntegerField(TEXT("seconds_remaining"));
					UAlakazamAuth::Get()->UpdateUsage(SecondsUsed, SecondsLimit, SecondsRemaining);
				}

				// Handle server warning (80%+ usage)
				FString Warning;
				if (JsonMsg->TryGetStringField(TEXT("warning"), Warning))
				{
					UAlakazamAuth::Get()->HandleWarning(Warning);
				}

				State = EAlakazamState::Ready;

				// If we have a pending image for extraction, send it now
				if (bExtractionOnlyMode && PendingStyleImage)
				{
					UE_LOG(LogTemp, Log, TEXT("Alakazam: Extraction-only mode - sending pending image"));
					SendPendingImageForExtraction();
				}

				OnConnected.Broadcast();
			}
			else if (Type == TEXT("error"))
			{
				FString ErrorMsg = JsonMsg->GetStringField(TEXT("message"));
				UE_LOG(LogTemp, Error, TEXT("Alakazam: Server error: %s"), *ErrorMsg);
				UAlakazamAuth::Get()->HandleAuthFailed(ErrorMsg);
				State = EAlakazamState::Error;
				OnError.Broadcast(ErrorMsg);
			}
			else if (Type == TEXT("style_extracted"))
			{
				FString ExtractedPrompt = JsonMsg->GetStringField(TEXT("prompt"));
				UE_LOG(LogTemp, Log, TEXT("Alakazam: Style extracted: %s"), *ExtractedPrompt);
				Prompt = ExtractedPrompt;
				bIsExtractingStyle = false;
				bIsUsingImageStyle = true;
				PendingStyleImage = nullptr;
				OnStyleExtracted.Broadcast(ExtractedPrompt);
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
	// Stop streaming first to prevent new captures
	bIsStreaming = false;
	bIsExtractingStyle = false;
	bExtractionOnlyMode = false;
	State = EAlakazamState::Disconnected;

	if (WebSocket.IsValid())
	{
		WebSocket->Close();
		WebSocket.Reset();
	}

	// Flush render commands and wait for GPU to finish before cleanup
	if (GPUReadback)
	{
		// Ensure no pending render commands are using the readback
		FlushRenderingCommands();

		delete GPUReadback;
		GPUReadback = nullptr;
	}
	bReadbackPending = false;
	bReadbackDataReady = false;
	PendingStyleImage = nullptr;

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

void UAlakazamController::SetStyleFromImage(UTexture2D* ReferenceImage)
{
	if (!ReferenceImage)
	{
		UE_LOG(LogTemp, Error, TEXT("Alakazam: Reference image is null"));
		return;
	}

	if (!WebSocket.IsValid() || !WebSocket->IsConnected() || State != EAlakazamState::Ready)
	{
		UE_LOG(LogTemp, Warning, TEXT("Alakazam: Not ready to set style from image"));
		return;
	}

	// Set extracting flag
	bIsExtractingStyle = true;

	// Read texture data
	FTexture2DMipMap& Mip = ReferenceImage->GetPlatformData()->Mips[0];
	int32 Width = Mip.SizeX;
	int32 Height = Mip.SizeY;

	const void* TextureData = Mip.BulkData.LockReadOnly();
	if (!TextureData)
	{
		UE_LOG(LogTemp, Error, TEXT("Alakazam: Failed to lock texture data"));
		bIsExtractingStyle = false;
		return;
	}

	// Encode to JPEG
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (ImageWrapper->SetRaw(TextureData, Width * Height * 4, Width, Height, ERGBFormat::BGRA, 8))
	{
		TArray64<uint8> JpegData = ImageWrapper->GetCompressed(90);
		Mip.BulkData.Unlock();

		if (JpegData.Num() > 0)
		{
			// Convert to base64
			FString Base64Data = FBase64::Encode(JpegData.GetData(), JpegData.Num());
			SetStyleFromBase64(Base64Data);
			UE_LOG(LogTemp, Log, TEXT("Alakazam: Sending reference image for style extraction (%lld bytes)"), JpegData.Num());
		}
		else
		{
			bIsExtractingStyle = false;
		}
	}
	else
	{
		Mip.BulkData.Unlock();
		bIsExtractingStyle = false;
		UE_LOG(LogTemp, Error, TEXT("Alakazam: Failed to encode reference image"));
	}
}

void UAlakazamController::SetStyleFromBase64(const FString& Base64ImageData)
{
	if (Base64ImageData.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Alakazam: Base64 image data is empty"));
		return;
	}

	if (!WebSocket.IsValid() || !WebSocket->IsConnected() || State != EAlakazamState::Ready)
	{
		UE_LOG(LogTemp, Warning, TEXT("Alakazam: Not ready to set style from image"));
		return;
	}

	TSharedPtr<FJsonObject> ImagePromptMsg = MakeShareable(new FJsonObject);
	ImagePromptMsg->SetStringField(TEXT("type"), TEXT("image_prompt"));
	ImagePromptMsg->SetStringField(TEXT("image_data"), Base64ImageData);
	ImagePromptMsg->SetBoolField(TEXT("enhance"), bEnhancePrompt);

	FString MsgStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MsgStr);
	FJsonSerializer::Serialize(ImagePromptMsg.ToSharedRef(), Writer);

	WebSocket->Send(MsgStr);
	UE_LOG(LogTemp, Log, TEXT("Alakazam: Sent image_prompt message for style extraction"));
}

void UAlakazamController::StartStreaming()
{
	if (State == EAlakazamState::Ready)
	{
		// Setup capture if not done yet (e.g., if we connected for extraction only)
		if (!bCaptureSetupDone)
		{
			SetupCapture();
		}

		bExtractionOnlyMode = false; // Clear extraction-only mode
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
	// Early exit if not streaming (prevents captures during shutdown)
	if (!bIsStreaming || State != EAlakazamState::Ready) return;
	if (!WebSocket.IsValid() || !WebSocket->IsConnected() || !CaptureRenderTarget) return;
	if (bReadbackPending) return; // Still waiting for previous readback

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

	// Get render target resource
	FTextureRenderTargetResource* RenderTargetResource = CaptureRenderTarget->GameThread_GetRenderTargetResource();
	if (!RenderTargetResource) return;

	// Create GPU readback if needed
	if (!GPUReadback)
	{
		GPUReadback = new FRHIGPUTextureReadback(TEXT("AlakazamReadback"));
	}

	// Enqueue async readback on render thread
	FRHITexture* TextureRHI = RenderTargetResource->GetRenderTargetTexture();
	if (!TextureRHI) return;

	// Capture local copies for lambda (avoid accessing 'this' members after potential destruction)
	FRHIGPUTextureReadback* LocalReadback = GPUReadback;
	int32 LocalWidth = CaptureWidth;
	int32 LocalHeight = CaptureHeight;

	ENQUEUE_RENDER_COMMAND(AlakazamAsyncReadback)(
		[LocalReadback, TextureRHI, LocalWidth, LocalHeight](FRHICommandListImmediate& RHICmdList)
		{
			if (LocalReadback)
			{
				LocalReadback->EnqueueCopy(RHICmdList, TextureRHI, FIntVector(0, 0, 0), 0, FIntVector(LocalWidth, LocalHeight, 1));
			}
		});

	bReadbackPending = true;
}

void UAlakazamController::ProcessAsyncReadback()
{
	// Early exit if not streaming (prevents processing during shutdown)
	if (!bIsStreaming) return;

	// Check if we have data ready to send (copied from render thread)
	if (bReadbackDataReady)
	{
		FScopeLock Lock(&ReadbackLock);

		if (ReadbackPixels.Num() > 0 && WebSocket.IsValid() && WebSocket->IsConnected())
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

			if (ImageWrapper->SetRaw(ReadbackPixels.GetData(), ReadbackPixels.Num() * sizeof(FColor), CaptureWidth, CaptureHeight, ERGBFormat::BGRA, 8))
			{
				TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(JpegQuality);
				if (CompressedData.Num() > 0)
				{
					WebSocket->Send(CompressedData.GetData(), CompressedData.Num(), true);
					FramesSent++;

					if (FramesSent <= 5 || FramesSent % 100 == 0)
					{
						UE_LOG(LogTemp, Log, TEXT("Alakazam: Sent frame %d (%lld bytes)"), FramesSent, CompressedData.Num());
					}
				}
			}
		}

		bReadbackDataReady = false;
		bReadbackPending = false;
		return;
	}

	if (!bReadbackPending || !GPUReadback) return;

	// Check if readback is ready (this is safe to call from game thread)
	if (!GPUReadback->IsReady()) return;

	// Copy data on render thread, then signal game thread
	int32 Width = CaptureWidth;
	int32 Height = CaptureHeight;
	FRHIGPUTextureReadback* Readback = GPUReadback;
	TArray<FColor>* PixelsPtr = &ReadbackPixels;
	bool* DataReadyPtr = &bReadbackDataReady;
	FCriticalSection* LockPtr = &ReadbackLock;

	ENQUEUE_RENDER_COMMAND(AlakazamReadbackCopy)(
		[Readback, Width, Height, PixelsPtr, DataReadyPtr, LockPtr](FRHICommandListImmediate& RHICmdList)
		{
			int32 RowPitchInPixels = 0;
			const FColor* PixelData = static_cast<const FColor*>(Readback->Lock(RowPitchInPixels));

			if (PixelData && RowPitchInPixels > 0)
			{
				FScopeLock Lock(LockPtr);
				PixelsPtr->SetNum(Width * Height);
				for (int32 Row = 0; Row < Height; Row++)
				{
					FMemory::Memcpy(
						PixelsPtr->GetData() + Row * Width,
						PixelData + Row * RowPitchInPixels,
						Width * sizeof(FColor)
					);
				}
				*DataReadyPtr = true;
			}

			Readback->Unlock();
		});
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

void UAlakazamController::ExtractStyleFromImage(UTexture2D* ReferenceImage)
{
	if (!ReferenceImage)
	{
		UE_LOG(LogTemp, Error, TEXT("Alakazam: Reference image is null"));
		return;
	}

	PendingStyleImage = ReferenceImage;
	bIsExtractingStyle = true;
	bExtractionOnlyMode = true;

	if (!IsConnected())
	{
		// Auto-connect for extraction only (no streaming)
		UE_LOG(LogTemp, Log, TEXT("Alakazam: Connecting for style extraction..."));
		ConnectForExtractionOnly();
	}
	else if (State == EAlakazamState::Ready)
	{
		// Already connected and ready - send extraction request immediately
		SendPendingImageForExtraction();
	}
	// else: connected but not ready yet - will be handled when "ready" is received
}

void UAlakazamController::ClearImageStyle()
{
	bIsUsingImageStyle = false;
	bIsExtractingStyle = false;
	PendingStyleImage = nullptr;
}

void UAlakazamController::ConnectForExtractionOnly()
{
	bExtractionOnlyMode = true;
	// DON'T setup capture for extraction-only - we don't need it

	// Use the regular Connect() which will handle extraction mode via bExtractionOnlyMode flag
	Connect();
}

void UAlakazamController::SendPendingImageForExtraction()
{
	if (!PendingStyleImage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Alakazam: No pending image for extraction"));
		bIsExtractingStyle = false;
		return;
	}

	// Read texture data
	FTexture2DMipMap& Mip = PendingStyleImage->GetPlatformData()->Mips[0];
	int32 Width = Mip.SizeX;
	int32 Height = Mip.SizeY;

	const void* TextureData = Mip.BulkData.LockReadOnly();
	if (!TextureData)
	{
		UE_LOG(LogTemp, Error, TEXT("Alakazam: Failed to lock texture data for extraction"));
		bIsExtractingStyle = false;
		PendingStyleImage = nullptr;
		return;
	}

	// Encode to JPEG
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (ImageWrapper->SetRaw(TextureData, Width * Height * 4, Width, Height, ERGBFormat::BGRA, 8))
	{
		TArray64<uint8> JpegData = ImageWrapper->GetCompressed(90);
		Mip.BulkData.Unlock();

		if (JpegData.Num() > 0)
		{
			// Convert to base64 and send
			FString Base64Data = FBase64::Encode(JpegData.GetData(), JpegData.Num());

			TSharedPtr<FJsonObject> ImagePromptMsg = MakeShareable(new FJsonObject);
			ImagePromptMsg->SetStringField(TEXT("type"), TEXT("image_prompt"));
			ImagePromptMsg->SetStringField(TEXT("image_data"), Base64Data);
			ImagePromptMsg->SetBoolField(TEXT("enhance"), bEnhancePrompt);

			FString MsgStr;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MsgStr);
			FJsonSerializer::Serialize(ImagePromptMsg.ToSharedRef(), Writer);

			WebSocket->Send(MsgStr);
			UE_LOG(LogTemp, Log, TEXT("Alakazam: Sent image for style extraction (%lld bytes)"), JpegData.Num());
		}
		else
		{
			bIsExtractingStyle = false;
			PendingStyleImage = nullptr;
		}
	}
	else
	{
		Mip.BulkData.Unlock();
		bIsExtractingStyle = false;
		PendingStyleImage = nullptr;
		UE_LOG(LogTemp, Error, TEXT("Alakazam: Failed to encode image for extraction"));
	}
}
