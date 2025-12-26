#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlakazamDemoActor.generated.h"

class UAlakazamController;

/**
 * Drop-in demo actor for Alakazam Portal.
 * Displays side-by-side comparison: Original view (left) vs AI-stylized (right).
 * Place in level, hit Play, and see the real-time stylization comparison.
 */
UCLASS(BlueprintType)
class ALAKAZAMPORTAL_API AAlakazamDemoActor : public AActor
{
	GENERATED_BODY()

public:
	AAlakazamDemoActor();

	// Alakazam controller (handles capture and stylization)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alakazam")
	UAlakazamController* AlakazamController;

	// Dynamic materials for display
	UPROPERTY(BlueprintReadOnly, Category = "Alakazam")
	UMaterialInstanceDynamic* OriginalMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "Alakazam")
	UMaterialInstanceDynamic* StylizedMaterial;

	// Auto-connect on play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam")
	bool bAutoConnect = true;

	// Show side-by-side comparison (false = stylized only fullscreen)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam")
	bool bSideBySide = true;

	// Capture resolution
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam")
	int32 CaptureWidth = 1280;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam")
	int32 CaptureHeight = 720;

	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void ToggleSideBySide();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnFrameReceived(UTexture2D* StylizedFrame);

	UFUNCTION()
	void OnConnected();

	UFUNCTION()
	void OnError(const FString& ErrorMessage);

private:
	void CreateSideBySideWidget();

	TSharedPtr<class SWidget> DisplayWidget;
	TSharedPtr<class SImage> OriginalImageWidget;
	TSharedPtr<class SImage> StylizedImageWidget;

	// Use TSharedPtr for brushes to ensure proper lifetime
	TSharedPtr<FSlateBrush> OriginalBrush;
	TSharedPtr<FSlateBrush> StylizedBrush;
};
