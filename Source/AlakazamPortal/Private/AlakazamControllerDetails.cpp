#include "AlakazamControllerDetails.h"

#if WITH_EDITOR

#include "AlakazamController.h"
#include "AlakazamImagePicker.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

TSharedRef<IDetailCustomization> FAlakazamControllerDetails::MakeInstance()
{
	return MakeShareable(new FAlakazamControllerDetails);
}

void FAlakazamControllerDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get the object being edited
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	if (ObjectsBeingCustomized.Num() == 1)
	{
		CachedController = Cast<UAlakazamController>(ObjectsBeingCustomized[0].Get());
	}

	// Add custom category for Style from Image
	IDetailCategoryBuilder& StyleCategory = DetailBuilder.EditCategory("Alakazam|Style from Image",
		FText::FromString("Style from Image"), ECategoryPriority::Important);

	// Add image preview
	StyleCategory.AddCustomRow(FText::FromString("Image Preview"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Preview"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MaxDesiredWidth(200.0f)
		[
			SNew(SBox)
			.WidthOverride(150.0f)
			.HeightOverride(100.0f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
				.Padding(2.0f)
				[
					SNew(SImage)
					.Image(this, &FAlakazamControllerDetails::GetPreviewBrush)
				]
			]
		];

	// Add status text
	StyleCategory.AddCustomRow(FText::FromString("Status"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Status"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text(this, &FAlakazamControllerDetails::GetStatusText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	// Add Browse button
	StyleCategory.AddCustomRow(FText::FromString("Browse"))
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString("Browse..."))
				.OnClicked(this, &FAlakazamControllerDetails::OnBrowseClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString("Extract Style"))
				.OnClicked(this, &FAlakazamControllerDetails::OnExtractClicked)
				.IsEnabled(this, &FAlakazamControllerDetails::IsExtractEnabled)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString("Clear"))
				.OnClicked(this, &FAlakazamControllerDetails::OnClearClicked)
				.IsEnabled(this, &FAlakazamControllerDetails::IsClearEnabled)
			]
		];

	// Add extracted style display
	StyleCategory.AddCustomRow(FText::FromString("Extracted Style"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Extracted Style"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MaxDesiredWidth(300.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() -> FText
			{
				// Get extracted style from controller when using image style
				if (CachedController.IsValid() && CachedController->bIsUsingImageStyle)
				{
					return FText::FromString(CachedController->Prompt);
				}
				return FText::FromString(TEXT("(none)"));
			})
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.AutoWrapText(true)
		];
}

FReply FAlakazamControllerDetails::OnBrowseClicked()
{
	FString FilePath;
	if (UAlakazamImagePicker::OpenImageFileDialog(FilePath))
	{
		LoadImageFromPath(FilePath);
	}
	return FReply::Handled();
}

FReply FAlakazamControllerDetails::OnClearClicked()
{
	LoadedTexture = nullptr;
	LoadedImagePath.Empty();
	PreviewBrush.Reset();

	if (CachedController.IsValid())
	{
		CachedController->ClearImageStyle();
	}

	return FReply::Handled();
}

FReply FAlakazamControllerDetails::OnExtractClicked()
{
	if (!LoadedTexture || !CachedController.IsValid())
	{
		return FReply::Handled();
	}

	// Extract style (will auto-connect if needed)
	CachedController->ExtractStyleFromImage(LoadedTexture);

	return FReply::Handled();
}

void FAlakazamControllerDetails::LoadImageFromPath(const FString& FilePath)
{
	UTexture2D* Texture = UAlakazamImagePicker::LoadImageFromFile(FilePath);
	if (Texture)
	{
		LoadedTexture = Texture;
		LoadedImagePath = FilePath;

		// Create brush for preview
		PreviewBrush = MakeShareable(new FSlateBrush());
		PreviewBrush->SetResourceObject(LoadedTexture);
		PreviewBrush->ImageSize = FVector2D(150.0f, 100.0f);
		PreviewBrush->DrawAs = ESlateBrushDrawType::Image;
	}
}


FText FAlakazamControllerDetails::GetStatusText() const
{
	if (CachedController.IsValid() && CachedController->bIsExtractingStyle)
	{
		return FText::FromString(TEXT("Extracting style..."));
	}

	if (CachedController.IsValid() && CachedController->bIsUsingImageStyle)
	{
		return FText::FromString(TEXT("Style ready"));
	}

	if (!LoadedImagePath.IsEmpty())
	{
		return FText::FromString(FString::Printf(TEXT("Loaded: %s"), *FPaths::GetCleanFilename(LoadedImagePath)));
	}

	return FText::FromString(TEXT("No image loaded"));
}

const FSlateBrush* FAlakazamControllerDetails::GetPreviewBrush() const
{
	if (PreviewBrush.IsValid())
	{
		return PreviewBrush.Get();
	}
	return nullptr;
}

bool FAlakazamControllerDetails::IsExtractEnabled() const
{
	return LoadedTexture != nullptr && CachedController.IsValid();
}

bool FAlakazamControllerDetails::IsClearEnabled() const
{
	if (LoadedTexture != nullptr)
	{
		return true;
	}
	if (CachedController.IsValid() && CachedController->bIsUsingImageStyle)
	{
		return true;
	}
	return false;
}

#endif // WITH_EDITOR
