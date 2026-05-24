/** @file
 * vmxcore.c — BEMI BIOS Ring-(-1) VMX Hypervisor Core
 *
 * Implements Intel VT-x setup, VMCS initialisation, VM-entry/exit dispatch,
 * and per-CPU VCPU management for BEMI BIOS v1.3.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <BemiVmxAsm.h>
#include <BemiVmxExitAsm.h>
#include "../common/HypervisorBackend.h"
#include <ApicShadow.h>

// ─── MSR indices ─────────────────────────────────────────────────────────────
#define VMX_BASIC_MSR               0x480
#define VMX_TRUE_CTRL_MSR           0x48D
#define VMX_ENTRY_CTRL_MSR          0x484
#define VMX_EXIT_CTRL_MSR           0x485
#define VMX_PINBASED_CTRL_MSR       0x481
#define VMX_PROCBASED_CTRL_MSR      0x482
#define VMX_PROCBASED2_CTRL_MSR     0x48B

// ─── VMCS field encodings (16-bit guest state) ────────────────────────────────
#define VMX_GUEST_ES_SEL            0x00000800
#define VMX_GUEST_CS_SEL            0x00000802
#define VMX_GUEST_SS_SEL            0x00000804
#define VMX_GUEST_DS_SEL            0x00000806
#define VMX_GUEST_FS_SEL            0x00000808
#define VMX_GUEST_GS_SEL            0x0000080A
#define VMX_GUEST_LDTR_SEL          0x0000080C
#define VMX_GUEST_TR_SEL            0x0000080E

// ─── VMCS field encodings (64-bit guest state) ────────────────────────────────
#define VMX_GUEST_ES_BASE           0x00000600
#define VMX_GUEST_CS_BASE           0x00000608
#define VMX_GUEST_SS_BASE           0x00000610
#define VMX_GUEST_DS_BASE           0x00000618
#define VMX_GUEST_FS_BASE           0x00000620
#define VMX_GUEST_GS_BASE           0x00000628
#define VMX_GUEST_LDTR_BASE         0x00000630
#define VMX_GUEST_TR_BASE           0x00000638
#define VMX_GUEST_GDTR_BASE         0x00000658
#define VMX_GUEST_IDTR_BASE         0x0000065C
#define VMX_GUEST_RSP               0x0000681C
#define VMX_GUEST_RIP               0x0000681E
#define VMX_GUEST_CR0               0x00006800
#define VMX_GUEST_CR3               0x00006802
#define VMX_GUEST_CR4               0x00006804
#define VMX_GUEST_RFLAGS            0x00006820
#define VMX_GUEST_EFER              0x00002806

// ─── VMCS field encodings (32-bit controls) ───────────────────────────────────
#define VMX_CTRL_PIN_BASED          0x00004000
#define VMX_CTRL_PROC_BASED         0x00004002
#define VMX_CTRL_PROC_BASED2        0x0000401E
#define VMX_CTRL_ENTRY              0x00004012
#define VMX_CTRL_EXIT               0x0000400C
#define VMX_CTRL_EXCEPTION_BITMAP   0x00004004
#define VMX_CTRL_PAGE_FAULT_ERC     0x00004006

// ─── EPT / VPID ──────────────────────────────────────────────────────────────
#define VMX_CTRL_EPT_POINTER        0x0000201A
#define VMX_CTRL_VPID               0x00000000

// ─── VM-exit reason codes ────────────────────────────────────────────────────
#define EXIT_REASON_CPUID           0x0A
#define EXIT_REASON_HLT             0x0C
#define EXIT_REASON_INVD            0x0D
#define EXIT_REASON_VMCALL          0x12
#define EXIT_REASON_CR_ACCESS       0x1C
#define EXIT_REASON_IO_INSTRUCTION  0x1E
#define EXIT_REASON_RDMSR           0x1F
#define EXIT_REASON_WRMSR           0x20
#define EXIT_REASON_EPT_VIOLATION   0x30
#define EXIT_REASON_EPT_MISCONFIG   0x31
#define EXIT_REASON_PREEMPTION      0x34
#define EXIT_REASON_EXTERNAL_INT    0x01
#define EXIT_REASON_INT_WINDOW      0x07
#define EXIT_REASON_XSETBV          0x37

// ─── VMCS control bit helpers ────────────────────────────────────────────────
#define PROCBASED_RDTSC_EXITING     BIT12
#define PROCBASED_MWAIT_EXITING     BIT24
#define PROCBASED_USE_MSR_BITMAPS   BIT28
#define PROCBASED2_ENABLE_EPT       BIT1
#define PROCBASED2_ENABLE_VPID      BIT5
#define PROCBASED2_ENABLE_XSAVES    BIT20
#define PINBASED_EXT_INT_EXITING    BIT0
#define PINBASED_NMI_EXITING        BIT3
#define EXIT_CTRL_SAVE_DEBUG        BIT2
#define EXIT_CTRL_HOST_ADDR_SPACE   BIT9
#define EXIT_CTRL_LOAD_EFER         BIT21
#define ENTRY_CTRL_LOAD_EFER        BIT15

// ─── VCPU state block ────────────────────────────────────────────────────────
typedef struct {
  UINT64 VmcsPhysAddr;
  UINT64 VmxonRegionPhys;
  BOOLEAN Active;
} VCPU_STATE;

STATIC VCPU_STATE  gVcpuState[MAX_LOGICAL_PROCESSORS];
STATIC UINT64      gCpuCount = 0;
HYPERVISOR_BACKEND gHypervisorBackend;

// ─── Forward declarations ─────────────────────────────────────────────────────
STATIC EFI_STATUS  VmxSetupVmcs        (IN UINT64 VcpuId, IN UINT64 EntryPoint, IN UINT64 StackPointer);
STATIC VOID        VmxReadGuestState    (OUT GUEST_STATE *State);
STATIC VOID        VmxWriteGuestState   (IN GUEST_STATE  *State);
STATIC UINT64      AdjustCtrl           (UINT64 Msr, UINT64 Requested);

// ─── Adjust a VMCS control field using TRUE_CTRL MSRs ────────────────────────
STATIC UINT64
AdjustCtrl (
  UINT64  Msr,
  UINT64  Requested
  )
{
  UINT64 MsrValue = AsmReadMsr64(Msr);
  UINT64 AllowedZero = MsrValue & 0xFFFFFFFF;        // bits that must be 0
  UINT64 AllowedOne  = (MsrValue >> 32) & 0xFFFFFFFF; // bits that may be 1
  return (Requested | AllowedZero) & AllowedOne;
}

// ─── HypervisorDetectVendor ──────────────────────────────────────────────────
EFI_STATUS
HypervisorDetectVendor (
  OUT HYPERVISOR_VENDOR *Vendor
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  BOOLEAN VmxSupported;

  if (Vendor == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // CPUID leaf 1, ECX[5] = VMX, ECX[2] = SVM
  AsmCpuid(1, &Eax, &Ebx, &Ecx, &Edx);
  VmxSupported = (Ecx & BIT5) != 0;

  if (VmxSupported) {
    *Vendor = HypervisorIntel;
    DEBUG((DEBUG_INFO, "VMX: Intel VT-x detected (CPUID ECX[5]=1)\n"));
    return EFI_SUCCESS;
  }

  // Check for AMD SVM (CPUID 0x80000001, ECX[2])
  AsmCpuid(0x80000001, &Eax, &Ebx, &Ecx, &Edx);
  if (Ecx & BIT2) {
    *Vendor = HypervisorAmd;
    DEBUG((DEBUG_INFO, "VMX: AMD SVM detected — will use svmcore\n"));
    return EFI_SUCCESS;
  }

  *Vendor = HypervisorUnknown;
  DEBUG((DEBUG_ERROR, "VMX: No hardware virtualisation support found\n"));
  return EFI_UNSUPPORTED;
}

// ─── HypervisorBackendInit ────────────────────────────────────────────────────
EFI_STATUS
HypervisorBackendInit (
  IN UINT64 CpuCount
  )
{
  EFI_STATUS  Status;
  UINT64      Cr0, Cr4;
  UINT64      FeatureCtrl;
  UINT64      VmxBasic;

  DEBUG((DEBUG_INFO, "VMX: Initialising backend for %lld CPUs\n", CpuCount));

  gCpuCount = CpuCount;
  ZeroMem(gVcpuState, sizeof(gVcpuState));

  // Verify VMX is enabled in CR4
  Cr4 = AsmReadCr4();
  if (!(Cr4 & BIT13)) {
    Cr4 |= BIT13;
    AsmWriteCr4(Cr4);
    DEBUG((DEBUG_INFO, "VMX: Enabled VMXE in CR4\n"));
  }

  // Verify CR0 compatibility
  Cr0 = AsmReadCr0();
  if (!(Cr0 & BIT31) || !(Cr0 & BIT0)) {
    DEBUG((DEBUG_ERROR, "VMX: CR0 not in valid VMX state (PE=%d, PG=%d)\n",
      (UINT32)(Cr0 & BIT0) != 0, (UINT32)(Cr0 & BIT31) != 0));
    return EFI_UNSUPPORTED;
  }

  // Check and lock the IA32_FEATURE_CONTROL MSR
  FeatureCtrl = AsmReadMsr64(0x3A);
  if (!(FeatureCtrl & BIT0)) {
    DEBUG((DEBUG_ERROR, "VMX: IA32_FEATURE_CONTROL is unlocked — BIOS should lock it\n"));
    return EFI_UNSUPPORTED;
  }
  if (!(FeatureCtrl & BIT2)) {
    DEBUG((DEBUG_ERROR, "VMX: VMX outside SMX is not enabled in IA32_FEATURE_CONTROL\n"));
    return EFI_UNSUPPORTED;
  }

  // Read VMCS revision identifier
  VmxBasic = AsmReadMsr64(VMX_BASIC_MSR);
  DEBUG((DEBUG_INFO, "VMX: VMX_BASIC_MSR=0x%llx, RevId=0x%08x\n",
    VmxBasic, (UINT32)(VmxBasic & 0x7FFFFFFF)));

  gHypervisorBackend.Vendor        = HypervisorIntel;
  gHypervisorBackend.Init          = HypervisorBackendInit;
  gHypervisorBackend.VcpuCreate    = VmxVcpuCreate;
  gHypervisorBackend.VcpuRun       = VmxVcpuRun;
  gHypervisorBackend.HandleExit    = VmxHandleExit;
  gHypervisorBackend.FeatureFlags  = 0;
  gHypervisorBackend.PageTableHost = AsmReadCr3();

  Status = EFI_SUCCESS;
  DEBUG((DEBUG_INFO, "VMX: Backend initialised OK\n"));
  return Status;
}

// ─── VmxVcpuCreate ───────────────────────────────────────────────────────────
EFI_STATUS
VmxVcpuCreate (
  IN UINT64 VcpuId,
  IN UINT64 EntryPoint,
  IN UINT64 StackPointer
  )
{
  EFI_STATUS  Status;
  VOID        *VmxonRegion;
  VOID        *VmcsRegion;
  UINT64      VmxBasic;
  UINT32      RevId;

  if (VcpuId >= MAX_LOGICAL_PROCESSORS) {
    return EFI_INVALID_PARAMETER;
  }

  VmxBasic = AsmReadMsr64(VMX_BASIC_MSR);
  RevId    = (UINT32)(VmxBasic & 0x7FFFFFFF);

  // Allocate 4 KB VMXON region
  VmxonRegion = AllocateAlignedPages(1, SIZE_4KB);
  if (VmxonRegion == NULL) {
    DEBUG((DEBUG_ERROR, "VMX: Failed to allocate VMXON region for vCPU %lld\n", VcpuId));
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem(VmxonRegion, SIZE_4KB);
  *(UINT32 *)VmxonRegion = RevId;

  // Allocate 4 KB VMCS region
  VmcsRegion = AllocateAlignedPages(1, SIZE_4KB);
  if (VmcsRegion == NULL) {
    FreeAlignedPages(VmxonRegion, 1);
    DEBUG((DEBUG_ERROR, "VMX: Failed to allocate VMCS region for vCPU %lld\n", VcpuId));
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem(VmcsRegion, SIZE_4KB);
  *(UINT32 *)VmcsRegion = RevId;

  gVcpuState[VcpuId].VmxonRegionPhys = (UINT64)(UINTN)VmxonRegion;
  gVcpuState[VcpuId].VmcsPhysAddr    = (UINT64)(UINTN)VmcsRegion;
  gVcpuState[VcpuId].Active          = FALSE;

  // Execute VMXON
  if (BemiVmxOn(&gVcpuState[VcpuId].VmxonRegionPhys) != 0) {
    DEBUG((DEBUG_ERROR, "VMX: VMXON failed for vCPU %lld\n", VcpuId));
    return EFI_DEVICE_ERROR;
  }

  // Clear and load VMCS
  if (BemiVmClear(&gVcpuState[VcpuId].VmcsPhysAddr) != 0) {
    DEBUG((DEBUG_ERROR, "VMX: VMCLEAR failed for vCPU %lld\n", VcpuId));
    return EFI_DEVICE_ERROR;
  }
  if (BemiVmPtrLd(&gVcpuState[VcpuId].VmcsPhysAddr) != 0) {
    DEBUG((DEBUG_ERROR, "VMX: VMPTRLD failed for vCPU %lld\n", VcpuId));
    return EFI_DEVICE_ERROR;
  }

  // Configure VMCS fields
  Status = VmxSetupVmcs(VcpuId, EntryPoint, StackPointer);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  gVcpuState[VcpuId].Active = TRUE;
  DEBUG((DEBUG_INFO, "VMX: vCPU %lld created, ENTRY=0x%llx SP=0x%llx\n",
    VcpuId, EntryPoint, StackPointer));
  return EFI_SUCCESS;
}

// ─── VmxSetupVmcs (internal) ─────────────────────────────────────────────────
STATIC EFI_STATUS
VmxSetupVmcs (
  IN UINT64 VcpuId,
  IN UINT64 EntryPoint,
  IN UINT64 StackPointer
  )
{
  UINT64 PinCtrl, ProcCtrl, ProcCtrl2, ExitCtrl, EntryCtrl;
  UINT64 Gdtr, Idtr;
  UINT64 HostCr3;

  (VOID)VcpuId;

  // ─── Control fields ───────────────────────────────────────────────────────
  PinCtrl = AdjustCtrl(VMX_PINBASED_CTRL_MSR,
              PINBASED_EXT_INT_EXITING | PINBASED_NMI_EXITING);

  ProcCtrl = AdjustCtrl(VMX_PROCBASED_CTRL_MSR,
               PROCBASED_USE_MSR_BITMAPS | PROCBASED_MWAIT_EXITING);

  ProcCtrl2 = AdjustCtrl(VMX_PROCBASED2_CTRL_MSR,
                PROCBASED2_ENABLE_EPT | PROCBASED2_ENABLE_VPID |
                PROCBASED2_ENABLE_XSAVES);

  ExitCtrl = AdjustCtrl(VMX_EXIT_CTRL_MSR,
               EXIT_CTRL_HOST_ADDR_SPACE | EXIT_CTRL_LOAD_EFER |
               EXIT_CTRL_SAVE_DEBUG);

  EntryCtrl = AdjustCtrl(VMX_ENTRY_CTRL_MSR,
                ENTRY_CTRL_LOAD_EFER);

  (VOID)BemiVmWrite(VMX_CTRL_PIN_BASED,        PinCtrl);
  (VOID)BemiVmWrite(VMX_CTRL_PROC_BASED,       ProcCtrl);
  (VOID)BemiVmWrite(VMX_CTRL_PROC_BASED2,      ProcCtrl2);
  (VOID)BemiVmWrite(VMX_CTRL_EXIT,             ExitCtrl);
  (VOID)BemiVmWrite(VMX_CTRL_ENTRY,            EntryCtrl);
  (VOID)BemiVmWrite(VMX_CTRL_EXCEPTION_BITMAP, 0);
  (VOID)BemiVmWrite(VMX_CTRL_PAGE_FAULT_ERC,   0);

  // ─── Host state ───────────────────────────────────────────────────────────
  HostCr3 = AsmReadCr3();
  (VOID)BemiVmWrite(0x00006C00, AsmReadCr0());       // HOST_CR0
  (VOID)BemiVmWrite(0x00006C02, HostCr3);            // HOST_CR3
  (VOID)BemiVmWrite(0x00006C04, AsmReadCr4());       // HOST_CR4
  (VOID)BemiVmWrite(0x00002C02, AsmReadMsr64(0xC0000080)); // HOST_EFER

  // ─── Guest segment selectors (flat model, ring-0) ────────────────────────
  (VOID)BemiVmWrite(VMX_GUEST_CS_SEL,   0x08);
  (VOID)BemiVmWrite(VMX_GUEST_DS_SEL,   0x10);
  (VOID)BemiVmWrite(VMX_GUEST_ES_SEL,   0x10);
  (VOID)BemiVmWrite(VMX_GUEST_SS_SEL,   0x10);
  (VOID)BemiVmWrite(VMX_GUEST_FS_SEL,   0x10);
  (VOID)BemiVmWrite(VMX_GUEST_GS_SEL,   0x10);
  (VOID)BemiVmWrite(VMX_GUEST_LDTR_SEL, 0x00);
  (VOID)BemiVmWrite(VMX_GUEST_TR_SEL,   0x18);

  // ─── Guest segment bases (all zero for flat model) ───────────────────────
  (VOID)BemiVmWrite(VMX_GUEST_CS_BASE,   0);
  (VOID)BemiVmWrite(VMX_GUEST_DS_BASE,   0);
  (VOID)BemiVmWrite(VMX_GUEST_ES_BASE,   0);
  (VOID)BemiVmWrite(VMX_GUEST_SS_BASE,   0);
  (VOID)BemiVmWrite(VMX_GUEST_FS_BASE,   0);
  (VOID)BemiVmWrite(VMX_GUEST_GS_BASE,   0);
  (VOID)BemiVmWrite(VMX_GUEST_LDTR_BASE, 0);
  (VOID)BemiVmWrite(VMX_GUEST_TR_BASE,   0);

  // ─── GDTR / IDTR ─────────────────────────────────────────────────────────
  AsmReadGdtr((IA32_DESCRIPTOR *)&Gdtr);
  AsmReadIdtr((IA32_DESCRIPTOR *)&Idtr);
  (VOID)BemiVmWrite(VMX_GUEST_GDTR_BASE, ((IA32_DESCRIPTOR *)&Gdtr)->Base);
  (VOID)BemiVmWrite(VMX_GUEST_IDTR_BASE, ((IA32_DESCRIPTOR *)&Idtr)->Base);

  // ─── Guest register state ─────────────────────────────────────────────────
  (VOID)BemiVmWrite(VMX_GUEST_CR0,    AsmReadCr0());
  (VOID)BemiVmWrite(VMX_GUEST_CR3,    HostCr3);
  (VOID)BemiVmWrite(VMX_GUEST_CR4,    AsmReadCr4());
  (VOID)BemiVmWrite(VMX_GUEST_RFLAGS, BIT1);         // RFLAGS.IF=0, RFLAGS.Reserved=1
  (VOID)BemiVmWrite(VMX_GUEST_EFER,   AsmReadMsr64(0xC0000080));
  (VOID)BemiVmWrite(VMX_GUEST_RIP,    EntryPoint);
  (VOID)BemiVmWrite(VMX_GUEST_RSP,    StackPointer);

  DEBUG((DEBUG_VERBOSE, "VMX: VMCS setup complete\n"));
  return EFI_SUCCESS;
}

// ─── VmxVcpuRun ──────────────────────────────────────────────────────────────
EFI_STATUS
VmxVcpuRun (
  IN UINT64 VcpuId
  )
{
  if (VcpuId >= MAX_LOGICAL_PROCESSORS || !gVcpuState[VcpuId].Active) {
    return EFI_INVALID_PARAMETER;
  }

  if (BemiVmPtrLd(&gVcpuState[VcpuId].VmcsPhysAddr) != 0) {
    return EFI_DEVICE_ERROR;
  }

  // Launch or resume the guest
  BemiVmLaunch();

  // If we get here the launch failed
  DEBUG((DEBUG_ERROR, "VMX: VMLAUNCH failed, error=%lld\n",
    (UINT64)BemiVmRead(0x00004400))); // VM_INSTRUCTION_ERROR
  return EFI_DEVICE_ERROR;
}

// ─── VmxReadGuestState / VmxWriteGuestState (internal) ───────────────────────
STATIC VOID
VmxReadGuestState (
  OUT GUEST_STATE *State
  )
{
  State->GuestRip    = BemiVmRead(VMX_GUEST_RIP);
  State->GuestRsp    = BemiVmRead(VMX_GUEST_RSP);
  State->GuestRflags = BemiVmRead(VMX_GUEST_RFLAGS);
  State->GuestCr0    = BemiVmRead(VMX_GUEST_CR0);
  State->GuestCr3    = BemiVmRead(VMX_GUEST_CR3);
  State->GuestCr4    = BemiVmRead(VMX_GUEST_CR4);
  State->GuestEfer   = BemiVmRead(VMX_GUEST_EFER);
}

STATIC VOID
VmxWriteGuestState (
  IN GUEST_STATE *State
  )
{
  (VOID)BemiVmWrite(VMX_GUEST_RIP,    State->GuestRip);
  (VOID)BemiVmWrite(VMX_GUEST_RSP,    State->GuestRsp);
  (VOID)BemiVmWrite(VMX_GUEST_CR0,    State->GuestCr0);
  (VOID)BemiVmWrite(VMX_GUEST_CR3,    State->GuestCr3);
  (VOID)BemiVmWrite(VMX_GUEST_CR4,    State->GuestCr4);
  (VOID)BemiVmWrite(VMX_GUEST_EFER,   State->GuestEfer);
  (VOID)BemiVmWrite(VMX_GUEST_RFLAGS, State->GuestRflags);
}

// ─── VmxHandleExit ───────────────────────────────────────────────────────────
EFI_STATUS
VmxHandleExit (
  IN     VM_EXIT_INFO  *ExitInfo,
  IN OUT GUEST_STATE   *GuestState
  )
{
  return gHypervisorBackend.HandleExit(ExitInfo, GuestState);
}

// ─── BemiVmxExitHandler — called from BemiVmxExitAsm stub ───────────────────
VOID
BemiVmxExitHandler (
  IN OUT UINT64 *GuestGprBase
  )
{
  GUEST_STATE   guestState;
  VM_EXIT_INFO  exitInfo;
  UINT64        exitReason;
  EFI_STATUS    status;
  UINT32        pendingVector;

  ZeroMem(&guestState, sizeof(guestState));
  ZeroMem(&exitInfo,   sizeof(exitInfo));

  // Restore GPRs saved by the ASM stub
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

  // Read remaining state from VMCS
  VmxReadGuestState(&guestState);

  // Read exit information
  exitReason                      = BemiVmRead(0x00004402); // EXIT_REASON
  exitInfo.ExitReason             = exitReason;
  exitInfo.ExitQualification      = BemiVmRead(0x00006400); // EXIT_QUALIFICATION
  exitInfo.GuestRip               = guestState.GuestRip;
  exitInfo.GuestRsp               = guestState.GuestRsp;
  exitInfo.InstructionLength      = BemiVmRead(0x0000440C); // VM_EXIT_INSTRUCTION_LEN
  exitInfo.ExitInterruptInfo      = BemiVmRead(0x00004404); // IDT_VECTORING_INFO
  exitInfo.ExitInterruptErrorCode = BemiVmRead(0x00004406); // IDT_VECTORING_ERROR_CODE

  DEBUG((DEBUG_VERBOSE, "VMX: Exit reason=%lld RIP=0x%llx\n",
    exitReason, guestState.GuestRip));

  status = EFI_SUCCESS;

  switch (exitReason) {
    case EXIT_REASON_CPUID:
      status = HandleCpuidExit(&exitInfo, &guestState);
      break;

    case EXIT_REASON_RDMSR:
    case EXIT_REASON_WRMSR:
      status = HandleMsrExit(&exitInfo, &guestState);
      break;

    case EXIT_REASON_CR_ACCESS:
      status = HandleCrAccess(&exitInfo, &guestState);
      break;

    case EXIT_REASON_IO_INSTRUCTION:
      status = HandleIoExit(&exitInfo, &guestState);
      break;

    case EXIT_REASON_EXTERNAL_INT:
      // External interrupt — just resume; host IDT handled it
      status = EFI_SUCCESS;
      break;

    case EXIT_REASON_INT_WINDOW:
      // Interrupt window requested and now open — inject pending interrupt
      status = EFI_SUCCESS;
      break;

    case EXIT_REASON_HLT:
      DEBUG((DEBUG_INFO, "VMX: Guest HLT at RIP 0x%llx\n", guestState.GuestRip));
      guestState.GuestRip += exitInfo.InstructionLength;
      status = EFI_SUCCESS;
      break;

    case EXIT_REASON_INVD:
      // INVD — emulate as WBINVD from host perspective
      AsmWbinvd();
      guestState.GuestRip += exitInfo.InstructionLength;
      status = EFI_SUCCESS;
      break;

    case EXIT_REASON_XSETBV:
      // Simplified: advance RIP only
      guestState.GuestRip += exitInfo.InstructionLength;
      status = EFI_SUCCESS;
      break;

    case EXIT_REASON_EPT_VIOLATION:
      DEBUG((DEBUG_ERROR, "VMX: EPT violation at GPA=0x%llx RIP=0x%llx\n",
        exitInfo.ExitQualification, guestState.GuestRip));
      // For now, halt the guest
      guestState.GuestRip += exitInfo.InstructionLength;
      status = EFI_SUCCESS;
      break;

    case EXIT_REASON_EPT_MISCONFIG:
      DEBUG((DEBUG_ERROR, "VMX: EPT misconfiguration at RIP 0x%llx\n", guestState.GuestRip));
      guestState.GuestRip += exitInfo.InstructionLength;
      status = EFI_SUCCESS;
      break;

    case EXIT_REASON_PREEMPTION:
      // VMX preemption timer fired — resume
      status = EFI_SUCCESS;
      break;

    case EXIT_REASON_VMCALL:
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

  // Write back GPRs to the stub's save area
  GuestGprBase[0]  = guestState.GuestRax;
  GuestGprBase[1]  = guestState.GuestRbx;
  GuestGprBase[2]  = guestState.GuestRcx;
  GuestGprBase[3]  = guestState.GuestRdx;
  GuestGprBase[4]  = guestState.GuestRsi;
  GuestGprBase[5]  = guestState.GuestRdi;
  GuestGprBase[6]  = guestState.GuestRbp;
  GuestGprBase[7]  = guestState.GuestR8;
  GuestGprBase[8]  = guestState.GuestR9;
  GuestGprBase[9]  = guestState.GuestR10;
  GuestGprBase[10] = guestState.GuestR11;
  GuestGprBase[11] = guestState.GuestR12;
  GuestGprBase[12] = guestState.GuestR13;
  GuestGprBase[13] = guestState.GuestR14;
  GuestGprBase[14] = guestState.GuestR15;

  // Flush updated VMCS state
  VmxWriteGuestState(&guestState);

  // Deliver a pending APIC interrupt if IF is set
  if (ApicGetPendingInterrupt(&pendingVector) && (guestState.GuestRflags & BIT9)) {
    UINT64 intrInfo = pendingVector | 0x700;
    (VOID)BemiVmWrite(0x00004016, intrInfo);  // VM_ENTRY_INTR_INFO_FIELD
  }

  (VOID)status;
}

// ─── Generic exit handlers ────────────────────────────────────────────────────

EFI_STATUS
HandleCpuidExit (
  IN     VM_EXIT_INFO  *ExitInfo,
  IN OUT GUEST_STATE   *GuestState
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 Leaf    = (UINT32)GuestState->GuestRax;
  UINT32 Subleaf = (UINT32)GuestState->GuestRcx;

  AsmCpuidEx(Leaf, Subleaf, &Eax, &Ebx, &Ecx, &Edx);

  // Hide hypervisor bit (ECX[31]) from the guest on leaf 1
  if (Leaf == 1) {
    Ecx &= ~BIT31;
    // Advertise XSAVE, SSE4.2, POPCNT; hide VMX
    Ecx &= ~BIT5;  // clear VMX
  }

  // Clamp the max leaf to avoid guest confusion
  if (Leaf == 0 && Eax > 0x1F) {
    Eax = 0x1F;
  }

  GuestState->GuestRax = (UINT64)Eax;
  GuestState->GuestRbx = (UINT64)Ebx;
  GuestState->GuestRcx = (UINT64)Ecx;
  GuestState->GuestRdx = (UINT64)Edx;
  GuestState->GuestRip += ExitInfo->InstructionLength;

  DEBUG((DEBUG_VERBOSE, "VMX: CPUID leaf 0x%x => EAX=0x%x EBX=0x%x ECX=0x%x EDX=0x%x\n",
    Leaf, Eax, Ebx, Ecx, Edx));
  return EFI_SUCCESS;
}

EFI_STATUS
HandleMsrExit (
  IN     VM_EXIT_INFO  *ExitInfo,
  IN OUT GUEST_STATE   *GuestState
  )
{
  UINT32 MsrIndex = (UINT32)GuestState->GuestRcx;
  UINT64 MsrValue;

  if (ExitInfo->ExitReason == EXIT_REASON_RDMSR) {
    MsrValue = AsmReadMsr64(MsrIndex);
    GuestState->GuestRax = MsrValue & 0xFFFFFFFF;
    GuestState->GuestRdx = MsrValue >> 32;
    DEBUG((DEBUG_VERBOSE, "VMX: RDMSR 0x%x => 0x%llx\n", MsrIndex, MsrValue));
  } else {
    MsrValue = (GuestState->GuestRax & 0xFFFFFFFF) |
               (GuestState->GuestRdx << 32);
    // Guard against writing to protected MSRs
    if (MsrIndex == 0x3A || MsrIndex == 0x13C) {
      DEBUG((DEBUG_WARN, "VMX: Blocked WRMSR to protected MSR 0x%x\n", MsrIndex));
    } else {
      AsmWriteMsr64(MsrIndex, MsrValue);
    }
    DEBUG((DEBUG_VERBOSE, "VMX: WRMSR 0x%x <= 0x%llx\n", MsrIndex, MsrValue));
  }

  GuestState->GuestRip += ExitInfo->InstructionLength;
  return EFI_SUCCESS;
}

EFI_STATUS
HandleCrAccess (
  IN     VM_EXIT_INFO  *ExitInfo,
  IN OUT GUEST_STATE   *GuestState
  )
{
  UINT64 Qualification = ExitInfo->ExitQualification;
  UINT64 CrNum      = Qualification & 0x0F;
  UINT64 AccessType = (Qualification >> 4) & 0x03; // 0=MOV-to-CR, 1=MOV-from-CR
  UINT64 RegNum     = (Qualification >> 8) & 0x0F;
  UINT64 *GprArray  = (UINT64 *)GuestState;         // first 16 fields are GPRs

  if (AccessType == 0) {
    // MOV to CR
    UINT64 Val = GprArray[RegNum];
    switch (CrNum) {
      case 0: GuestState->GuestCr0 = Val; break;
      case 3: GuestState->GuestCr3 = Val; break;
      case 4: GuestState->GuestCr4 = Val; break;
      default: break;
    }
  } else {
    // MOV from CR
    UINT64 Val = 0;
    switch (CrNum) {
      case 0: Val = GuestState->GuestCr0; break;
      case 3: Val = GuestState->GuestCr3; break;
      case 4: Val = GuestState->GuestCr4; break;
      default: break;
    }
    GprArray[RegNum] = Val;
  }

  GuestState->GuestRip += ExitInfo->InstructionLength;
  return EFI_SUCCESS;
}

EFI_STATUS
HandleIoExit (
  IN     VM_EXIT_INFO  *ExitInfo,
  IN OUT GUEST_STATE   *GuestState
  )
{
  // For now, silently consume all I/O exits
  // A full implementation would call into a virtual device model
  GuestState->GuestRip += ExitInfo->InstructionLength;
  return EFI_SUCCESS;
}
