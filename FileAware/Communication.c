#include <ntstatus.h>
#include <ntstrsafe.h>
#include "Communication.h"
#include "ConnData.h"

FILEAWARE_GLOBALS g_Globals = { 0 };

NTSTATUS SendMessageToUserMode(
	__in PUNICODE_STRING FileName,
	__in PUNICODE_STRING FileType,
	__in PUNICODE_STRING Volume,
	__in PUNICODE_STRING FilePath
) {
	PAGED_CODE();

	// 参数检查
	if (!FileName || !FileType || !Volume || !FilePath) {
		DbgPrint("[SendMessageToUserMode] Parameter invalid\n");
		return STATUS_INVALID_PARAMETER;
	}

	// 安全检查
	if (!g_Globals.FilterHandle || !g_Globals.ServerPort) {
		DbgPrint("[SendMessageToUserMode] Device state wrong\n");
		return STATUS_INVALID_DEVICE_STATE;
	}

	if (!InterlockedCompareExchange(&g_Globals.ClientConnected, 0, 0)) {
		DbgPrint("[SendMessageToUserMode] Port disconnected\n");
		return STATUS_PORT_DISCONNECTED;
	}

	//FILEAWARE_REPLY reply = { 0 };
	LARGE_INTEGER SystemTime;
	KeQuerySystemTime(&SystemTime);
	// 文件信息填充
	FILEAWARE_MESSAGE message = { 0 };
	message.Type = 1;
	message.ProcessId = 0;
	message.ThreadId = 0;
	message.TimeStamp = SystemTime;
	RtlStringCchCopyW(
		message.FileName,
		sizeof(message.FileName) / sizeof(WCHAR),
		FileName->Buffer);
    RtlStringCchCopyW(
		message.FileType,
		sizeof(message.FileType)/ sizeof(WCHAR),
		FileType->Buffer);
    RtlStringCchCopyW(
		message.FilePath,
		sizeof(message.FilePath) / sizeof(WCHAR),
		FilePath->Buffer);
	RtlStringCchCopyW(
		message.Volume,
        sizeof(message.Volume) / sizeof(WCHAR),
        Volume->Buffer
	);

	DbgPrint("[SendMessageToUserMode] send message file name:%ws, file type:%ws, file path:%ws, Volume:%ws\n", 
		message.FileName, message.FileType, message.FilePath, message.Volume);
	
	LARGE_INTEGER timeout;
	//timeout.QuadPart = -1 * 1000 * 1000 * 10;
	timeout.QuadPart = 0;
	//ULONG ReplyLength = sizeof(reply);
	ULONG ReplyLength = 0;
	NTSTATUS status = FltSendMessage(
		g_Globals.FilterHandle,
		&g_Globals.ClientPort,
		&message,
		sizeof(message),
		NULL,
		&ReplyLength,
		&timeout);
	DbgPrint("[SendMessageToUserMode] FltSendMessage returned 0x%X\n", status);
	return status;
}

NTSTATUS FileAwareConnect(
	__in PFLT_PORT ClientPort, 
	__in PVOID ServerPortCookie, 
	__in_bcount(SizeOfContext) PVOID ConnectionContext, 
	__in ULONG SizeOfContext, 
	__deref_out_opt PVOID* ConnectionCookie) {
	PAGED_CODE();
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionCookie);

	InterlockedExchange(&g_Globals.ClientConnected, 1);
	g_Globals.ClientPort = ClientPort;
	//*ConnectionCookie = NULL;

	DbgPrint("[FileAwareConnect] Client connected\n");

	return STATUS_SUCCESS;
}

VOID FileAwareDisconnect(__in_opt PVOID ConnectionCookie) {
	PAGED_CODE();
	UNREFERENCED_PARAMETER(ConnectionCookie);
	InterlockedExchange(&g_Globals.ClientConnected, 0);
	if (g_Globals.ClientPort)
	{
		FltCloseClientPort(g_Globals.FilterHandle, &g_Globals.ClientPort);
		g_Globals.ClientPort = NULL;
	}
	DbgPrint("[FileAwareDisconnect] Client disconnected\n");
}

NTSTATUS FileAwareMessage(
	__in PVOID ConnectionCookie,
	__in_bcount_opt(InputBufferSize) PVOID InputBuffer,
	__in ULONG InputBufferSize,
	__out_bcount_part_opt(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
	__in ULONG OutputBufferSize,
	__out PULONG ReturnOutputBufferLength
) {
	PAGED_CODE();
	UNREFERENCED_PARAMETER(ConnectionCookie);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);

	if (OutputBufferSize < sizeof(FILEAWARE_REPLY)) {
		*ReturnOutputBufferLength = 0;
		return STATUS_BUFFER_TOO_SMALL;
	}

	PFILEAWARE_REPLY reply = (PFILEAWARE_REPLY)OutputBuffer;
	if (reply)
	{
		reply->Status = 0x12345678; // 简单的状态码
	}

	*ReturnOutputBufferLength = sizeof(FILEAWARE_REPLY);
	return STATUS_SUCCESS;
}

NTSTATUS CreateFileAwarePort(PFLT_FILTER FilterHandle) {
	PAGED_CODE();
	NTSTATUS status;
	PSECURITY_DESCRIPTOR sd;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING portName;
	g_Globals.FilterHandle = FilterHandle;
	RtlInitUnicodeString(&portName, FILEAWARE_PORT_NAME);
	status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[CreateFileAwarePort] FltBuildDefaultSecurityDescriptor failed with status 0x%X\n", status);
	}
	InitializeObjectAttributes(
		&oa,
		&portName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		sd
	);
	status = FltCreateCommunicationPort(
		g_Globals.FilterHandle,
		&g_Globals.ServerPort,
		&oa,
		NULL,
		FileAwareConnect,
		FileAwareDisconnect,
		FileAwareMessage,
		1);
	FltFreeSecurityDescriptor(sd);

	if (!NT_SUCCESS(status)) {
		DbgPrint("[CreateFileAwarePort] FltCreateCommunicationPort failed with status 0x%X\n", status);
	}
	return status;
}

VOID DeleteFileAwarePort() {
	if (g_Globals.ServerPort) {
		FltCloseCommunicationPort(g_Globals.ServerPort);
		g_Globals.ServerPort = NULL;
	}
}
