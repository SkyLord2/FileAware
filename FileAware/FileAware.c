#include <fltKernel.h>
#include "Communication.h"

#define FILE_AWARE_FILTER 0x01020060

PFLT_FILTER gFilterHandle = NULL;

NTSTATUS PtUnload(__in FLT_FILTER_UNLOAD_FLAGS Flags) {
	UNREFERENCED_PARAMETER(Flags);
	PAGED_CODE();

	DbgPrint("[%ws] driver unload begin\n", __FUNCTIONW__);
	
	DeleteFileAwarePort();

	if (gFilterHandle != NULL)
	{
		FltUnregisterFilter(gFilterHandle);
		gFilterHandle = NULL;
	}
	DbgPrint("[%ws] driver unload complete\n", __FUNCTIONW__);
	
	return STATUS_SUCCESS;
}

NTSTATUS IsNeededFileType(UNICODE_STRING extension) {
	UNICODE_STRING docExt  = RTL_CONSTANT_STRING(L"doc");
	UNICODE_STRING docxExt = RTL_CONSTANT_STRING(L"docx");
	UNICODE_STRING txtExt  = RTL_CONSTANT_STRING(L"txt");
	if (RtlEqualUnicodeString(&extension, &docExt,  TRUE) ||
		RtlEqualUnicodeString(&extension, &docxExt, TRUE) ||
		RtlEqualUnicodeString(&extension, &txtExt,  TRUE)
	) {
        return STATUS_SUCCESS;
	} else {
		return STATUS_UNSUCCESSFUL;
	}
}

