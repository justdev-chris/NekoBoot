[Defines]
  PLATFORM_NAME           = NekoPlatform
  PLATFORM_GUID           = 87654321-BA09-FEDC-4321-098765FEDCBA
  DSC_SPECIFICATION       = 0x00010005
  OUTPUT_DIRECTORY        = Build
  SUPPORTED_ARCHITECTURES = X64
  BUILD_TARGETS           = DEBUG
  SKUID                   = DEFAULT

[Components]
  NekoBoot/NekoBoot.inf
