#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/FileHandleLib.h>

#pragma pack(1)
typedef struct {
    UINT16 bfType;
    UINT32 bfSize;
    UINT16 bfReserved1;
    UINT16 bfReserved2;
    UINT32 bfOffBits;
} BMP_FILE_HEADER;

typedef struct {
    UINT32 biSize;
    INT32  biWidth;
    INT32  biHeight;
    UINT16 biPlanes;
    UINT16 biBitCount;
    UINT32 biCompression;
    UINT32 biSizeImage;
    INT32  biXPelsPerMeter;
    INT32  biYPelsPerMeter;
    UINT32 biClrUsed;
    UINT32 biClrImportant;
} BMP_INFO_HEADER;
#pragma pack()

EFI_STATUS DrawBMP(EFI_HANDLE ImageHandle, CHAR16 *FileName) {
    EFI_STATUS                      Status;
    EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
    EFI_FILE_HANDLE                 Volume = NULL, FileHandle = NULL;
    EFI_GRAPHICS_OUTPUT_PROTOCOL    *GraphicsOutput = NULL;
    UINTN                           FileSize, ReadSize;
    UINT8                           *FileBuffer = NULL;
    BMP_FILE_HEADER                 *BmpFileHeader;
    BMP_INFO_HEADER                 *BmpInfoHeader;
    UINT8                           *PixelData;
    UINT32                          DestinationX, DestinationY;

    // 1. Get the loaded image protocol to find our device
    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get LoadedImage: %r\n", Status);
        return Status;
    }

    // 2. Get the file system on the device where our app is loaded
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get FileSystem: %r\n", Status);
        return Status;
    }

    // 3. Open the root directory
    Status = FileSystem->OpenVolume(FileSystem, &Volume);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open volume: %r\n", Status);
        return Status;
    }

    // 4. Open the BMP file
    Status = Volume->Open(Volume, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open file %s: %r\n", FileName, Status);
        Volume->Close(Volume);
        return Status;
    }

    // 5. Get file size and read entire file
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 128;
    FileInfo = AllocatePool(InfoSize);
    if (FileInfo == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CLEANUP_FILE;
    }
    
    Status = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get file info: %r\n", Status);
        FreePool(FileInfo);
        goto CLEANUP_FILE;
    }
    FileSize = FileInfo->FileSize;
    FreePool(FileInfo);

    FileBuffer = AllocatePool(FileSize);
    if (FileBuffer == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CLEANUP_FILE;
    }
    
    ReadSize = FileSize;
    Status = FileHandle->Read(FileHandle, &ReadSize, FileBuffer);
    if (EFI_ERROR(Status) || ReadSize != FileSize) {
        Print(L"Failed to read file: %r (read %lu of %lu)\n", Status, ReadSize, FileSize);
        goto CLEANUP_BUFFER;
    }

    // 6. Parse BMP headers
    BmpFileHeader = (BMP_FILE_HEADER *)FileBuffer;
    BmpInfoHeader = (BMP_INFO_HEADER *)(FileBuffer + sizeof(BMP_FILE_HEADER));
    
    // Validate offsets
    if (BmpFileHeader->bfOffBits >= FileSize) {
        Print(L"Invalid BMP offset\n");
        Status = EFI_UNSUPPORTED;
        goto CLEANUP_BUFFER;
    }
    
    PixelData = FileBuffer + BmpFileHeader->bfOffBits;

    // Validate BMP (basic checks)
    if (BmpFileHeader->bfType != 0x4D42) { // "BM"
        Print(L"Not a valid BMP file\n");
        Status = EFI_UNSUPPORTED;
        goto CLEANUP_BUFFER;
    }
    if (BmpInfoHeader->biBitCount != 24) {
        Print(L"Only 24-bit BMP supported\n");
        Status = EFI_UNSUPPORTED;
        goto CLEANUP_BUFFER;
    }

    // 7. Get graphics protocol
    Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&GraphicsOutput);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get GraphicsOutput: %r\n", Status);
        goto CLEANUP_BUFFER;
    }

    // 8. Center the image on screen (slightly up - at 40% from top instead of 50%)
    DestinationX = (GraphicsOutput->Mode->Info->HorizontalResolution - BmpInfoHeader->biWidth) / 2;
    DestinationY = (GraphicsOutput->Mode->Info->VerticalResolution - BmpInfoHeader->biHeight) / 4; // 1/4 from top

    // 9. Draw the BMP using GraphicsOutput->Blt
    // Convert 24-bit BGR to 32-bit BGRx pixel format
    UINTN PixelCount = BmpInfoHeader->biWidth * BmpInfoHeader->biHeight;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer = AllocatePool(PixelCount * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    
    if (BltBuffer == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CLEANUP_BUFFER;
    }
    
    // BMP is stored bottom-to-top, need to flip vertically
    // Also convert transparency (white pixels) to black
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Dst = BltBuffer;
    
    // Calculate row size with padding
    UINTN RowSize = BmpInfoHeader->biWidth * 3; // 24-bit = 3 bytes per pixel
    if (RowSize % 4 != 0) RowSize += (4 - (RowSize % 4)); // BMP row padding
    
    // Process pixels from bottom to top (flip vertically)
    for (INT32 y = BmpInfoHeader->biHeight - 1; y >= 0; y--) {
        UINT8 *RowStart = PixelData + (y * RowSize);
        for (INT32 x = 0; x < BmpInfoHeader->biWidth; x++) {
            UINT8 *Pixel = RowStart + (x * 3);
            
            // Check for white/transparent pixels (RGB = 255, 255, 255 or close)
            // Convert to black if mostly white
            if (Pixel[2] > 240 && Pixel[1] > 240 && Pixel[0] > 240) {
                // White/transparent → Black
                Dst->Blue  = 0;
                Dst->Green = 0;
                Dst->Red   = 0;
            } else {
                // Normal pixel (BGR to RGB)
                Dst->Blue  = Pixel[0];
                Dst->Green = Pixel[1];
                Dst->Red   = Pixel[2];
            }
            Dst->Reserved = 0;
            Dst++;
        }
    }
    
    Status = GraphicsOutput->Blt(
        GraphicsOutput,
        BltBuffer,
        EfiBltBufferToVideo,
        0, 0,                                 // SourceX, SourceY
        DestinationX, DestinationY,           // DestinationX, DestinationY
        BmpInfoHeader->biWidth,               // Width
        BmpInfoHeader->biHeight,              // Height
        0                                     // Delta (0 for BltBuffer)
    );
    
    FreePool(BltBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"GraphicsOutput->Blt failed: %r\n", Status);
        goto CLEANUP_BUFFER;
    }

    Status = EFI_SUCCESS;

