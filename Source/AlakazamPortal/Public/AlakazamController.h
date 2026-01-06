#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IWebSocket.h"
#include "RHIGPUReadback.h"
#include "AlakazamController.generated.h"

UENUM(BlueprintType)
enum class EAlakazamState : uint8
{
	Disconnected,
	Connecting,
	Authenticating,
	Ready,
	Error
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAlakazamConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAlakazamFrameReceived, UTexture2D*, StylizedFrame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAlakazamError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAlakazamStyleExtracted, const FString&, ExtractedPrompt);

/**
 * Alakazam Portal Controller
 *
 * Connects to Alakazam server and handles real-time stylization.
 * Attach to any actor, configure the server URL and prompt, then call Connect().
 */
UCLASS(ClassGroup=(Alakazam), meta=(BlueprintSpawnableComponent))
class ALAKAZAMPORTAL_API UAlakazamController : public UActorComponent
{
	GENERATED_BODY()

public:
	UAlakazamController();

	// === Configuration ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Server")
	FString ServerUrl = TEXT("ws://127.0.0.1:9001");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Style")
	FString Prompt = TEXT("anime style, vibrant colors");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Style")
	bool bEnhancePrompt = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Capture")
	int32 CaptureWidth = 1280;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Capture")
	int32 CaptureHeight = 720;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Capture")
	int32 JpegQuality = 85;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Capture")
	float TargetFPS = 30.0f;

	/** If true, automatically capture from the player's camera view (like Unity). If false, use manually assigned SceneCaptureComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Capture")
	bool bCaptureFromPlayerCamera = true;

	/** Optional: Manually assign a SceneCaptureComponent2D. Only used if bCaptureFromPlayerCamera is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam|Capture")
	class USceneCaptureComponent2D* SceneCaptureComponent;

	// === Output ===

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|Output")
	UTexture2D* OutputTexture;

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|Output")
	UTextureRenderTarget2D* CaptureRenderTarget;

	// === State ===

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|State")
	EAlakazamState State = EAlakazamState::Disconnected;

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|State")
	bool bIsStreaming = false;

	/** True when extracting style from an image (waiting for server response) */
	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|State")
	bool bIsExtractingStyle = false;

	/** True when using a style extracted from an image (not manual text prompt) */
	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|State")
	bool bIsUsingImageStyle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|Stats")
	int32 FramesSent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|Stats")
	int32 FramesReceived = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam|Stats")
	float CurrentFPS = 0.0f;

	// === Events ===

	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnAlakazamConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnAlakazamFrameReceived OnFrameReceived;

	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnAlakazamError OnError;

	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnAlakazamStyleExtracted OnStyleExtracted;

	// === Functions ===

	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void Connect();

	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void SetPrompt(const FString& NewPrompt);

	/** Set style from a reference image texture. Server will analyze and extract style. Requires being connected first. */
	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void SetStyleFromImage(UTexture2D* ReferenceImage);

	/** Set style from base64-encoded image data. Requires being connected first. */
	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void SetStyleFromBase64(const FString& Base64ImageData);

	/**
	 * Extract style from image immediately. Will auto-connect if needed.
	 * Does NOT start streaming - only extracts the style.
	 * Use this when you want to pre-extract a style before streaming starts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void ExtractStyleFromImage(UTexture2D* ReferenceImage);

	/** Clear image style mode and return to text prompt mode. */
	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void ClearImageStyle();

	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void StartStreaming();

	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void StopStreaming();

	UFUNCTION(BlueprintPure, Category = "Alakazam")
	bool IsConnected() const;

	UFUNCTION(BlueprintPure, Category = "Alakazam")
	bool IsReady() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TSharedPtr<IWebSocket> WebSocket;
	FString SessionId;

	float FrameTimer = 0.0f;
	float FPSTimer = 0.0f;
	int32 FPSFrameCount = 0;

	TArray<uint8> ReceiveBuffer;

	// Extraction-only mode state
	bool bExtractionOnlyMode = false;
	bool bCaptureSetupDone = false;

	UPROPERTY()
	UTexture2D* PendingStyleImage = nullptr;

	// Auto-created scene capture for player camera mode
	UPROPERTY()
	class USceneCaptureComponent2D* AutoSceneCapture;

	// Async GPU readback to avoid blocking game thread
	FRHIGPUTextureReadback* GPUReadback = nullptr;
	bool bReadbackPending = false;
	bool bReadbackDataReady = false;
	TArray<FColor> ReadbackPixels;
	FCriticalSection ReadbackLock;

	void SetupCapture();
	void CaptureAndSendFrame();
	void ProcessAsyncReadback();
	void ProcessReceivedFrame(const void* Data, SIZE_T Size);
	void SyncCaptureWithPlayerCamera();
	void ConnectForExtractionOnly();
	void SendPendingImageForExtraction();
};
