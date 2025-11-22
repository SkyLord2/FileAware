#pragma once
#include <fltKernel.h>
#include <dontuse.h>

// 全局变量
typedef struct _FILEAWARE_GLOBALS {
	PFLT_FILTER FilterHandle;
	PFLT_PORT ServerPort;
	PFLT_PORT ClientPort;
	volatile LONG ClientConnected;
} FILEAWARE_GLOBALS, * PFILEAWARE_GLOBALS;

extern FILEAWARE_GLOBALS g_Globals;

NTSTATUS
SendMessageToUserMode(
	__in PUNICODE_STRING FileName,
	__in PUNICODE_STRING FileType,
	__in PUNICODE_STRING Volume,
	__in PUNICODE_STRING FilePath
);

NTSTATUS
FileAwareMessage(
	__in PVOID ConnectionCookie,
	__in_bcount_opt(InputBufferSize) PVOID InputBuffer,
	__in ULONG InputBufferSize,
	__out_bcount_part_opt(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
	__in ULONG OutputBufferSize,
	__out PULONG ReturnOutputBufferLength
);

NTSTATUS
FileAwareConnect(
	__in PFLT_PORT ClientPort,
	__in PVOID ServerPortCookie,
	__in_bcount(SizeOfContext) PVOID ConnectionContext,
	__in ULONG SizeOfContext,
	__deref_out_opt PVOID* ConnectionCookie
);

VOID
FileAwareDisconnect(
	__in_opt PVOID ConnectionCookie
);

NTSTATUS CreateFileAwarePort(PFLT_FILTER FilterHandle);

VOID DeleteFileAwarePort();