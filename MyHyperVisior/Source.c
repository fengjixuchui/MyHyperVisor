#include <intrin.h>
#include <ntddk.h>
#include "cpuid.h"
#include "cr.h"
#include "msr.h"
#include "dr.h"
#include "rflags.h"
#include "vmx.h"

/*
void log_error(char* msg, int vmm_context)
{
	DbgPrint("ERROR::\t%s", msg, vmm_context);
}

void log_success(char* msg, int vmm_context)
{
	DbgPrint("SUCCESS::\t%s", msg, vmm_context);
}

void log_debug(char* msg, int vmm_context)
{
	DbgPrint("DEBUG::\t%s", msg, vmm_context);
}
*/


void disable_vmx(void)
{
	__vmx_off();
	DbgPrint("VMM OFF!.\n");
}

struct __vmm_context_t* allocate_vmm_context(void)
{
	struct __vmm_context_t* vmm_context = NULL;
	vmm_context = (struct __vmm_context_t*)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct __vmm_context_t), VMM_TAG);
	if (vmm_context == NULL) {
		DbgPrint("Oops! vmm_context could not be allocated.\n");
		return NULL;
	}
	vmm_context->processor_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	vmm_context->vcpu_table = ExAllocatePoolWithTag(NonPagedPool, sizeof(struct __vcpu_t*) * vmm_context->processor_count, VMM_TAG);
	//
	// Allocate stack for vm-exit handlers and fill it with garbage
	// data.
	//
	//vmm_context->stack = ExAllocatePoolWithTag(NonPagedPool, VMM_STACK_SIZE, VMM_TAG);
	//memset(vmm_context->stack, 0xCC, VMM_STACK_SIZE);
	DbgPrint("vmm_context allocated at %llX\n", vmm_context);
	DbgPrint("vcpu_table allocated at %llX\n", vmm_context->vcpu_table);
	//DbgPrint("vmm stack allocated at %llX\n", vmm_context->stack);
	return vmm_context;
}

struct __vcpu_t* init_vcpu(void)
{
	struct __vcpu_t* vcpu = NULL;
	vcpu = ExAllocatePoolWithTag(NonPagedPool, sizeof(struct __vcpu_t), VMM_TAG);
	if (!vcpu) {
		DbgPrint("Oops! vcpu could not be allocated.\n");
		return NULL;
	}
	RtlSecureZeroMemory(vcpu, sizeof(struct __vcpu_t));
	return vcpu;
}

int init_vmcs(struct __vcpu_t* vcpu, void* guest_rsp, void (*guest_rip)(), int is_pt_allowed)
{
	struct __vmcs_t* vmcs;
	union __vmx_basic_msr_t vmx_basic = { 0 };
	PHYSICAL_ADDRESS physical_max;
	vmx_basic.control = __readmsr(IA32_VMX_BASIC);
	physical_max.QuadPart = ~0ULL;
	vcpu->vmcs = MmAllocateContiguousMemory(PAGE_SIZE, physical_max);
	vcpu->vmcs_physical = MmGetPhysicalAddress(vcpu->vmcs).QuadPart;
	RtlSecureZeroMemory(vcpu->vmcs, PAGE_SIZE);
	vmcs = vcpu->vmcs;
	vmcs->header.all = vmx_basic.bits.vmcs_revision_identifier;
	vmcs->header.bits.shadow_vmcs_indicator = 0;
	return TRUE;
}