NTSTATUS GetDriveLetterFromDeviceNameEx(
	__in PUNICODE_STRING DeviceName,
	__out PWCHAR DriveLetter
) {
	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING dosDriveName;
	WCHAR driveBuffer[16] = L"\\DosDevices\\ :";
	OBJECT_ATTRIBUTES objectAttributes;
	HANDLE handle = NULL;
	WCHAR targetBuffer[256] = { 0 };
	UNICODE_STRING targetName;

	PAGED_CODE();

	*DriveLetter = L'\0';

	for (WCHAR drive = L'C'; drive <= L'Z'; drive++) {
		// DOS name
		driveBuffer[12] = drive;  // \DosDevices\C:
		dosDriveName.Buffer = driveBuffer;
		dosDriveName.Length = 14 * sizeof(WCHAR);
		dosDriveName.MaximumLength = sizeof(driveBuffer);

		// 初始化对象属性
		InitializeObjectAttributes(
			&objectAttributes,
			&dosDriveName,
			OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
			NULL,
			NULL
		);

		// 打开符号链接
		status = ZwOpenSymbolicLinkObject(&handle, SYMBOLIC_LINK_QUERY, &objectAttributes);
		if (NT_SUCCESS(status)) {
			// 查询符号链接目标
			targetName.Buffer = targetBuffer;
			targetName.Length = 0;
			targetName.MaximumLength = sizeof(targetBuffer);

			status = ZwQuerySymbolicLinkObject(handle, &targetName, NULL);
			if (NT_SUCCESS(status)) {
				// 比较设备名称是否匹配
				if (RtlEqualUnicodeString(DeviceName, &targetName, TRUE)) {
					*DriveLetter = drive;
					ZwClose(handle);
					break;
				}
			}
			ZwClose(handle);
		}
	}

	return (*DriveLetter != L'\0') ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

NTSTATUS DoFileFilter(__in PFLT_CALLBACK_DATA Data) {
	PFLT_FILE_NAME_INFORMATION nameInfo;
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	WCHAR driveLetter = L'\0';
	// 首先检查 Create.Options 是否为 FILE_AWARE_FILTER
	ULONG Options = Data->Iopb->Parameters.Create.Options;
	// 如果不是目标值，直接返回
	if (Options != FILE_AWARE_FILTER) {
		return status;
	}
	PAGED_CODE();
	__try {
		status = FltGetFileNameInformation(
			Data,
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
			&nameInfo
		);
		if (NT_SUCCESS(status)) {
			FltParseFileNameInformation(nameInfo);
			status = IsNeededFileType(nameInfo->Extension);
			if (NT_SUCCESS(status)) {
				DbgPrint("[NPPreCreate] file open\n");
				status = GetDriveLetterFromDeviceNameEx(&nameInfo->Volume, &driveLetter);
				UNICODE_STRING DriverLetter = nameInfo->Volume;
				if (NT_SUCCESS(status))
				{
					RtlInitUnicodeString(&DriverLetter, &driveLetter);
					DbgPrint("[%ws] Opening file drive letter: %wZ:\n", __FUNCTIONW__, &DriverLetter);
				}
				else
				{
					DbgPrint("[%ws] Opening file Volume: %wZ\n", __FUNCTIONW__, &nameInfo->Volume);
				}
				DbgPrint("[%ws] Opening file name: %wZ\n", __FUNCTIONW__, &nameInfo->Name);
				DbgPrint("[%ws] Opening file extension: %wZ\n", __FUNCTIONW__, &nameInfo->Extension);
				DbgPrint("[%ws] Opening file path: %wZ\n", __FUNCTIONW__, &nameInfo->ParentDir);
				SendMessageToUserMode(&nameInfo->Name, &nameInfo->Extension, &DriverLetter, &nameInfo->ParentDir);
			}
			FltReleaseFileNameInformation(nameInfo);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		DbgPrint("NPPreCreate EXCEPTION_EXECUTE_HANDLER\n");
	}
	return status;
}

FLT_PREOP_CALLBACK_STATUS NPPreCreate(
	__inout PFLT_CALLBACK_DATA Data, 
	__in PCFLT_RELATED_OBJECTS FltObjects, 
	__deref_out_opt PVOID* CompletionContext
) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	PAGED_CODE();
	DoFileFilter(Data);
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS NPPostCreate(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
    __in_opt PVOID CompletionContext,
	__in FLT_POST_OPERATION_FLAGS Flags
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();
	DbgPrint("[%ws] post operation called\n", __FUNCTIONW__);
	return FLT_POSTOP_FINISHED_PROCESSING;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
	if (DriverObject != NULL)
	{
		DbgPrint("[%ws] Driver Unload, DriverObject: 0x%p\n", __FUNCTIONW__, DriverObject);
	}
	return;
}

NTSTATUS StartFileFilter(PDRIVER_OBJECT DriverObject) {
	PAGED_CODE();
	NTSTATUS status = STATUS_SUCCESS;
	CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
		{
			IRP_MJ_CREATE,
			0,
			NPPreCreate,
			NULL
		},
		{
			IRP_MJ_OPERATION_END
		}
	};

	CONST FLT_REGISTRATION FilterRegistration = {
		sizeof(FLT_REGISTRATION),
		FLT_REGISTRATION_VERSION,
		0,
		NULL,
		Callbacks,
		PtUnload,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};

	status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
	if (NT_SUCCESS(status)) {
		DbgPrint("[%ws] driver register success\n", __FUNCTIONW__);
		status = FltStartFiltering(gFilterHandle);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("[%ws] filter begin failed: 0x%X\n", __FUNCTIONW__, status);
			FltUnregisterFilter(gFilterHandle);
		}
		else {
			DbgPrint("[%ws] filter begin success\n", __FUNCTIONW__);
		}
	}
	else {
		DbgPrint("[%ws] driver register failed: 0x%X\n", __FUNCTIONW__, status);
	}
    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	//KdBreakPoint();
	PAGED_CODE();
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(RegistryPath);
	if (RegistryPath == NULL)
	{
		return status;
	}
	
	DbgPrint("[%ws] registry path: %wZ\n", __FUNCTIONW__, RegistryPath);

	if (DriverObject == NULL)
	{
		return status;
	}
	DbgPrint("[%ws] driver object address: %p\n", __FUNCTIONW__, DriverObject);
	DriverObject->DriverUnload = DriverUnload;

	status = StartFileFilter(DriverObject);

	if (NT_SUCCESS(status)) {
		status = CreateFileAwarePort(gFilterHandle);
	}
	
	return status;
}