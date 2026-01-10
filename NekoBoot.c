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
    // same DrawBMP as before...
    // handles centering & scaling
    return EFI_SUCCESS;
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