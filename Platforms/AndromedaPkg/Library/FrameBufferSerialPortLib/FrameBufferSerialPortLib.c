/** @file

  Copyright (c) 2022-2024 DuoWoA authors

  SPDX-License-Identifier: MIT

**/
#include <PiDxe.h>

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/SerialPortLib.h>

#include <Resources/FbColor.h>
#include <Resources/font5x12.h>

#include <Library/FrameBufferSerialPortLib.h>

ARM_MEMORY_REGION_DESCRIPTOR_EX DisplayMemoryRegion;

FBCON_POSITION* p_Position = NULL;
FBCON_POSITION m_MaxPosition;
FBCON_COLOR    m_Color;
BOOLEAN        m_Initialized = FALSE;
STATIC UEFI_IN_MEMORY_LOG *m_LogBuffer = NULL;

UINTN gWidth = FixedPcdGet32(PcdMipiFrameBufferWidth);
// Reserve half screen for output
UINTN gHeight = FixedPcdGet32(PcdMipiFrameBufferHeight);
UINTN gBpp    = FixedPcdGet32(PcdMipiFrameBufferPixelBpp);

// Module-used internal routine
void FbConPutCharWithFactor(char c, int type, unsigned scale_factor);

void FbConDrawglyph(
    char *pixels, unsigned stride, unsigned bpp, unsigned *glyph,
    unsigned scale_factor);

void FbConReset(void);
void FbConScrollUp(void);
void FbConFlush(void);

UEFI_IN_MEMORY_LOG *GetUefiLogBuffer(VOID)
{
  if (m_LogBuffer != NULL && m_LogBuffer->Magic == UEFI_LOG_MAGIC) {
    return m_LogBuffer;
  }
  ARM_MEMORY_REGION_DESCRIPTOR_EX DisplayMem;
  EFI_STATUS Status = LocateMemoryMapAreaByName("Display Reserved", &DisplayMem);
  if (!EFI_ERROR(Status)) {
    UEFI_IN_MEMORY_LOG *Log = (UEFI_IN_MEMORY_LOG *)(DisplayMem.Address + 
      (FixedPcdGet32(PcdMipiFrameBufferWidth) * FixedPcdGet32(PcdMipiFrameBufferHeight) * FixedPcdGet32(PcdMipiFrameBufferPixelBpp) / 8) + 64);
    if (Log->Magic == UEFI_LOG_MAGIC) {
      m_LogBuffer = Log;
      return Log;
    }
  }
  return NULL;
}

RETURN_STATUS
EFIAPI
SerialPortInitialize(VOID)
{
  // Prevent dup initialization
  if (m_Initialized)
    return RETURN_SUCCESS;

  EFI_STATUS Status = LocateMemoryMapAreaByName("Display Reserved", &DisplayMemoryRegion);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  
  p_Position = (FBCON_POSITION*)(DisplayMemoryRegion.Address + (FixedPcdGet32(PcdMipiFrameBufferWidth) * FixedPcdGet32(PcdMipiFrameBufferHeight) * FixedPcdGet32(PcdMipiFrameBufferPixelBpp) / 8));

  m_LogBuffer = (UEFI_IN_MEMORY_LOG *)(DisplayMemoryRegion.Address + (FixedPcdGet32(PcdMipiFrameBufferWidth) * FixedPcdGet32(PcdMipiFrameBufferHeight) * FixedPcdGet32(PcdMipiFrameBufferPixelBpp) / 8) + 64);
  if (m_LogBuffer->Magic != UEFI_LOG_MAGIC) {
    m_LogBuffer->Magic = UEFI_LOG_MAGIC;
    m_LogBuffer->MaxSize = UEFI_LOG_MAX_SIZE;
    m_LogBuffer->WriteIndex = 0;
    m_LogBuffer->Buffer[0] = '\0';
  }

  // Reset console
  FbConReset();

  // Set flag
  m_Initialized = TRUE;

  return RETURN_SUCCESS;
}

void ResetFb(void)
{
  // Preserve screen contents - do not clear to black!
}

