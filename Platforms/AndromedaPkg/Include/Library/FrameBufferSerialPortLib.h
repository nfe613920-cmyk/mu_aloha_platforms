/** @file

  Copyright (c) 2022-2024 DuoWoA authors

  SPDX-License-Identifier: MIT

**/
#ifndef _FRAMEBUFFER_SERIALPORT_LIB_H_
#define _FRAMEBUFFER_SERIALPORT_LIB_H_

#define FBCON_POSITION_MAGIC 0x4642504F53495421ULL /* "FBPOSIT!" */

typedef struct _FBCON_POSITION {
  UINT64 Magic;
  INTN x;
  INTN y;
} FBCON_POSITION, *PFBCON_POSITION;

typedef struct _FBCON_COLOR {
  UINTN Foreground;
  UINTN Background;
} FBCON_COLOR, *PFBCON_COLOR;

enum FbConMsgType {
  /* type for menu */
  FBCON_COMMON_MSG = 0,
  FBCON_UNLOCK_TITLE_MSG,
  FBCON_TITLE_MSG,
  FBCON_SUBTITLE_MSG,

  /* type for warning */
  FBCON_YELLOW_MSG,
  FBCON_ORANGE_MSG,
  FBCON_RED_MSG,
  FBCON_GREEN_MSG,

  /* and the select message's background */
  FBCON_SELECT_MSG_BG_COLOR,
};

#define UEFI_LOG_MAGIC 0x554546494C4F4721ULL /* "UEFILOG!" */
#define UEFI_LOG_MAX_SIZE (2 * 1024 * 1024)   /* 2 MB */

#pragma pack(push, 1)
typedef struct {
  UINT64 Magic;
  UINT32 MaxSize;
  UINT32 WriteIndex;
  CHAR8  Buffer[UEFI_LOG_MAX_SIZE];
} UEFI_IN_MEMORY_LOG;
#pragma pack(pop)

UEFI_IN_MEMORY_LOG *GetUefiLogBuffer(VOID);

void ResetFb(void);

UINTN
EFIAPI
SerialPortWriteCritical(IN UINT8 *Buffer, IN UINTN NumberOfBytes);

#endif