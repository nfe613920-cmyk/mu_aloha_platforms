#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FrameBufferSerialPortLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Guid/EventGroup.h>

STATIC VOID      *mFileSystemRegistration = NULL;
STATIC EFI_EVENT mTimerEvent              = NULL;
STATIC EFI_EVENT mReadyToBootEvent        = NULL;

STATIC BOOLEAN   mInFlush                 = FALSE;

STATIC VOID *mBlockIoRegistration    = NULL;

STATIC
VOID
ConnectAllControllers (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       HandleCount = 0;
  EFI_HANDLE  *Handles    = NULL;
  UINTN       Index;

  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (!EFI_ERROR (Status) && (Handles != NULL)) {
    for (Index = 0; Index < HandleCount; Index++) {
      gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
    }
    FreePool (Handles);
  }
}

STATIC UINTN mLastFlushedIndex = 0;

STATIC
BOOLEAN
FlushLogToFilesystem (
  IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *Fs,
  IN UEFI_IN_MEMORY_LOG               *LogBuffer,
  IN BOOLEAN                          IsFirstBootFlush
  )
{
  EFI_STATUS          Status;
  EFI_FILE_PROTOCOL   *Root      = NULL;
  EFI_FILE_PROTOCOL   *File      = NULL;
  BOOLEAN             WroteAny   = FALSE;

  Status = Fs->OpenVolume (Fs, &Root);
  if (EFI_ERROR (Status) || (Root == NULL)) {
    return FALSE;
  }

  // If first flush of this boot session, remove stale old log files
  if (IsFirstBootFlush) {
    Status = Root->Open (Root, &File, L"uefi_boot.log", EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (!EFI_ERROR (Status) && (File != NULL)) {
      File->Delete (File);
      File = NULL;
    }
    Status = Root->Open (Root, &File, L"uefi.log", EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (!EFI_ERROR (Status) && (File != NULL)) {
      File->Delete (File);
      File = NULL;
    }
  }

  // 1. Write to uefi_boot.log
  Status = Root->Open (
                   Root,
                   &File,
                   L"uefi_boot.log",
                   EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                   0
                   );
  if (!EFI_ERROR (Status) && (File != NULL)) {
    UINTN  WriteSize = LogBuffer->WriteIndex;
    File->SetPosition (File, 0);
    Status = File->Write (File, &WriteSize, LogBuffer->Buffer);
    if (!EFI_ERROR (Status)) {
      File->Flush (File);
      WroteAny = TRUE;
    }
    File->Close (File);
  }

  // 2. Also write to uefi.log
  Status = Root->Open (
                   Root,
                   &File,
                   L"uefi.log",
                   EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                   0
                   );
  if (!EFI_ERROR (Status) && (File != NULL)) {
    UINTN  WriteSize = LogBuffer->WriteIndex;
    File->SetPosition (File, 0);
    Status = File->Write (File, &WriteSize, LogBuffer->Buffer);
    if (!EFI_ERROR (Status)) {
      File->Flush (File);
      WroteAny = TRUE;
    }
    File->Close (File);
  }

  Root->Close (Root);
  return WroteAny;
}

VOID
DumpAllLogs (
  VOID
  )
{
  EFI_STATUS                       Status;
  UINTN                            HandleCount = 0;
  EFI_HANDLE                       *Handles    = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *Fs         = NULL;
  UINTN                            i;
  UEFI_IN_MEMORY_LOG               *LogBuffer;
  BOOLEAN                          WroteAtLeastOne = FALSE;
  BOOLEAN                          IsFirstBootFlush;

  if (mInFlush) {
    return;
  }
  mInFlush = TRUE;

  LogBuffer = GetUefiLogBuffer ();
  if ((LogBuffer == NULL) || (LogBuffer->Magic != UEFI_LOG_MAGIC) || (LogBuffer->WriteIndex == 0)) {
    mInFlush = FALSE;
    return;
  }

  if (LogBuffer->WriteIndex == mLastFlushedIndex) {
    mInFlush = FALSE;
    return;
  }

  // Proactively connect block devices so PartitionDxe and Fat bind to them
  ConnectAllControllers ();

  IsFirstBootFlush = (mLastFlushedIndex == 0);

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (!EFI_ERROR (Status) && (Handles != NULL) && (HandleCount > 0)) {
    for (i = 0; i < HandleCount; i++) {
      Fs     = NULL;
      Status = gBS->HandleProtocol (Handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
      if (!EFI_ERROR (Status) && (Fs != NULL)) {
        if (FlushLogToFilesystem (Fs, LogBuffer, IsFirstBootFlush)) {
          WroteAtLeastOne = TRUE;
        }
      }
    }

    FreePool (Handles);
  }

  if (WroteAtLeastOne) {
    mLastFlushedIndex = LogBuffer->WriteIndex;
  }

  mInFlush = FALSE;
}

STATIC
VOID
EFIAPI
OnFileSystemNotification (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DumpAllLogs ();
}

STATIC
VOID
EFIAPI
OnTimerTick (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DumpAllLogs ();
}

STATIC
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DumpAllLogs ();
}

STATIC
VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DumpAllLogs ();
}

EFI_STATUS
EFIAPI
UefiLogDumpDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   FsEvent;
  EFI_EVENT   ExitEvent;

  // 1. Flush any already mounted filesystems
  DumpAllLogs ();

  // 2. Register notification for any new SimpleFileSystem (e.g. logfs / boot_b mounted later)
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnFileSystemNotification,
                  NULL,
                  &FsEvent
                  );
  if (!EFI_ERROR (Status)) {
    gBS->RegisterProtocolNotify (
           &gEfiSimpleFileSystemProtocolGuid,
           FsEvent,
           &mFileSystemRegistration
           );
  }

  // 2.5 Register notification for any new BlockIo devices (UFS / USB)
  {
    EFI_EVENT BlockIoEvent;
    Status = gBS->CreateEvent (
                    EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    OnFileSystemNotification,
                    NULL,
                    &BlockIoEvent
                    );
    if (!EFI_ERROR (Status)) {
      gBS->RegisterProtocolNotify (
             &gEfiBlockIoProtocolGuid,
             BlockIoEvent,
             &mBlockIoRegistration
             );
    }
  }

  // 3. Register ReadyToBoot notification
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnReadyToBoot,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &mReadyToBootEvent
                  );

  // 3.5 Register ExitBootServices notification
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnExitBootServices,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &ExitEvent
                  );

  // 4. Create a recurring 100ms timer matching log scrolling rhythm
  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnTimerTick,
                  NULL,
                  &mTimerEvent
                  );
  if (!EFI_ERROR (Status)) {
    gBS->SetTimer (mTimerEvent, TimerPeriodic, 1000000);   // Every 100ms (matches log scroll rhythm)
  }

  return EFI_SUCCESS;
}
