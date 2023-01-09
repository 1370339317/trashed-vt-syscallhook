#include <ntifs.h>
#include <ntddk.h>
#include <intrin.h>
#include "Struct.h"
#include "vmx.h"
#include "vm_exit_handler.h"
#include "SSDTHook.h"

//��֤�Ƿ�֧��VMX
int VmHasCpuidSupport(void) {

	_cpuinfo cpuinfo{ 0 };

	__cpuid((int*)&cpuinfo, 1);//leaf==1����


	return cpuinfo.Ecx.Bits.VMX;


}


int enable_vmx_operation(void) {
	
	//����SMX
	union __ia32_feature_control_msr_t feature_msr {0};
	
	//cr4 vmxλ
	union __cr4_t cr4 {0};

	cr4.control = __readcr4();

	cr4.bits.vmx_enable = 1;

	__writecr4(cr4.control);

	feature_msr.control = __readmsr(IA32_FEATURE_CONTROL);

	if (feature_msr.bits.lock == 0) {

		//���뿪����λ+out/inside����֧��VMX����

		feature_msr.bits.lock = 1;
		feature_msr.bits.vmxon_outside_smx = 1;
		__writemsr(IA32_FEATURE_CONTROL, feature_msr.control);
		return true;

		
	}

	return false;
}

void Unload(PDRIVER_OBJECT) {
	
	vmm_exit();
	SyscallHook::GetInsctance()->fn_remove_all_hooks();
}

NTSTATUS MyOpenProcess(	_Out_ PHANDLE ProcessHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_opt_ PCLIENT_ID ClientId) {
	asm_stac();
	DbgPrintEx(77, 0, "[+]NtOpenProcess catched\r\n");
	DbgPrintEx(77, 0, "[+]Process Handle 0%llx DesiredAccess %x ClinetId %llx\r\n",ProcessHandle,DesiredAccess,ClientId);

	return NtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
}

EXTERN_C NTSTATUS DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING usRegPath) {

	UNREFERENCED_PARAMETER(usRegPath);

	if (VmHasCpuidSupport()) {

		KdPrint(("[+]Support Vmx\r\n"));
		
		//����syscallhook
		//��BUG �޷�hook����4������ϵͳ����
		//���ĸ�����Ҫ�Լ��� �Լ���ȡ
		SyscallHook::GetInsctance()->fn_syshook_init(0xfffff800832018c0, 0xfffff8008280aa90, 0xfffff800827f9630, 0xfffff8008280bbc0,FALSE);
		
		
		SyscallHook::GetInsctance()->fn_add_hook_by_index(0x26, (UINT64)MyOpenProcess);
		vmm_init((UINT64)asm_vmx_exit_handler);
		

	}
	else {

		KdPrint(("[+]Not Support Vmx\r\n"));
		return STATUS_NOT_SUPPORTED;

	}

	Driver->DriverUnload = Unload;



	return STATUS_SUCCESS;
}