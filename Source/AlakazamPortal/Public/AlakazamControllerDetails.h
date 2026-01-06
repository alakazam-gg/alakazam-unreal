#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "IDetailCustomization.h"
#include "Input/Reply.h"

class UAlakazamController;

/**
 * Custom Details panel for AlakazamController.
 * Adds an image picker for style extraction directly in the Editor.
 */
class FAlakazamControllerDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	// Cached reference to the controller being edited
	TWeakObjectPtr<UAlakazamController> CachedController;

	// Loaded image state
	UTexture2D* LoadedTexture = nullptr;
	FString LoadedImagePath;

	// UI Callbacks
	FReply OnBrowseClicked();
	FReply OnClearClicked();
	FReply OnExtractClicked();

	// Helper to load image from file
	void LoadImageFromPath(const FString& FilePath);

	// Get status text for display
	FText GetStatusText() const;

	// Get preview brush
	const FSlateBrush* GetPreviewBrush() const;

	// Check button enabled states
	bool IsExtractEnabled() const;
	bool IsClearEnabled() const;

	// Brush for preview
	TSharedPtr<FSlateBrush> PreviewBrush;
};

#endif // WITH_EDITOR
