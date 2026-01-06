#include "AlakazamImagePicker.h"
#include "AlakazamController.h"
#include "DesktopPlatformModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"

bool UAlakazamImagePicker::OpenImageFileDialog(FString& OutFilePath)
{
	return OpenImageFileDialogWithTitle(TEXT("Select Style Reference Image"), OutFilePath);
}

bool UAlakazamImagePicker::OpenImageFileDialogWithTitle(const FString& DialogTitle, FString& OutFilePath)
{
	OutFilePath.Empty();

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: Desktop platform not available"));
		return false;
	}

	// Get the native window handle
	void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}
	}

	// File type filter for images
	const FString FileTypes = TEXT("Image Files (*.png;*.jpg;*.jpeg;*.bmp)|*.png;*.jpg;*.jpeg;*.bmp|All Files (*.*)|*.*");

	TArray<FString> OutFiles;
	bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		DialogTitle,
		FPaths::GetProjectFilePath(),
		TEXT(""),
		FileTypes,
		EFileDialogFlags::None,
		OutFiles
	);

	if (bOpened && OutFiles.Num() > 0)
	{
		OutFilePath = OutFiles[0];
		UE_LOG(LogTemp, Log, TEXT("AlakazamImagePicker: Selected file: %s"), *OutFilePath);
		return true;
	}

	return false;
}

UTexture2D* UAlakazamImagePicker::LoadImageFromFile(const FString& FilePath)
{
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: File path is empty"));
		return nullptr;
	}

	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: File not found: %s"), *FilePath);
		return nullptr;
	}

	// Read file data
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: Failed to read file: %s"), *FilePath);
		return nullptr;
	}

	// Determine image format from extension
	FString Extension = FPaths::GetExtension(FilePath).ToLower();
	EImageFormat ImageFormat = EImageFormat::Invalid;

	if (Extension == TEXT("png"))
	{
		ImageFormat = EImageFormat::PNG;
	}
	else if (Extension == TEXT("jpg") || Extension == TEXT("jpeg"))
	{
		ImageFormat = EImageFormat::JPEG;
	}
	else if (Extension == TEXT("bmp"))
	{
		ImageFormat = EImageFormat::BMP;
	}
	else
	{
		// Try to detect from magic bytes
		if (FileData.Num() >= 4)
		{
			if (FileData[0] == 0x89 && FileData[1] == 0x50 && FileData[2] == 0x4E && FileData[3] == 0x47)
			{
				ImageFormat = EImageFormat::PNG;
			}
			else if (FileData[0] == 0xFF && FileData[1] == 0xD8)
			{
				ImageFormat = EImageFormat::JPEG;
			}
			else if (FileData[0] == 0x42 && FileData[1] == 0x4D)
			{
				ImageFormat = EImageFormat::BMP;
			}
		}
	}

	if (ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: Unsupported image format: %s"), *Extension);
		return nullptr;
	}

	// Decompress image
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if (!ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: Failed to decompress image: %s"), *FilePath);
		return nullptr;
	}

	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: Failed to get raw image data: %s"), *FilePath);
		return nullptr;
	}

	int32 Width = ImageWrapper->GetWidth();
	int32 Height = ImageWrapper->GetHeight();

	// Create texture
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: Failed to create texture"));
		return nullptr;
	}

	// Copy pixel data
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
	Mip.BulkData.Unlock();

	Texture->UpdateResource();

	UE_LOG(LogTemp, Log, TEXT("AlakazamImagePicker: Loaded image %s (%dx%d)"), *FilePath, Width, Height);
	return Texture;
}

bool UAlakazamImagePicker::BrowseAndApplyStyleImage(UAlakazamController* Controller)
{
	if (!Controller)
	{
		UE_LOG(LogTemp, Error, TEXT("AlakazamImagePicker: Controller is null"));
		return false;
	}

	FString FilePath;
	if (!OpenImageFileDialog(FilePath))
	{
		return false;
	}

	UTexture2D* Texture = LoadImageFromFile(FilePath);
	if (!Texture)
	{
		return false;
	}

	Controller->SetStyleFromImage(Texture);
	return true;
}

bool UAlakazamImagePicker::IsSupportedImageFormat(const FString& FilePath)
{
	FString Extension = FPaths::GetExtension(FilePath).ToLower();
	return Extension == TEXT("png") ||
	       Extension == TEXT("jpg") ||
	       Extension == TEXT("jpeg") ||
	       Extension == TEXT("bmp");
}

TArray<FString> UAlakazamImagePicker::GetSupportedImageExtensions()
{
	return TArray<FString>{ TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("bmp") };
}