CLEANUP_BUFFER:
    if (FileBuffer) FreePool(FileBuffer);
CLEANUP_FILE:
    if (FileHandle) FileHandle->Close(FileHandle);
    if (Volume) Volume->Close(Volume);
    return Status;
}

EFI_STATUS ChainloadWindows(EFI_HANDLE ImageHandle) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_HANDLE WinHandle = NULL;

    // Locate our own loaded image protocol
    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;

    // Load the Windows Boot Manager from the same device
    CHAR16 *WinBootPath = L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
    Status = gBS->LoadImage(
        FALSE,                    // BootPolicy
        ImageHandle,              // ParentImageHandle
        NULL,                     // DevicePath (NULL = use LoadedImage->DeviceHandle)
        WinBootPath,              // FilePath
        0,                        // SourceSize
        &WinHandle                // ImageHandle
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to load Windows: %r\n", Status);
        return Status;
    }

    // Start Windows Boot Manager
    Status = gBS->StartImage(WinHandle, NULL, NULL);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to start Windows: %r\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    UINTN MaxCol, MaxRow;
    
    // Clear screen
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    
    // Draw BMP logo (centered, slightly up)
    Status = DrawBMP(ImageHandle, L"nekologo.bmp");
    if (EFI_ERROR(Status)) {
        // If BMP fails, just continue with text
        Print(L"Failed to draw logo: %r\n", Status);
    }
    
    // Get console dimensions
    Status = SystemTable->ConOut->QueryMode(SystemTable->ConOut, 
                                           SystemTable->ConOut->Mode->Mode, 
                                           &MaxCol, &MaxRow);
    if (EFI_ERROR(Status)) {
        // Default values if query fails
        MaxCol = 80;
        MaxRow = 25;
    }
    
    // Position text: slightly down (3/4 from top), centered
    // "Booting Up NekoOS" is 19 characters
    UINTN TextRow = (MaxRow * 3) / 4;  // 75% down the screen
    UINTN TextCol = (MaxCol - 19) / 2; // Center the 19-character string
    
    // Ensure we're within bounds
    if (TextCol > MaxCol) TextCol = 0;
    if (TextRow > MaxRow) TextRow = MaxRow - 1;
    
    // Move cursor and display text
    SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, TextCol, TextRow);
    Print(L"Booting Up NekoOS");
    
    // Wait for 5 seconds
    gBS->Stall(5000000);
    
    // Clear the text
    SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, TextCol, TextRow);
    Print(L"                   ");  // 19 spaces to clear
    
    // Chainload Windows
    Status = ChainloadWindows(ImageHandle);
    if (EFI_ERROR(Status)) {
        // Display error at bottom
        SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, 0, MaxRow - 1);
        Print(L"Windows boot failed: %r", Status);
        gBS->Stall(3000000);  // Show error for 3 seconds
    }

    return Status;
}