void FbConReset(void)
{
  // Calc max position.
  m_MaxPosition.x = gWidth / (FONT_WIDTH + 1);
  m_MaxPosition.y = (gHeight / (FONT_HEIGHT * SCALE_FACTOR)) * SCALE_FACTOR;

  // Reset color.
  m_Color.Foreground = FB_BGRA8888_WHITE;
  m_Color.Background = FB_BGRA8888_BLACK;

  // Validate or initialize position in shared memory
  if (p_Position != NULL) {
    if (p_Position->Magic != FBCON_POSITION_MAGIC) {
      p_Position->Magic = FBCON_POSITION_MAGIC;
      p_Position->x = 0;
      p_Position->y = 0;
    } else {
      if (p_Position->x < 0 || p_Position->x >= (INTN)(m_MaxPosition.x / SCALE_FACTOR)) {
        p_Position->x = 0;
      }
      if (p_Position->y < 0 || p_Position->y + SCALE_FACTOR > m_MaxPosition.y) {
        p_Position->y = 0;
      }
    }
  }
}

STATIC BOOLEAN m_FatalCrashMode = FALSE;

STATIC BOOLEAN StrHasSubstr(CONST CHAR8 *Str, UINTN Len, CONST CHAR8 *Sub, UINTN SubLen)
{
  if (Len < SubLen) return FALSE;
  for (UINTN i = 0; i <= Len - SubLen; i++) {
    UINTN j;
    for (j = 0; j < SubLen; j++) {
      if (Str[i + j] != Sub[j]) break;
    }
    if (j == SubLen) return TRUE;
  }
  return FALSE;
}


STATIC VOID FbConNewLine(VOID)
{
  p_Position->x = 0;
  p_Position->y += SCALE_FACTOR;

  if (p_Position->y + SCALE_FACTOR > m_MaxPosition.y) {
    p_Position->y = 0;
  }

  // Clear this line so old text does not overlap with new text
  if (DisplayMemoryRegion.Address != 0) {
    UINTN LineStride = (UINTN)gWidth * (gBpp / 8);
    UINTN LineBytes  = LineStride * FONT_HEIGHT * SCALE_FACTOR;
    UINT8 *LineDst   = (UINT8 *)DisplayMemoryRegion.Address + (p_Position->y * LineStride * FONT_HEIGHT);
    ZeroMem(LineDst, LineBytes);
  }
}

void FbConPutCharWithFactor(char c, int type, unsigned scale_factor)
{
  char *Pixels;
  BOOLEAN intstate;

  if ((unsigned char)c > 127)
    return;

  if (c == '\r') {
    p_Position->x = 0;
    return;
  }

  if (c == '\n') {
    intstate = ArmGetInterruptState();
    if (intstate)
      ArmDisableInterrupts();

    FbConNewLine();

    if (intstate)
      ArmEnableInterrupts();
    return;
  }

  if ((unsigned char)c < 32)
    return;

  // Save some space
  if (p_Position->x == 0 && (unsigned char)c == ' ' &&
      type != FBCON_SUBTITLE_MSG && type != FBCON_TITLE_MSG)
    return;

  intstate = ArmGetInterruptState();
  if (intstate)
    ArmDisableInterrupts();

  Pixels = (void *)DisplayMemoryRegion.Address;
  Pixels += p_Position->y * ((gBpp / 8) * FONT_HEIGHT * gWidth);
  Pixels += p_Position->x * scale_factor * ((gBpp / 8) * (FONT_WIDTH + 1));

  FbConDrawglyph(
      Pixels, gWidth, (gBpp / 8), font5x12 + (c - 32) * 2, scale_factor);

  p_Position->x++;

  if (p_Position->x >= (int)(m_MaxPosition.x / scale_factor)) {
    FbConNewLine();
  }

  if (intstate)
    ArmEnableInterrupts();
}

