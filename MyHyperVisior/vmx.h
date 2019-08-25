#pragma once
#pragma warning(disable:4201)
#pragma warning(disable:4214)

//added from tutorial
#define IA32_VMX_BASIC          0x480
#define IA32_VMX_CR0_FIXED0     0x486
#define IA32_VMX_CR0_FIXED1     0x487
#define IA32_VMX_CR4_FIXED0     0x488
#define IA32_VMX_CR4_FIXED1     0x489
#define VMM_STACK_SIZE			0x6000
#define VMM_TAG					"REvH"



struct __vmcs_t
{
	union
	{
		unsigned int all;
		struct
		{
			unsigned int revision_identifier : 31;
			unsigned int shadow_vmcs_indicator : 1;
		} bits;
	} header;
	unsigned int abort_indicator;
	char data[0x1000 - 2 * sizeof(unsigned)];
};

struct __vcpu_t
{
	//vmexit_status_t status;
	unsigned __int64 guest_rsp;
	unsigned __int64 guest_rip;
	struct __vmcs_t* vmcs;
	unsigned __int64 vmcs_physical;
	struct __vmcs_t* vmxon;
	unsigned __int64 vmxon_physical;
	//__declspec(align(4096)) struct __vmm_stack_t vmm_stack;
};

struct __vmm_context_t
{
	unsigned long processor_count;
	__declspec(align(4096)) struct __vcpu_t** vcpu_table;
	__declspec(align(4096)) void* msr_bitmap;
};

struct __guest_registers_t
{
	__m128 xmm[6];
	void* padding;
	unsigned __int64 r15;
	unsigned __int64 r14;
	unsigned __int64 r13;
	unsigned __int64 r12;
	unsigned __int64 r11;
	unsigned __int64 r10;
	unsigned __int64 r9;
	unsigned __int64 r8;
	unsigned __int64 rdi;
	unsigned __int64 rsi;
	unsigned __int64 rbp;
	unsigned __int64 rbx;
	unsigned __int64 rdx;
	unsigned __int64 rcx;
	unsigned __int64 rax;
};

struct __vmexit_stack_t
{
	struct __guest_registers_t guest_registers;
	struct __vmm_context_t vmm_context;
};

struct __ext_registers_t
{
	unsigned __int64 rip;
	unsigned __int64 rsp;
	union __rflags_t rflags;
};