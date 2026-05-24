#ifndef HYPERVISOR_BACKEND_H
#define HYPERVISOR_BACKEND_H

#include <Uefi.h>

#define MAX_LOGICAL_PROCESSORS 256
#define TRACE_CACHE_SIZE_4MB   0x400000
#define VM_EXIT_REASON_COUNT   68

typedef enum {
  HypervisorIntel = 0,
  HypervisorAmd   = 1,
  HypervisorUnknown
} HYPERVISOR_VENDOR;

typedef struct {
  UINT64  GuestRax, GuestRbx, GuestRcx, GuestRdx;
  UINT64  GuestRsi, GuestRdi, GuestRbp, GuestRsp;
  UINT64  GuestR8,  GuestR9,  GuestR10, GuestR11;
  UINT64  GuestR12, GuestR13, GuestR14, GuestR15;
  UINT64  GuestRip, GuestRflags;
  UINT64  GuestCr0, GuestCr2, GuestCr3, GuestCr4;
  UINT64  GuestDr0, GuestDr1, GuestDr2, GuestDr3, GuestDr6, GuestDr7;
  UINT64  GuestGdtrBase, GuestGdtrLimit;
  UINT64  GuestIdtrBase, GuestIdtrLimit;
  UINT64  GuestTrBase, GuestTrLimit;
  UINT64  GuestLdtrBase, GuestLdtrLimit;
  UINT64  GuestCs, GuestDs, GuestEs, GuestFs, GuestGs, GuestSs;
  UINT64  GuestEfer;
  UINT64  GuestApicBase;
} GUEST_STATE;

typedef struct {
  UINT64  ExitReason;
  UINT64  ExitQualification;
  UINT64  GuestRip;
  UINT64  GuestRsp;
  UINT64  InstructionLength;
  UINT64  ExitInterruptInfo;
  UINT64  ExitInterruptErrorCode;
} VM_EXIT_INFO;

typedef
EFI_STATUS
(*HYPERVISOR_INIT)(
  IN UINT64 CpuCount
  );

typedef
EFI_STATUS
(*HYPERVISOR_VCPU_CREATE)(
  IN UINT64  VcpuId,
  IN UINT64  EntryPoint,
  IN UINT64  StackPointer
  );

typedef
EFI_STATUS
(*HYPERVISOR_VCPU_RUN)(
  IN UINT64 VcpuId
  );

typedef
EFI_STATUS
(*HYPERVISOR_HANDLE_EXIT)(
  IN VM_EXIT_INFO *ExitInfo,
  IN OUT GUEST_STATE *GuestState
  );

typedef struct {
  HYPERVISOR_VENDOR    Vendor;
  HYPERVISOR_INIT      Init;
  HYPERVISOR_VCPU_CREATE VcpuCreate;
  HYPERVISOR_VCPU_RUN    VcpuRun;
  HYPERVISOR_HANDLE_EXIT HandleExit;
  UINT64                 FeatureFlags;
  UINT64                 PageTableHost;
} HYPERVISOR_BACKEND;

extern HYPERVISOR_BACKEND gHypervisorBackend;

EFI_STATUS
HypervisorDetectVendor(
  OUT HYPERVISOR_VENDOR *Vendor
  );

EFI_STATUS
HypervisorBackendInit(
  IN UINT64 CpuCount
  );

EFI_STATUS
HandleCpuidExit(
  IN VM_EXIT_INFO *ExitInfo,
  IN OUT GUEST_STATE *GuestState
  );

EFI_STATUS
HandleMsrExit(
  IN VM_EXIT_INFO *ExitInfo,
  IN OUT GUEST_STATE *GuestState
  );

EFI_STATUS
HandleCrAccess(
  IN VM_EXIT_INFO *ExitInfo,
  IN OUT GUEST_STATE *GuestState
  );

EFI_STATUS
HandleIoExit(
  IN VM_EXIT_INFO *ExitInfo,
  IN OUT GUEST_STATE *GuestState
  );

#endif
