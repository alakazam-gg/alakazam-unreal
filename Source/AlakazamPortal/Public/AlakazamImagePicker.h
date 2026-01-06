#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AlakazamImagePicker.generated.h"

/**
 * Blueprint Function Library for Alakazam image picking functionality.
 * Provides file browser and image loading utilities.
 */
UCLASS()
class ALAKAZAMPORTAL_API UAlakazamImagePicker : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Opens a native file dialog to select an image file.
	 * @param OutFilePath The selected file path (empty if cancelled)
	 * @return True if a file was selected, false if cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Image")
	static bool OpenImageFileDialog(FString& OutFilePath);

	/**
	 * Opens a native file dialog with custom title.
	 * @param DialogTitle The title for the dialog window
	 * @param OutFilePath The selected file path (empty if cancelled)
	 * @return True if a file was selected, false if cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Image")
	static bool OpenImageFileDialogWithTitle(const FString& DialogTitle, FString& OutFilePath);

	/**
	 * Load an image file from disk as a Texture2D.
	 * @param FilePath Full path to the image file
	 * @return The loaded texture, or nullptr if loading failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Image")
	static UTexture2D* LoadImageFromFile(const FString& FilePath);

	/**
	 * Load an image and send it to the Alakazam controller for style extraction.
	 * Combines OpenImageFileDialog, LoadImageFromFile, and SetStyleFromImage.
	 * @param Controller The Alakazam controller to send the image to
	 * @return True if image was selected and sent successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Alakazam|Image")
	static bool BrowseAndApplyStyleImage(class UAlakazamController* Controller);

	/**
	 * Check if a file path points to a supported image format.
	 * @param FilePath The file path to check
	 * @return True if the file extension is a supported image format
	 */
	UFUNCTION(BlueprintPure, Category = "Alakazam|Image")
	static bool IsSupportedImageFormat(const FString& FilePath);

	/**
	 * Get supported image file extensions.
	 * @return Array of supported extensions (e.g., "png", "jpg", "jpeg")
	 */
	UFUNCTION(BlueprintPure, Category = "Alakazam|Image")
	static TArray<FString> GetSupportedImageExtensions();
};