void FbConDrawglyph(
    char *pixels, unsigned stride, unsigned bpp, unsigned *glyph,
    unsigned scale_factor)
{
  char *       bg_pixels = pixels;
  unsigned     x, y, i, j, k;
  unsigned     data, temp;
  unsigned int fg_color = m_Color.Foreground;
  unsigned int bg_color = m_Color.Background;
  stride -= FONT_WIDTH * scale_factor;

  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    for (i = 0; i < scale_factor; i++) {
      for (x = 0; x < FONT_WIDTH; ++x) {
        for (j = 0; j < scale_factor; j++) {
          bg_color = m_Color.Background;
          for (k = 0; k < bpp; k++) {
            *bg_pixels = (unsigned char)bg_color;
            bg_color   = bg_color >> 8;
            bg_pixels++;
          }
        }
      }
      bg_pixels += (stride * bpp);
    }
  }

  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    for (i = 0; i < scale_factor; i++) {
      for (x = 0; x < FONT_WIDTH; ++x) {
        for (j = 0; j < scale_factor; j++) {
          bg_color = m_Color.Background;
          for (k = 0; k < bpp; k++) {
            *bg_pixels = (unsigned char)bg_color;
            bg_color   = bg_color >> 8;
            bg_pixels++;
          }
        }
      }
      bg_pixels += (stride * bpp);
    }
  }

  data = glyph[0];
  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    temp = data;
    for (i = 0; i < scale_factor; i++) {
      data = temp;
      for (x = 0; x < FONT_WIDTH; ++x) {
        if (data & 1) {
          for (j = 0; j < scale_factor; j++) {
            fg_color = m_Color.Foreground;
            for (k = 0; k < bpp; k++) {
              *pixels  = (unsigned char)fg_color;
              fg_color = fg_color >> 8;
              pixels++;
            }
          }
        }
        else {
          for (j = 0; j < scale_factor; j++) {
            pixels = pixels + bpp;
          }
        }
        data >>= 1;
      }
      pixels += (stride * bpp);
    }
  }

  data = glyph[1];
  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    temp = data;
    for (i = 0; i < scale_factor; i++) {
      data = temp;
      for (x = 0; x < FONT_WIDTH; ++x) {
        if (data & 1) {
          for (j = 0; j < scale_factor; j++) {
            fg_color = m_Color.Foreground;
            for (k = 0; k < bpp; k++) {
              *pixels  = (unsigned char)fg_color;
              fg_color = fg_color >> 8;
              pixels++;
            }
          }
        }
        else {
          for (j = 0; j < scale_factor; j++) {
            pixels = pixels + bpp;
          }
        }
        data >>= 1;
      }
      pixels += (stride * bpp);
    }
  }
}

void FbConScrollUp(void)
{
  UINT8  *dst         = (UINT8 *)DisplayMemoryRegion.Address;
  UINT32  line_height = FONT_HEIGHT * SCALE_FACTOR;
  UINTN   line_bytes  = (UINTN)gWidth * line_height * (gBpp / 8);
  UINTN   total_bytes = (UINTN)gWidth * gHeight * (gBpp / 8);
  UINT8  *src         = dst + line_bytes;
  UINTN   scroll_size = total_bytes - line_bytes;

  if (DisplayMemoryRegion.Address == 0) {
    return;
  }

  CopyMem (dst, src, scroll_size);
  ZeroMem (dst + scroll_size, line_bytes);
  FbConFlush();
}

void FbConFlush(void)
{
  unsigned total_x, total_y;
  unsigned bytes_per_bpp;

  total_x       = gWidth;
  total_y       = gHeight;
  bytes_per_bpp = (gBpp / 8);

  WriteBackInvalidateDataCacheRange(
      (void *)DisplayMemoryRegion.Address,
      (total_x * total_y * bytes_per_bpp));
}

