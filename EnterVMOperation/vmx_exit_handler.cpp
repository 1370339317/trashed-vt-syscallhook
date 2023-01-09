#include "vm_exit_handler.h"
#include "vmcscode.h"
#include <intrin.h>
#include "Struct.h"
#include "vmx.h"

//����g_old_syscall
EXTERN_C UINT64 g_orig_system_call;

void vmx_cpuid_handler(PGuestContext context) {

	//for test
	if (context->mRax == 0x8888) {

		//������Ҫ����
		context->mRax = 0x1;
		context->mRbx = 0x2;
		context->mRcx = 0x3;
		context->mRdx = 0x4;
	}
	else {
		cpuinfo info;
		//VMM �в������� 
		__cpuidex((int*)&info,(int)context->mRax, (int)context->mRcx);
		context->mRax = info.Eax;
		context->mRbx = info.Ebx;
		context->mRcx = info.Ecx.all;
		context->mRdx = info.Edx;
	}


}

void vmx_read_msr_handler(PGuestContext context) {

	ULONG64 uMsrVaule = __readmsr((unsigned int)context->mRcx);

	if (context->mRcx == 0xc0000082) {

		DbgPrintEx(77, 0, "[+]syscall ia32_lstar msr read\r\n");

		//���ٵ�ֵ
		context->mRax=g_orig_system_call & 0xffffffff;
		context->mRdx = (g_orig_system_call >> 32) & 0xffffffff;
		return;
	}

	context->mRax = uMsrVaule & 0xffffffff;
	context->mRdx = (uMsrVaule >> 32) & 0xffffffff;

}

void vmx_write_msr_handler(PGuestContext context) {

	if (context->mRcx == 0xc0000082) {

		DbgPrintEx(77, 0, "[+]syscall ia32_lstar msr write\r\n");
		//ֱ�ӷ���
		return;
	}

	ULONG64 uMsrVaule = context->mRax | context->mRdx;

	__writemsr((unsigned long)context->mRcx, uMsrVaule);
}


EXTERN_C void vmx_exit_handler(PGuestContext guest_context) {

	UNREFERENCED_PARAMETER(guest_context);

	//VM Exit����������������
	ULONG64 uVmExitReason = 0;
	ULONG64 uRip = 0;
	ULONG64 uRsp = 0;
	ULONG64 uInstLen = 0;//Vm exit�ĳ���
	ULONG64 uInstInfo = 0;//ָ�����ϸ��Ϣ
	__vmx_vmread(VM_EXIT_REASON, &uVmExitReason);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &uInstLen);
	__vmx_vmread(VMX_INSTRUCTION_INFO,&uInstInfo);
	__vmx_vmread(GUEST_RSP,&uRsp);
	__vmx_vmread(GUEST_RIP,&uRip);

	//reason�����Ĺ̶���ʽ
	uVmExitReason &= 0xffff;

	//DbgPrintEx(77, 0, "[+]reason %d\r\n",uVmExitReason);

	switch (uVmExitReason)
	{
		case	EXIT_REASON_VMCALL:{


			if (guest_context->mRax != 'exit') {


				//ʧ��
				ULONG64 uRflags = 0;
				__vmx_vmread(GUEST_RFLAGS, &uRflags);
				uRflags |= 0x41;
				__vmx_vmwrite(GUEST_RFLAGS, uRflags);
				break;
			}
			else {
				
				__vmx_off();
				//���ʱ�����ǲ�������������
				//��ҪJmp������
				asm_jmup_target(uRip + uInstLen, uRsp);
			}

		}
		case	EXIT_REASON_VMCLEAR: {


			break;
		}
		case	EXIT_REASON_VMLAUNCH:break;
		case	EXIT_REASON_VMPTRLD:break;
		case	EXIT_REASON_VMPTRST:break;
		case	EXIT_REASON_VMREAD: break;
		case    EXIT_REASON_XSETBV: {
			//Win 10Ҫ�� 
			//����32λ
			ULONG64 vaule = guest_context->mRax | guest_context->mRcx;
			_xsetbv((int)guest_context->mRcx,vaule);
			break;
		}
		case EXIT_REASON_GETSEC: {
				//һ��OS��������
			DbgBreakPoint();

			DbgPrintEx(77, 0, "[+]Unk vm exit!\r\n");

			break;

		}
		case EXIT_REASON_INVD: {

			asm_invd();

			break;
		}
		case	EXIT_REASON_VMRESUME:
		case	EXIT_REASON_VMWRITE:
		case	EXIT_REASON_VMXOFF:
		case	EXIT_REASON_VMXON: {
		//��ֹ����VT����
		ULONG64 uRflags = 0;
		__vmx_vmread(GUEST_RFLAGS, &uRflags);
		uRflags |= 0x41;
		__vmx_vmwrite(GUEST_RFLAGS, uRflags);
		break;
	}
	case EXIT_REASON_CPUID: {

		vmx_cpuid_handler(guest_context);
		break;
	}
	case EXIT_REASON_MSR_READ: {

		//DbgPrintEx(77, 0, "[+]Msr read %x\r\n",guest_context->mRcx);

		vmx_read_msr_handler(guest_context);

		break;
	}
	case EXIT_REASON_MSR_WRITE: {

		//DbgPrintEx(77, 0, "[+]msr write %x vaule\r\n",guest_context->mRcx);

		vmx_write_msr_handler(guest_context);

		break;

	}
	default:
		break;
	}

	__vmx_vmwrite(GUEST_RIP, uRip + uInstLen);
	__vmx_vmwrite(GUEST_RSP, uRsp);

}