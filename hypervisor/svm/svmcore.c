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

typedef struct {
  UINT64  VmcbPhysAddr;
  UINT64  HsavePhysAddr;
  BOOLEAN Active;
} SVM_VCPU_STATE;

STATIC SVM_VCPU_STATE gSvmVcpuState[MAX_LOGICAL_PROCESSORS];
STATIC UINT64         gSvmCpuCount = 0;

EFI_STATUS
SvmInit(
  IN UINT64 CpuCount
  )
{
  gSvmCpuCount = CpuCount;
  ZeroMem(gSvmVcpuState, sizeof(gSvmVcpuState));

  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x80000001, &eax, &ebx, &ecx, &edx);
  if (!(ecx & BIT2)) {
    DEBUG((DEBUG_ERROR, "SVM: SVM is not supported by this CPU\n"));
    return EFI_UNSUPPORTED;
  }

  UINT64 efer = AsmReadMsr64(0xC0000080);
  if (!(efer & BIT12)) {
    efer |= BIT12;
    AsmWriteMsr64(0xC0000080, efer);
    DEBUG((DEBUG_INFO, "SVM: Enabled SVME in EFER\n"));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
SvmCreateVcpu(
  IN UINT64 VcpuId,
  IN UINT64 EntryPoint,
  IN UINT64 StackPointer
  )
{
  if (VcpuId >= gSvmCpuCount) {
    return EFI_INVALID_PARAMETER;
  }

  VOID *vmcb = AllocateAlignPages(1, 4096);
  if (vmcb == NULL) return EFI_OUT_OF_RESOURCES;
  ZeroMem(vmcb, 4096);

  VOID *hsave = AllocateAlignPages(1, 4096);
  if (hsave == NULL) {
    FreePages(vmcb, 1);
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem(hsave, 4096);

  gSvmVcpuState[VcpuId].VmcbPhysAddr = (UINT64)(UINTN)vmcb;
  gSvmVcpuState[VcpuId].HsavePhysAddr = (UINT64)(UINTN)hsave;
  gSvmVcpuState[VcpuId].Active = TRUE;

  AsmWriteMsr64(SVM_MSR_VM_HSAVE_PA, (UINT64)(UINTN)hsave);

  *(UINT64 *)((UINT8 *)vmcb + SVM_VMCB_GUEST_RIP) = EntryPoint;
  *(UINT64 *)((UINT8 *)vmcb + SVM_VMCB_GUEST_RSP) = StackPointer;
  *(UINT64 *)((UINT8 *)vmcb + SVM_VMCB_GUEST_CR0) = AsmReadCr0();
  *(UINT64 *)((UINT8 *)vmcb + SVM_VMCB_GUEST_CR3) = AsmReadCr3();
  *(UINT64 *)((UINT8 *)vmcb + SVM_VMCB_GUEST_CR4) = AsmReadCr4();
  *(UINT64 *)((UINT8 *)vmcb + SVM_VMCB_GUEST_RFLAGS) = 0x02;

  *(UINT32 *)((UINT8 *)vmcb + SVM_VMCB_CR_INTERCEPTS) = SVM_VMCB_INTERCEPT_CR_READ | SVM_VMCB_INTERCEPT_CR_WRITE;
  *(UINT32 *)((UINT8 *)vmcb + SVM_VMCB_INSTRUCTION_INTERCEPTS) = SVM_INTERCEPT_CPUID | SVM_INTERCEPT_MSR | SVM_INTERCEPT_IOIO | SVM_INTERCEPT_HLT;

  DEBUG((DEBUG_INFO, "SVM: Created VCPU %lld (VMCB=0x%llx, HSAVE=0x%llx)\n",
    VcpuId, gSvmVcpuState[VcpuId].VmcbPhysAddr, gSvmVcpuState[VcpuId].HsavePhysAddr));

  return EFI_SUCCESS;
}

EFI_STATUS
BemiSvmExitHandler (
  IN UINT8          *vmcb,
  IN OUT UINT64     *GuestGprBase
  )
{
  GUEST_STATE   guestState;
  VM_EXIT_INFO  exitInfo;
  UINT64        exitReason;
  EFI_STATUS    status;

  ZeroMem(&guestState, sizeof(guestState));
  ZeroMem(&exitInfo,   sizeof(exitInfo));

  guestState.GuestRax = GuestGprBase[0];
  guestState.GuestRbx = GuestGprBase[1];
  guestState.GuestRcx = GuestGprBase[2];
  guestState.GuestRdx = GuestGprBase[3];
  guestState.GuestRsi = GuestGprBase[4];
  guestState.GuestRdi = GuestGprBase[5];
  guestState.GuestRbp = GuestGprBase[6];
  guestState.GuestR8  = GuestGprBase[7];
  guestState.GuestR9  = GuestGprBase[8];
  guestState.GuestR10 = GuestGprBase[9];
  guestState.GuestR11 = GuestGprBase[10];
  guestState.GuestR12 = GuestGprBase[11];
  guestState.GuestR13 = GuestGprBase[12];
  guestState.GuestR14 = GuestGprBase[13];
  guestState.GuestR15 = GuestGprBase[14];

  guestState.GuestRip = *(UINT64 *)(vmcb + SVM_VMCB_GUEST_RIP);
  guestState.GuestRsp = *(UINT64 *)(vmcb + SVM_VMCB_GUEST_RSP);
  guestState.GuestCr0 = *(UINT64 *)(vmcb + SVM_VMCB_GUEST_CR0);
  guestState.GuestCr3 = *(UINT64 *)(vmcb + SVM_VMCB_GUEST_CR3);
  guestState.GuestCr4 = *(UINT64 *)(vmcb + SVM_VMCB_GUEST_CR4);
  guestState.GuestRflags = *(UINT64 *)(vmcb + SVM_VMCB_GUEST_RFLAGS);

  exitReason = *(UINT64 *)(vmcb + 0x70);
  exitInfo.ExitReason = exitReason;
  exitInfo.ExitQualification = *(UINT64 *)(vmcb + 0x78);
  UINT64 nextRip = *(UINT64 *)(vmcb + 0x90);
  exitInfo.InstructionLength = (nextRip > guestState.GuestRip) ? (nextRip - guestState.GuestRip) : 3;
  exitInfo.GuestRip = guestState.GuestRip;
  exitInfo.GuestRsp = guestState.GuestRsp;

  DEBUG((DEBUG_VERBOSE, "SVM: Exit reason=%lld RIP=0x%llx\n",
    exitReason, guestState.GuestRip));

  status = EFI_SUCCESS;

  switch (exitReason) {
    case 0x72: {
      status = HandleCpuidExit(&exitInfo, &guestState);
      break;
    }
    case 0x7C: {
      status = HandleMsrExit(&exitInfo, &guestState);
      break;
    }
    case 0x00:
    case 0x03:
    case 0x04:
    case 0x10:
    case 0x13:
    case 0x14: {
      status = HandleCrAccess(&exitInfo, &guestState);
      break;
    }
    case 0x7B: {
      status = HandleIoExit(&exitInfo, &guestState);
      break;
    }
    case 0x78: {
      DEBUG((DEBUG_INFO, "SVM: HLT exit, resuming\n"));
      guestState.GuestRip += exitInfo.InstructionLength;
      break;
    }
    case 0x400: {
      UINT64 gpa = exitInfo.ExitQualification & 0xFFFFFFFFFFFFF000ULL;
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

EFI_STATUS
SvmRunVcpu(
  IN UINT64 VcpuId
  )
{
  if (VcpuId >= gSvmCpuCount || !gSvmVcpuState[VcpuId].Active) {
    return EFI_INVALID_PARAMETER;
  }

  UINT8 *vmcb = (UINT8 *)(UINTN)gSvmVcpuState[VcpuId].VmcbPhysAddr;
  UINT64 guestGpr[15];
  ZeroMem(guestGpr, sizeof(guestGpr));

  DEBUG((DEBUG_INFO, "SVM: Entering guest VCPU %lld loop\n", VcpuId));

  while (TRUE) {
    UINT8 result = BemiSvmVmRun(&gSvmVcpuState[VcpuId].VmcbPhysAddr);
    if (result != 0) {
      DEBUG((DEBUG_ERROR, "SVM: VMRUN failed, result=%d\n", result));
      return EFI_DEVICE_ERROR;
    }

    EFI_STATUS status = BemiSvmExitHandler(vmcb, guestGpr);
    if (EFI_ERROR(status)) {
      DEBUG((DEBUG_ERROR, "SVM: Exit handler returned error %r\n", status));
      return status;
    }
  }

  return EFI_SUCCESS;
}
