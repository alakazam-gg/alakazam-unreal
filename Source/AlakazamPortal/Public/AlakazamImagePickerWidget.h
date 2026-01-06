#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "AlakazamImagePickerWidget.generated.h"

class UAlakazamController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnImageSelected, UTexture2D*, SelectedImage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStyleApplied, const FString&, ExtractedPrompt);

/**
 * Image Picker Widget for Alakazam style extraction.
 * Supports file browser and drag & drop.
 */
UCLASS()
class ALAKAZAMPORTAL_API UAlakazamImagePickerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Reference to the Alakazam controller */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alakazam")
	UAlakazamController* AlakazamController;

	/** Event fired when an image is selected (loaded) */
	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnImageSelected OnImageSelected;

	/** Event fired when style is extracted and applied */
	UPROPERTY(BlueprintAssignable, Category = "Alakazam|Events")
	FOnStyleApplied OnStyleApplied;

	/** Open file browser to select an image */
	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void BrowseForImage();

	/** Apply the currently loaded image to extract style */
	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void ApplyStyle();

	/** Clear the currently loaded image */
	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void ClearImage();

	/** Load an image from a file path */
	UFUNCTION(BlueprintCallable, Category = "Alakazam")
	void LoadImageFromPath(const FString& FilePath);

	/** Get the currently loaded texture */
	UFUNCTION(BlueprintPure, Category = "Alakazam")
	UTexture2D* GetLoadedTexture() const { return LoadedTexture; }

	/** Check if an image is loaded */
	UFUNCTION(BlueprintPure, Category = "Alakazam")
	bool HasImageLoaded() const { return LoadedTexture != nullptr; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Drag and drop support
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Bind to named widgets in the widget blueprint */
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* PreviewImage;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BrowseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ApplyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ClearButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* DropZone;

private:
	UPROPERTY()
	UTexture2D* LoadedTexture;

	FString LoadedFilePath;
	bool bIsDraggingOver;

	void UpdateUI();
	void SetStatusText(const FString& Text);

	UFUNCTION()
	void HandleStyleExtracted(const FString& ExtractedPrompt);
};
