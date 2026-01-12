#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>

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
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_HANDLE Volume, File;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP;

    UINT8 *FileBuffer = NULL;
    UINTN FileSize;

    Status = gBS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImage
    );
    if (EFI_ERROR(Status)) return Status;

    Status = gBS->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&FileSystem
    );
    if (EFI_ERROR(Status)) return Status;

    Status = FileSystem->OpenVolume(FileSystem, &Volume);
    if (EFI_ERROR(Status)) return Status;

    Status = Volume->Open(
        Volume,
        &File,
        FileName,
        EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR(Status)) return Status;

    EFI_FILE_INFO *Info;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 200;
    Info = AllocatePool(InfoSize);

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, Info);
    if (EFI_ERROR(Status)) return Status;

    FileSize = Info->FileSize;
    FreePool(Info);

    FileBuffer = AllocatePool(FileSize);
    File->Read(File, &FileSize, FileBuffer);
    File->Close(File);
    Volume->Close(Volume);

    BMP_FILE_HEADER *FH = (BMP_FILE_HEADER*)FileBuffer;
    BMP_INFO_HEADER *IH = (BMP_INFO_HEADER*)(FileBuffer + sizeof(BMP_FILE_HEADER));

    if (FH->bfType != 0x4D42 || IH->biBitCount != 24) {
        FreePool(FileBuffer);
        return EFI_UNSUPPORTED;
    }

    Status = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID**)&GOP
    );
    if (EFI_ERROR(Status)) return Status;

    UINT32 Width = IH->biWidth;
    UINT32 Height = IH->biHeight;

    UINT32 ScreenW = GOP->Mode->Info->HorizontalResolution;
    UINT32 ScreenH = GOP->Mode->Info->VerticalResolution;

    UINT32 DestX = (ScreenW - Width) / 2;
    UINT32 DestY = (ScreenH - Height) / 2;

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Blt =
        AllocatePool(Width * Height * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

    UINT8 *PixelData = FileBuffer + FH->bfOffBits;

    // --- FIX 1: BMP upside-down + fake transparency ---
    for (UINT32 y = 0; y < Height; y++) {
        UINT8 *SrcRow = PixelData + (Height - 1 - y) * Width * 3;
        for (UINT32 x = 0; x < Width; x++) {
            UINT8 B = SrcRow[x * 3 + 0];
            UINT8 G = SrcRow[x * 3 + 1];
            UINT8 R = SrcRow[x * 3 + 2];

            EFI_GRAPHICS_OUTPUT_BLT_PIXEL *P =
                &Blt[y * Width + x];

            // White → black (fake transparency)
            if (R > 245 && G > 245 && B > 245) {
                P->Red = P->Green = P->Blue = 0;
            } else {
                P->Red = R;
                P->Green = G;
                P->Blue = B;
            }
            P->Reserved = 0;
        }
    }

    GOP->Blt(
        GOP,
        Blt,
        EfiBltBufferToVideo,
        0, 0,
        DestX, DestY,
        Width, Height,
        0
    );

    FreePool(Blt);
    FreePool(FileBuffer);

    // --- FIX 3: centered text ---
    UINTN Cols = GOP->Mode->Info->HorizontalResolution / 8;
    UINTN Rows = GOP->Mode->Info->VerticalResolution / 16;

    UINTN TextX = (Cols - 22) / 2;
    UINTN TextY = (DestY + Height + 20) / 16;

    gST->ConOut->SetCursorPosition(gST->ConOut, TextX, TextY);
    Print(L"Booting up Neko OS...");

    return EFI_SUCCESS;
}

EFI_STATUS ChainloadWindows(EFI_HANDLE ImageHandle) {
    EFI_STATUS Status;
    EFI_HANDLE WinHandle = NULL;

    CHAR16 *WinPath = L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";

    Status = gBS->LoadImage(
        FALSE,
        ImageHandle,
        NULL,
        WinPath,
        0,
        &WinHandle
    );
    if (EFI_ERROR(Status)) return Status;

    return gBS->StartImage(WinHandle, NULL, NULL);
}

EFI_STATUS EFIAPI UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
) {
    DrawBMP(ImageHandle, L"nekologo.bmp");

    // --- FIX 2: 10 second delay ---
    gBS->Stall(10 * 1000 * 1000);

    ChainloadWindows(ImageHandle);
    return EFI_SUCCESS;
}