#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/FileHandleLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>

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
    EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_HANDLE                 Volume, FileHandle;
    EFI_GRAPHICS_OUTPUT_PROTOCOL    *GraphicsOutput;
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
    Status = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get file info: %r\n", Status);
        goto CLEANUP_FILE;
    }
    FileSize = FileInfo->FileSize;
    FreePool(FileInfo);

    FileBuffer = AllocatePool(FileSize);
    ReadSize = FileSize;
    Status = FileHandle->Read(FileHandle, &ReadSize, FileBuffer);
    if (EFI_ERROR(Status) || ReadSize != FileSize) {
        Print(L"Failed to read file: %r (read %lu of %lu)\n", Status, ReadSize, FileSize);
        goto CLEANUP_BUFFER;
    }

    // 6. Parse BMP headers
    BmpFileHeader = (BMP_FILE_HEADER *)FileBuffer;
    BmpInfoHeader = (BMP_INFO_HEADER *)(FileBuffer + sizeof(BMP_FILE_HEADER));
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

    // 8. Center the image on screen
    DestinationX = (GraphicsOutput->Mode->Info->HorizontalResolution - BmpInfoHeader->biWidth) / 2;
    DestinationY = (GraphicsOutput->Mode->Info->VerticalResolution - BmpInfoHeader->biHeight) / 2;

    // 9. Draw the BMP using FrameBufferBlt
    Status = FrameBufferBlt (
        GraphicsOutput->Mode->FrameBufferBase,
        GraphicsOutput->Mode->Info,
        PixelData,
        EfiBltBufferToVideo,
        DestinationX,                               // DestinationX
        DestinationY,                               // DestinationY
        0,                                          // SourceX
        0,                                          // SourceY
        BmpInfoHeader->biWidth,                     // Width
        BmpInfoHeader->biHeight,                    // Height
        BmpInfoHeader->biWidth * 3                  // Delta (3 bytes per pixel for 24-bit)
    );
    if (EFI_ERROR(Status)) {
        Print(L"FrameBufferBlt failed: %r\n", Status);
        goto CLEANUP_BUFFER;
    }

    Print(L"BMP displayed successfully at %ux%u\n", DestinationX, DestinationY);
    Status = EFI_SUCCESS;

CLEANUP_BUFFER:
    if (FileBuffer) FreePool(FileBuffer);
CLEANUP_FILE:
    FileHandle->Close(FileHandle);
    Volume->Close(Volume);
    return Status;
}

EFI_STATUS ChainloadWindows(EFI_HANDLE ImageHandle) {
    EFI_STATUS Status;

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_HANDLE BootMgrHandle = NULL;

    // Locate Windows Boot Manager
    Status = gBS->LocateProtocol(&gEfiLoadedImageProtocolGuid, NULL, (VOID**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;

    // Load the Windows Boot Manager (adjust path if needed)
    CHAR16 *WinBootPath = L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
    EFI_HANDLE WinHandle = NULL;
    Status = gBS->LoadImage(
        FALSE,
        ImageHandle,
        WinBootPath,
        NULL,
        0,
        &WinHandle
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
    Print(L"Booting Neko OS...\n");

    EFI_STATUS Status = DrawBMP(ImageHandle, L"nekologo.bmp");
    if (EFI_ERROR(Status)) {
        Print(L"Failed to draw BMP: %r\n", Status);
    }

    // Show logo for 5 sec
    gBS->Stall(5000000);

    // Now chainload Windows
    Status = ChainloadWindows(ImageHandle);
    if (EFI_ERROR(Status)) {
        Print(L"Windows boot failed: %r\n", Status);
    }

    return EFI_SUCCESS;
}
