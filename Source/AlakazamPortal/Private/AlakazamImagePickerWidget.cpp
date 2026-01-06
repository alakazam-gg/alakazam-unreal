#include "AlakazamImagePickerWidget.h"
#include "AlakazamController.h"
#include "AlakazamImagePicker.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

void UAlakazamImagePickerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button clicks
	if (BrowseButton)
	{
		BrowseButton->OnClicked.AddDynamic(this, &UAlakazamImagePickerWidget::BrowseForImage);
	}

	if (ApplyButton)
	{
		ApplyButton->OnClicked.AddDynamic(this, &UAlakazamImagePickerWidget::ApplyStyle);
	}

	if (ClearButton)
	{
		ClearButton->OnClicked.AddDynamic(this, &UAlakazamImagePickerWidget::ClearImage);
	}

	// Bind to controller's style extracted event
	if (AlakazamController)
	{
		AlakazamController->OnStyleExtracted.AddDynamic(this, &UAlakazamImagePickerWidget::HandleStyleExtracted);
	}

	bIsDraggingOver = false;
	UpdateUI();
}

void UAlakazamImagePickerWidget::NativeDestruct()
{
	// Unbind from controller
	if (AlakazamController)
	{
		AlakazamController->OnStyleExtracted.RemoveDynamic(this, &UAlakazamImagePickerWidget::HandleStyleExtracted);
	}

	Super::NativeDestruct();
}

void UAlakazamImagePickerWidget::BrowseForImage()
{
	FString FilePath;
	if (UAlakazamImagePicker::OpenImageFileDialog(FilePath))
	{
		LoadImageFromPath(FilePath);
	}
}

void UAlakazamImagePickerWidget::LoadImageFromPath(const FString& FilePath)
{
	UTexture2D* Texture = UAlakazamImagePicker::LoadImageFromFile(FilePath);
	if (Texture)
	{
		LoadedTexture = Texture;
		LoadedFilePath = FilePath;

		OnImageSelected.Broadcast(LoadedTexture);
		SetStatusText(FString::Printf(TEXT("Loaded: %s"), *FPaths::GetCleanFilename(FilePath)));
		UpdateUI();

		// Auto-extract style immediately (will auto-connect if needed)
		if (AlakazamController)
		{
			AlakazamController->ExtractStyleFromImage(LoadedTexture);
			SetStatusText(TEXT("Extracting style..."));
		}
	}
	else
	{
		SetStatusText(TEXT("Failed to load image"));
	}
}

void UAlakazamImagePickerWidget::ApplyStyle()
{
	if (!LoadedTexture)
	{
		SetStatusText(TEXT("No image loaded"));
		return;
	}

	if (!AlakazamController)
	{
		SetStatusText(TEXT("No controller assigned"));
		return;
	}

	// Use ExtractStyleFromImage which auto-connects if needed
	AlakazamController->ExtractStyleFromImage(LoadedTexture);
	SetStatusText(TEXT("Extracting style..."));
}

void UAlakazamImagePickerWidget::ClearImage()
{
	LoadedTexture = nullptr;
	LoadedFilePath.Empty();

	// Clear image style on controller
	if (AlakazamController)
	{
		AlakazamController->ClearImageStyle();
	}

	SetStatusText(TEXT("Drop image here or click Browse"));
	UpdateUI();
}

void UAlakazamImagePickerWidget::UpdateUI()
{
	// Update preview image
	if (PreviewImage)
	{
		if (LoadedTexture)
		{
			PreviewImage->SetBrushFromTexture(LoadedTexture);
			PreviewImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			PreviewImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// Update button states
	if (ApplyButton)
	{
		ApplyButton->SetIsEnabled(LoadedTexture != nullptr);
	}

	if (ClearButton)
	{
		ClearButton->SetIsEnabled(LoadedTexture != nullptr);
	}

	// Update drop zone appearance
	if (DropZone)
	{
		FLinearColor ZoneColor;
		if (bIsDraggingOver)
		{
			ZoneColor = FLinearColor(0.3f, 0.5f, 0.7f, 0.9f); // Blue when dragging
		}
		else if (LoadedTexture)
		{
			ZoneColor = FLinearColor(0.2f, 0.5f, 0.3f, 0.9f); // Green when loaded
		}
		else
		{
			ZoneColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.8f); // Gray default
		}
		DropZone->SetBrushColor(ZoneColor);
	}
}

void UAlakazamImagePickerWidget::SetStatusText(const FString& Text)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Text));
	}
}

void UAlakazamImagePickerWidget::HandleStyleExtracted(const FString& ExtractedPrompt)
{
	SetStatusText(FString::Printf(TEXT("Style applied: %s"), *ExtractedPrompt.Left(50)));
	OnStyleApplied.Broadcast(ExtractedPrompt);
}

// Drag and drop handlers
bool UAlakazamImagePickerWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bIsDraggingOver = false;
	UpdateUI();

	// Check for external file drop (from OS)
	// Note: Unreal's drag drop from OS requires platform-specific handling
	// For now, this handles internal UMG drag operations

	if (InOperation)
	{
		// Handle custom drag operations if needed
		UE_LOG(LogTemp, Log, TEXT("AlakazamImagePickerWidget: Drop received from UMG"));
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

bool UAlakazamImagePickerWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return true; // Accept drag over
}

void UAlakazamImagePickerWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bIsDraggingOver = true;
	UpdateUI();
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
}

void UAlakazamImagePickerWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bIsDraggingOver = false;
	UpdateUI();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}
