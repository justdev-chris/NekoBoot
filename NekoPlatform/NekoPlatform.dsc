[Defines]
  PLATFORM_NAME           = NekoPlatform
  PLATFORM_GUID           = 2e18045e-66ad-42d0-a0af-43528d667de0
  PLATFORM_VERSION        = 0.1
  DSC_SPECIFICATION       = 0x00010005
  OUTPUT_DIRECTORY        = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES = X64
  BUILD_TARGETS           = DEBUG|RELEASE
  SKUID_IDENTIFIER        = DEFAULT

[LibraryClasses]
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf

[Components]
  NekoBoot/NekoBoot.inf