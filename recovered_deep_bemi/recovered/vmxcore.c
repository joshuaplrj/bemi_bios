#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <BemiVmxAsm.h>
#include <BemiVmxExitAsm.h>
#include "../common/HypervisorBackend.h"
#include <ApicShadow.h>

#define VMX_BASIC_MSR         0x480
#define VMX_TRUE_CTRL_MSR     0x48D
#define VMX_ENTRY_CTRL_MSR    0x484
#define VMX_EXIT_CTRL_MSR     0x485
#define VMX_PINBASED_CTRL_MSR 0x481
#define VMX_PROCBASED_CTRL_MSR 0x482
#define VMX_PROCBASED2_CTRL_MSR 0x48B

#define VMX_GUEST_ES_BASE     0x00000600
#define VMX_GUEST_CS_BASE     0x00000608
#define VMX_GUEST_SS_BASE     0x00000610
#define VMX_GUEST_DS_BASE     0x00000618
#define VMX_GUEST_FS_BASE     0x00000620
#define VMX_GUEST_GS_BASE     0x00000628
#define VMX_GUEST_LDTR_BASE   0x00000630
#define VMX_GUEST_TR_BASE     0x00000638
#define VMX_GUEST_GDTR_BASE   0x00000658
#define VMX_GUEST_IDTR_BASE   0x0000065C
#define VMX_GUEST_RSP         0x0000681C
#define VMX_GUEST_RIP         0x0000681E
#define VMX_GUEST_CR0         0x00006800
#define VMX_GUEST_CR3         0x00006802
#define VMX_GUEST_CR4         0x00006804
#define VMX_GUEST_CS_SEL      0x00000802
#define VMX_GUEST_SS_SEL      0x00000804
#define VMX_GUEST_DS_SEL      0x00000806
#define VMX_GUEST_ES_SEL      0x00000800
#define VMX_GUEST_FS_SEL      0x00000808
#define VMX_GUEST_GS_SEL      0x0000080A
4
<truncated 31 bytes>
UCCESS;
      break;
    case 0x12:
      DEBUG((DEBUG_INFO, "VMX: VMCALL from guest at RIP 0x%llx\n", guestState.GuestRip));
      guestState.GuestRip += exitInfo.InstructionLength;
      status = EFI_SUCCESS;
      break;
    default:
      DEBUG((DEBUG_ERROR, "VMX: Unhandled exit reason %lld (0x%llx) at RIP 0x%llx\n",
        exitReason, exitReason, guestState.GuestRip));
      guestState.GuestRip += exitInfo.InstructionLength;
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

  (VOID)BemiVmWrite(VMX_GUEST_RIP, guestState.GuestRip);
  (VOID)BemiVmWrite(VMX_GUEST_RSP, guestState.GuestRsp);
  (VOID)BemiVmWrite(VMX_GUEST_CR0, guestState.GuestCr0);
  (VOID)BemiVmWrite(VMX_GUEST_CR3, guestState.GuestCr3);
  (VOID)BemiVmWrite(VMX_GUEST_CR4, guestState.GuestCr4);
  (VOID)BemiVmWrite(VMX_GUEST_EFER, guestState.GuestEfer);
  (VOID)BemiVmWrite(VMX_GUEST_RFLAGS, guestState.GuestRflags);

  UINT32 pendingVector;
  if (ApicGetPendingInterrupt(&pendingVector) && (guestState.GuestRflags & BIT9)) {
    UINT64 intrInfo = pendingVector | 0x700;
    (VOID)BemiVmWrite(0x00004016, intrInfo);
  }

  return status;
}
