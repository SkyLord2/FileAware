#pragma once
#include <ntdef.h>
#define DRIVER_TAG 'nmFD'

// 通信相关定义
#define FILEAWARE_PORT_NAME L"\\FileMonitorPort"

typedef struct _FILEAWARE_MESSAGE {
	ULONG Type;                // 消息类型
	ULONG ProcessId;           // 进程ID
	ULONG ThreadId;            // 线程ID
	WCHAR FileName[256];       // 文件名
	WCHAR FilePath[512];       // 文件路径
	WCHAR FileType[64];        // 文件类型
	WCHAR Volume[64];          // 文件所在卷
	LARGE_INTEGER TimeStamp;   // 时间戳
} FILEAWARE_MESSAGE, * PFILEAWARE_MESSAGE;

typedef struct _FILEAWARE_REPLY {
	ULONG Status;
} FILEAWARE_REPLY, * PFILEAWARE_REPLY;