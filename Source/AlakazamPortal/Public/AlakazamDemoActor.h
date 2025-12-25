#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlakazamDemoActor.generated.h"

class UAlakazamController;
class USceneCaptureComponent2D;
class UStaticMeshComponent;

/**
 * Drop-in demo actor for Alakazam Portal.
 * Place in level, hit Play, and see the stylized output on the quad.
 */
UCLASS(BlueprintType)
class ALAKAZAMPORTAL_API AAlakazamDemoActor : public AActor
{
	GENERATED_BODY()

public:
	AAlakazamDemoActor();

	// Alakazam controller
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alakazam")
	UAlakazamController* AlakazamController;

	// Captures the scene
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alakazam")
	USceneCaptureComponent2D* SceneCapture;

	// Quad to display output
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alakazam")
	UStaticMeshComponent* OutputQuad;

	// Dynamic material for output display
	UPROPERTY(BlueprintReadOnly, Category = "Alakazam")
	UMaterialInstanceDynamic* OutputMaterial;

	// Auto-connect on play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam")
	bool bAutoConnect = true;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnFrameReceived(UTexture2D* StylizedFrame);

	UFUNCTION()
	void OnConnected();

	UFUNCTION()
	void OnError(const FString& ErrorMessage);
};
