#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include <Library/UefiBootManagerLib.h>

// draw BMP logo
EFI_STATUS DrawBmp(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16* FileName) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Volume;
    EFI_FILE_PROTOCOL* Root;
    EFI_FILE_PROTOCOL* File;

    SystemTable->BootServices->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID**)&Volume);
    Volume->OpenVolume(Volume, &Root);

    Root->Open(Root, &File, FileName, EFI_FILE_MODE_READ, 0);

    EFI_FILE_INFO* FileInfo;
    UINTN FileInfoSize = sizeof(EFI_FILE_INFO) + 200;
    FileInfo = AllocateZeroPool(FileInfoSize);
    File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);

    UINTN FileSize = (UINTN)FileInfo->FileSize;
    UINT8* Buffer = AllocateZeroPool(FileSize);

    File->Read(File, &FileSize, Buffer);

    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics;
    SystemTable->BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&Graphics);

    Graphics->Blt(
        Graphics,
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)(Buffer + 54), // BMP pixel data offset
        EfiBltBufferToVideo,
        0, 0, 50, 50,       // dest x, y
        200, 200, 0         // width, height
    );

    FreePool(Buffer);
    FreePool(FileInfo);
    File->Close(File);
    Root->Close(Root);

    return EFI_SUCCESS;
}

// fake boot log animation
void FakeBootLog() {
    CHAR16* log[] = {
        L"[ OK ] Initializing graphics",
        L"[ OK ] Loading kernel modules",
        L"[ OK ] Starting network services",
        L"[ OK ] Mounting filesystems",
        L"[ OK ] Launching system processes"
    };

    for (int i = 0; i < sizeof(log)/sizeof(log[0]); i++) {
        Print(L"%s\n", log[i]);
        gBS->Stall(500000); // 0.5 sec between lines
    }
}

// dots animation for fun
void DotsAnimation() {
    Print(L"Booting Neko OS");
    for (int i=0; i<6; i++) {
        gBS->Stall(500000);
        Print(L".");
    }
    Print(L"\n");
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
    // draw logo
    DrawBmp(ImageHandle, SystemTable, L"nekologo.bmp");

    // fake boot log
    FakeBootLog();

    // dots animation
    DotsAnimation();

    Print(L"Launching Windows...\n");

    // chainload Windows Boot Manager
    EFI_HANDLE WindowsHandle;
    EFI_STATUS Status = EfiBootManagerLoadOption(
        0,
        L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
        &WindowsHandle
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to boot Windows. Error: %r\n", Status);
        while(1) gBS->Stall(500000);
    }

    return EFI_SUCCESS;
}