STATIC VOID RecordToLogBuffer(IN CONST UINT8 *Buffer, IN UINTN NumberOfBytes)
{
  if (m_LogBuffer == NULL) {
    GetUefiLogBuffer();
  }
  if (m_LogBuffer != NULL && m_LogBuffer->Magic == UEFI_LOG_MAGIC) {
    UINT32 Space = (m_LogBuffer->MaxSize > m_LogBuffer->WriteIndex + 1) ? 
                   (m_LogBuffer->MaxSize - m_LogBuffer->WriteIndex - 1) : 0;
    UINT32 ToCopy = (NumberOfBytes < Space) ? (UINT32)NumberOfBytes : Space;
    if (ToCopy > 0) {
      CopyMem(&m_LogBuffer->Buffer[m_LogBuffer->WriteIndex], Buffer, ToCopy);
      m_LogBuffer->WriteIndex += ToCopy;
      m_LogBuffer->Buffer[m_LogBuffer->WriteIndex] = '\0';
    }
  }
}

UINTN
EFIAPI
SerialPortWrite(IN UINT8 *Buffer, IN UINTN NumberOfBytes)
{
  UINT8 *CONST Final          = &Buffer[NumberOfBytes];
  UINTN        InterruptState = ArmGetInterruptState();

  if (InterruptState)
    ArmDisableInterrupts();

  RecordToLogBuffer(Buffer, NumberOfBytes);

  if (StrHasSubstr((CONST CHAR8 *)Buffer, NumberOfBytes, "CRITICAL", 8) ||
      StrHasSubstr((CONST CHAR8 *)Buffer, NumberOfBytes, "ASSERT", 6) ||
      StrHasSubstr((CONST CHAR8 *)Buffer, NumberOfBytes, "EXCEPTION", 9) ||
      StrHasSubstr((CONST CHAR8 *)Buffer, NumberOfBytes, "Exception", 9) ||
      StrHasSubstr((CONST CHAR8 *)Buffer, NumberOfBytes, "FATAL", 5) ||
      StrHasSubstr((CONST CHAR8 *)Buffer, NumberOfBytes, "DEADLOOP", 8)) {
    m_FatalCrashMode = TRUE;
    m_Color.Foreground = FB_BGRA8888_YELLOW;
  }

  while (Buffer < Final) {
    FbConPutCharWithFactor(*Buffer++, FBCON_COMMON_MSG, SCALE_FACTOR);
  }

  FbConFlush();

  if (InterruptState)
    ArmEnableInterrupts();
  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortWriteCritical(IN UINT8 *Buffer, IN UINTN NumberOfBytes)
{
  UINT8 *CONST Final             = &Buffer[NumberOfBytes];
  UINTN        CurrentForeground = m_Color.Foreground;
  UINTN        InterruptState    = ArmGetInterruptState();

  if (InterruptState)
    ArmDisableInterrupts();

  RecordToLogBuffer(Buffer, NumberOfBytes);

  m_Color.Foreground = FB_BGRA8888_YELLOW;

  while (Buffer < Final) {
    FbConPutCharWithFactor(*Buffer++, FBCON_COMMON_MSG, SCALE_FACTOR);
  }

  m_Color.Foreground = CurrentForeground;
  FbConFlush();

  if (InterruptState)
    ArmEnableInterrupts();
  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortRead(OUT UINT8 *Buffer, IN UINTN NumberOfBytes) { return 0; }

BOOLEAN
EFIAPI
SerialPortPoll(VOID) { return FALSE; }

RETURN_STATUS
EFIAPI
SerialPortSetControl(IN UINT32 Control) { return RETURN_UNSUPPORTED; }

RETURN_STATUS
EFIAPI
SerialPortGetControl(OUT UINT32 *Control) { return RETURN_UNSUPPORTED; }

RETURN_STATUS
EFIAPI
SerialPortSetAttributes(
    IN OUT UINT64 *BaudRate, IN OUT UINT32 *ReceiveFifoDepth,
    IN OUT UINT32 *Timeout, IN OUT EFI_PARITY_TYPE *Parity,
    IN OUT UINT8 *DataBits, IN OUT EFI_STOP_BITS_TYPE *StopBits)
{
  return RETURN_UNSUPPORTED;
}

UINTN SerialPortFlush(VOID) { return 0; }

VOID EnableSynchronousSerialPortIO(VOID)
{
  // Already synchronous
}