int init_vmxon(struct __vcpu_t* vcpu)
{
	union __vmx_basic_msr_t vmx_basic = { 0 };
	struct __vmcs_t* vmxon;
	PHYSICAL_ADDRESS physical_max;
	if (!vcpu) {
		DbgPrint("VMXON region could not be initialized. vcpu was null.\n");
		return FALSE;
	}
	vmx_basic.control = __readmsr(IA32_VMX_BASIC);
	physical_max.QuadPart = ~0ULL;
	if (vmx_basic.bits.vmxon_region_size > PAGE_SIZE) {
		vcpu->vmxon = MmAllocateContiguousMemory(PAGE_SIZE, physical_max);
	}
	else {
		vcpu->vmxon = MmAllocateContiguousMemory(vmx_basic.bits.vmxon_region_size, physical_max);
	}
	vcpu->vmxon_physical = MmGetPhysicalAddress(vcpu->vmxon).QuadPart;
	vmxon = vcpu->vmxon;
	RtlSecureZeroMemory(vmxon, PAGE_SIZE);
	vmxon->header.all = vmx_basic.bits.vmcs_revision_identifier;
	/*log_debug("VMXON for vcpu %d initialized:\n\t-> VA: %llX\n\t-> PA: %llX\n\t-> REV: %X\n",
		KeGetCurrentProcessorNumber(),
		vcpu->vmxon,
		vcpu->vmxon_physical,
		vcpu->vmxon->header.all);*/

	DbgPrint("VMXON for vcpu %d initialized:\n\t-> VA: %llX\n\t-> PA: %llX\n\t-> REV: %X\n",
		KeGetCurrentProcessorNumber(),
		vcpu->vmxon,
		vcpu->vmxon_physical,
		vcpu->vmxon->header.all);
	return TRUE;
}

int adjust_control_registers(void)
{
	//setting the VMX enable bit in CR4.
	union __cr4_t cr4 = { 0 };
	union __ia32_feature_control_msr_t feature_msr = { 0 };
	cr4.control = __readcr4();
	cr4.bits.vmx_enable = 1;
	__writecr4(cr4.control);
	//enable the use of VMXON in general operations (inside and outside SMX.) “Safer Mode Extensions (‬aka SMX‭)
	feature_msr.control = __readmsr(IA32_FEATURE_CONTROL_MSR);

	if (feature_msr.bits.lock == 0) {// VMX enable only if lock and vmxon_outside_smx are enabled so check if lock bit is disabled then reenable later
		feature_msr.bits.vmxon_outside_smx = 1;//set this bit to allow for vmxon to be used outside of safer mode extension operation
		feature_msr.bits.lock = 1;

		__writemsr(IA32_FEATURE_CONTROL_MSR, feature_msr.control);
		return TRUE;
	}
	return FALSE;
}//

int is_vmx_supported(void)
{

	union __cpuid_t cpuid = { 0 };

	DbgPrint("Inside %s .\n", __FUNCTION__);
	__cpuid(cpuid.cpu_info, 1);
	return cpuid.feature_ecx.virtual_machine_extensions;
}

void init_logical_processor( struct __vmm_context_t *context, void *guest_rsp )
{
  struct __vmm_context_t *vmm_context;
  struct __vcpu_t *vcpu;
  union __vmx_misc_msr_t vmx_misc;
  unsigned long processor_number;
  processor_number = KeGetCurrentProcessorNumber( );
  vmm_context = ( struct __vmm_context_t * )context;
  vcpu = vmm_context->vcpu_table[ processor_number ];
  DbgPrint( "vcpu %d guest_rsp = %llX\n", processor_number, guest_rsp );
  adjust_control_registers( );
  if( !is_vmx_supported( ) ) {
    DbgPrint( "VMX operation is not supported on this processor.\n" );
    //free_vmm_context( vmm_context );
    return;
  }
  if( !init_vmxon( vcpu ) ) {
   DbgPrint( "VMXON failed to initialize for vcpu %d.\n", processor_number );
    //free_vcpu( vcpu );
    disable_vmx( );
    return;
  }
  if( __vmx_on( &vcpu->vmxon_physical ) != 0 ) {
    DbgPrint( "Failed to put vcpu %d into VMX operation.\n", KeGetCurrentProcessorNumber( ) );
    //free_vcpu( vcpu );
    //disable_vmx( );
    //free_vmm_context( vmm_context );
	return;
  }
  DbgPrint( "vcpu %d is now in VMX operation.\n", KeGetCurrentProcessorNumber( ) );
}

int vmm_init(void)
{
	struct __vmm_context_t* vmm_context;
	vmm_context = allocate_vmm_context();
	for (unsigned iter = 0; iter < vmm_context->processor_count; iter++) {
		vmm_context->vcpu_table[iter] = init_vcpu();
		//vmm_context->vcpu_table[iter]->vmm_context = vmm_context;
	}
	init_logical_processor(vmm_context, 0);
	return TRUE;
}




void SampleUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);
	DbgPrint("%s - Unloaded ReHV.\n\n", __FUNCTION__);
	disable_vmx();
}

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
	vmm_init();
	return STATUS_SUCCESS;
}





