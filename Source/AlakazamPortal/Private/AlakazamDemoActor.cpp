#include "AlakazamDemoActor.h"
#include "AlakazamController.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "UObject/ConstructorHelpers.h"

AAlakazamDemoActor::AAlakazamDemoActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Create Scene Capture
	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(RootComponent);
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	// Create Alakazam Controller
	AlakazamController = CreateDefaultSubobject<UAlakazamController>(TEXT("AlakazamController"));
	AlakazamController->SceneCaptureComponent = SceneCapture;

	// Create output display quad
	OutputQuad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OutputQuad"));
	OutputQuad->SetupAttachment(RootComponent);
	OutputQuad->SetRelativeLocation(FVector(200.0f, 0.0f, 0.0f));
	OutputQuad->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	OutputQuad->SetRelativeScale3D(FVector(1.6f, 0.9f, 1.0f)); // 16:9 aspect

	// Use engine plane mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMesh.Succeeded())
	{
		OutputQuad->SetStaticMesh(PlaneMesh.Object);
	}
}

void AAlakazamDemoActor::BeginPlay()
{
	Super::BeginPlay();

	// Create an unlit material with texture parameter at runtime
	UMaterial* DynamicBaseMaterial = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
	DynamicBaseMaterial->SetShadingModel(MSM_Unlit);
	DynamicBaseMaterial->TwoSided = true;

	// Create texture sample parameter
	UMaterialExpressionTextureSampleParameter2D* TextureParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(DynamicBaseMaterial);
	TextureParam->ParameterName = FName("OutputTexture");
	TextureParam->SamplerType = SAMPLERTYPE_Color;
	DynamicBaseMaterial->GetExpressionCollection().AddExpression(TextureParam);

	// Connect texture RGB to emissive color
	DynamicBaseMaterial->GetEditorOnlyData()->EmissiveColor.Connect(0, TextureParam);

	// Compile the material
	DynamicBaseMaterial->PreEditChange(nullptr);
	DynamicBaseMaterial->PostEditChange();

	// Create material instance from our dynamic material
	OutputMaterial = UMaterialInstanceDynamic::Create(DynamicBaseMaterial, this);
	OutputQuad->SetMaterial(0, OutputMaterial);

	UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Created runtime unlit material"));

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

void AAlakazamDemoActor::OnConnected()
{
	UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Connected! Starting stream..."));
	AlakazamController->StartStreaming();
}

void AAlakazamDemoActor::OnFrameReceived(UTexture2D* StylizedFrame)
{
	if (OutputMaterial && StylizedFrame)
	{
		OutputMaterial->SetTextureParameterValue(FName("OutputTexture"), StylizedFrame);

		// Log success (first few frames only)
		static int32 LogCount = 0;
		if (LogCount < 3)
		{
			UE_LOG(LogTemp, Log, TEXT("Alakazam Demo: Applied stylized frame (%dx%d) to output"),
				StylizedFrame->GetSizeX(), StylizedFrame->GetSizeY());
			LogCount++;
		}
	}
}

void AAlakazamDemoActor::OnError(const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("Alakazam Demo: %s"), *ErrorMessage);
}
