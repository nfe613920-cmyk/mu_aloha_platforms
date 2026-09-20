[PcdsFixedAtBuild.common]
gAndromedaPkgTokenSpaceGuid.PcdMipiFrameBufferWidth|2944
gAndromedaPkgTokenSpaceGuid.PcdMipiFrameBufferHeight|1840

# Display Caller
gAndromedaPkgTokenSpaceGuid.PcdDisplayCallerExitDisableDisplay|FALSE
gAndromedaPkgTokenSpaceGuid.PcdDisplayCallerStallBeforeEnable|1000

# Smbios Info
gAndromedaPkgTokenSpaceGuid.PcdSmbiosSystemBrand|"Lenovo"
gAndromedaPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"TB520FU"

gAndromedaPkgTokenSpaceGuid.PcdABLProduct|"Lapis"

  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2f
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x800fee0f

[PcdsDynamicDefault.common]
gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|2944
gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|1840
gEfiMdeModulePkgTokenSpaceGuid.PcdSetupVideoHorizontalResolution|2944
gEfiMdeModulePkgTokenSpaceGuid.PcdSetupVideoVerticalResolution|1840
gEfiMdeModulePkgTokenSpaceGuid.PcdSetupConOutColumn|368  # = 2944 / EFI_GLYPH_WIDTH(8)
gEfiMdeModulePkgTokenSpaceGuid.PcdSetupConOutRow|96     # = 1840 / EFI_GLYPH_HEIGHT(19)
gEfiMdeModulePkgTokenSpaceGuid.PcdConOutColumn|368       # = 2944 / EFI_GLYPH_WIDTH(8)
gEfiMdeModulePkgTokenSpaceGuid.PcdConOutRow|96          # = 1840 / EFI_GLYPH_HEIGHT(19)
