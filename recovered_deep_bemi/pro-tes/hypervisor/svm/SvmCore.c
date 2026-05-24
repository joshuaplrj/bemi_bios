#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <BemiSvmAsm.h>
#include <BemiVmxExitAsm.h>
#include <ApicShadow.h>
#include "../common/HypervisorBackend.h"

#define SVM_MSR_VM_CR          0xC0010114
#define SVM_MSR_IGNNE          0xC0010115
#define SVM_MSR_VM_HSAVE_PA    0xC0010117

#define SVM_VMCB_CR_INTERCEPTS    0x000
#define SVM_VMCB_DR_INTERCEPTS    0x004
#define SVM_VMCB_EXCEPTION_INTERCEPTS 0x008
#define SVM_VMCB_INSTRUCTION_INTERCEPTS 0x00C
#define SVM_INTERCEPT_CPUID       BIT5
#define SVM_INTERCEPT_MSR         BIT11
#define SVM_INTERCEPT_IOIO        BIT10
#define SVM_INTERCEPT_HLT         BIT7
#define SVM_INTERCEPT_INVLPGA     BIT9
#define SVM_VMCB_INTERCEPT_CR_READ 0x00
#define SVM_VMCB_INTERCEPT_CR_WRITE 0x01

#define SVM_VMCB_GUEST_CR0       0x100
#define SVM_VMCB_GUEST_CR2       0x108
#define SVM_VMCB_GUEST_CR3       0x110
#define SVM_VMCB_GUEST_CR4       0x118
#define SVM_VMCB_GUEST_RIP       0x138
#define SVM_VMCB_GUEST_RFLAGS    0x140
#define SVM_VMCB_GUEST_RSP       0x148
#define SVM_VMCB_GUEST_RAX       0x150
#define SVM_VMCB_GUEST_RSI       0x168
#define SVM_VMCB_GUEST_RDI       0x170
#define SVM_VMCB_GUEST_GDTR_BASE 0x248
#define SVM_VMCB_GUEST_IDTR_BASE 0x260

#define SVM_VMCB_HOST_RSP        0x418
#define SVM_VMCB_HOST_RIP        0x420

#defin
<truncated 8834 bytes>
xitInfo.ExitQualification & 0xFFFFFFFFFFFFF000ULL;
      if (gpa >= 0xFEC00000 && gpa < 0xFF000000) {
        BOOLEAN isWrite = (exitInfo.ExitQualification & BIT5) != 0;
        UINT64 value = guestState.GuestRax;
        if (ApicHandleMmioAccess(gpa, isWrite, &value, 4)) {
          if (!isWrite) guestState.GuestRax = (guestState.GuestRax & ~0xFFFFFFFFULL) | (value & 0xFFFFFFFF);
        }
      }
      guestState.GuestRip += 1;
      status = EFI_SUCCESS;
      break;
    }
    default:
      DEBUG((DEBUG_ERROR, "SVM: Unhandled exit code %lld (0x%llx) at RIP 0x%llx\n",
        exitReason, exitReason, guestState.GuestRip));
      guestState.GuestRip += 1;
      status = EFI_SUCCESS;
      break;
  }

  GuestGprBase[0] = guestState.GuestRax;
  GuestGprBase[1] = guestState.GuestRbx;
  GuestGprBase[2] = guestState.GuestRcx;
  GuestGprBase[3] = guestState.GuestRdx;
  GuestGprBase[4] = guestState.GuestRsi;
  GuestGprBase[5] = guestState.GuestRdi;
  GuestGprBase[6] = guestState.GuestRbp;
  GuestGprBase[7] = guestState.GuestR8;
  GuestGprBase[8] = guestState.GuestR9;
  GuestGprBase[9] = guestState.GuestR10;
  GuestGprBase[10] = guestState.GuestR11;
  GuestGprBase[11] = guestState.GuestR12;
  GuestGprBase[12] = guestState.GuestR13;
  GuestGprBase[13] = guestState.GuestR14;
  GuestGprBase[14] = guestState.GuestR15;

  *(UINT64 *)(vmcb + SVM_VMCB_GUEST_RIP) = guestState.GuestRip;
  *(UINT64 *)(vmcb + SVM_VMCB_GUEST_RSP) = guestState.GuestRsp;
  *(UINT64 *)(vmcb + SVM_VMCB_GUEST_CR0) = guestState.GuestCr0;
  *(UINT64 *)(vmcb + SVM_VMCB_GUEST_CR3) = guestState.GuestCr3;
  *(UINT64 *)(vmcb + SVM_VMCB_GUEST_CR4) = guestState.GuestCr4;
  *(UINT64 *)(vmcb + SVM_VMCB_GUEST_RFLAGS) = guestState.GuestRflags;

  return status;
}
