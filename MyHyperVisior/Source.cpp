#include <intrin.h>
#include "cpuid.h"
#include <ntddk.h>



int VmHasCpuidSupport(void)
{

	union __cpuid_t cpuid = { 0 };

	DbgPrint("Inside %s .\n", __FUNCTION__);
	__cpuid(cpuid.cpu_info, 1);
	return cpuid.feature_ecx.virtual_machine_extensions;
}




void SampleUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);
	DbgPrint("%s - Unloaded ReHV.\n\n", __FUNCTION__);
}

extern "C" //Added to prevent linker error. The DriverEntry function must have C-linkage, which is not the default in C++ compilation.
NTSTATUS
DriverEntry(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath
)
{
	OSVERSIONINFOEXW osVersionInfo;
	NTSTATUS status = STATUS_SUCCESS;

	//UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = SampleUnload;//prevent a leak in kernel
	DbgPrint("%s - Output Test from ReHV.\n", __FUNCTION__);

	osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	status = RtlGetVersion((POSVERSIONINFOW)& osVersionInfo);


	DbgPrint("%d Mj version.!", osVersionInfo.dwMajorVersion);
	DbgPrint("%d Min version.!", osVersionInfo.dwMinorVersion);
	DbgPrint("%d Build Number.!", osVersionInfo.dwBuildNumber);
	DbgPrint("%d VMX support.", VmHasCpuidSupport());
	//VmHasCpuidSupport
	return STATUS_SUCCESS;
}





