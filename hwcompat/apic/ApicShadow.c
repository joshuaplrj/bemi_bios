#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <ApicShadow.h>

#define LAPIC_REGS_SIZE 0x400
STATIC UINT32 gLapicRegs[LAPIC_REGS_SIZE / 4];
STATIC UINT32 gApicId = 0;
STATIC BOOLEAN gTimerEnabled = FALSE;
STATIC UINT32 gTimerInitialCount = 0;
STATIC UINT32 gTimerCurrentCount = 0;
STATIC UINT32 gTimerLvt = 0;

// Simple 8259 PIC state
STATIC UINT8 gPicMasterMask = 0;
STATIC UINT8 gPicSlaveMask = 0;

VOID
ApicShadowInit (
  IN UINT32 ApicId
  )
{
  gApicId = ApicId;
  ZeroMem(gLapicRegs, sizeof(gLapicRegs));
  
  // Set default values (e.g. APIC ID, Version)
  gLapicRegs[0x20 / 4] = (ApicId << 24); // Local APIC ID Register
  gLapicRegs[0x30 / 4] = 0x50014;       // Local APIC Version Register (e.g., 0x50014 for x2APIC/APIC)
  gLapicRegs[0xF0 / 4] = 0xFF;          // Spurious Interrupt Vector Register
  
  gTimerEnabled = FALSE;
  gTimerInitialCount = 0;
  gTimerCurrentCount = 0;
  gTimerLvt = 0x10000;                  // Masked by default
  
  gPicMasterMask = 0;
  gPicSlaveMask = 0;
  
  DEBUG((DEBUG_INFO, "APIC: Initialised shadow for APIC ID %d\n", ApicId));
}

UINT32
ApicShadowRead (
  IN UINT32 Offset
  )
{
  if (Offset >= LAPIC_REGS_SIZE) {
    return 0;
  }
  return gLapicRegs[Offset / 4];
}

VOID
ApicShadowWrite (
  IN UINT32 Offset,
  IN UINT32 Value
  )
{
  if (Offset >= LAPIC_REGS_SIZE) {
    return;
  }
  
  gLapicRegs[Offset / 4] = Value;
  
  // Handle specific register side-effects
  switch (Offset) {
    case 0x320: // LVT Timer Register
      gTimerLvt = Value;
      break;
    case 0x380: // Initial Count Register
      gTimerInitialCount = Value;
      gTimerCurrentCount = Value;
      gTimerEnabled = (Value > 0) && !(gTimerLvt & BIT16);
      break;
    case 0x3B0: // Divide Configuration Register
      // Custom shift tracking
      break;
    case 0xB0:  // EOI Register
      ApicShadowEoi(0); // Trigger dummy EOI flow
      break;
    default:
      break;
  }
}

VOID
ApicShadowTimerTick (
  VOID
  )
{
  if (gTimerEnabled && gTimerCurrentCount > 0) {
    gTimerCurrentCount--;
    if (gTimerCurrentCount == 0) {
      // Inject timer interrupt if LVT specifies vector
      UINT32 vector = gTimerLvt & 0xFF;
      if (vector >= 16) {
        ApicShadowInjectInterrupt(vector);
      }
      
      // Handle periodic mode
      if (gTimerLvt & BIT17) {
        gTimerCurrentCount = gTimerInitialCount;
      } else {
        gTimerEnabled = FALSE;
      }
    }
  }
}

UINT32
ApicShadowInjectInterrupt (
  IN UINT32 Vector
  )
{
  // Set the corresponding bit in IRR (Interrupt Request Register)
  // IRR starts at 0x200 offset (8 registers of 32-bits)
  UINT32 regIndex = (0x200 + ((Vector / 32) * 16)) / 4;
  if (regIndex < LAPIC_REGS_SIZE / 4) {
    gLapicRegs[regIndex] |= (1 << (Vector % 32));
    DEBUG((DEBUG_VERBOSE, "APIC: Injected Vector %d (IRR reg[%d] = 0x%x)\n",
      Vector, regIndex, gLapicRegs[regIndex]));
    return Vector;
  }
  return 0;
}

VOID
ApicShadowEoi (
  IN UINT32 Vector
  )
{
  // Clear highest priority bit in ISR (In Service Register)
  // ISR starts at 0x100 (8 registers of 32-bits)
  for (INT32 i = 7; i >= 0; i--) {
    UINT32 regIndex = (0x100 + (i * 16)) / 4;
    if (gLapicRegs[regIndex] != 0) {
      UINT32 bit = 31 - AsmMsb32(gLapicRegs[regIndex]);
      gLapicRegs[regIndex] &= ~(1 << bit);
      DEBUG((DEBUG_VERBOSE, "APIC: Clear ISR bit %d (reg[%d] = 0x%x)\n",
        (i * 32) + bit, regIndex, gLapicRegs[regIndex]));
      break;
    }
  }
}

BOOLEAN
ApicGetPendingInterrupt (
  OUT UINT32 *Vector
  )
{
  if (Vector == NULL) {
    return FALSE;
  }
  
  // Find highest priority vector set in IRR
  for (INT32 i = 7; i >= 0; i--) {
    UINT32 regIndex = (0x200 + (i * 16)) / 4;
    if (gLapicRegs[regIndex] != 0) {
      UINT32 bit = 31 - AsmMsb32(gLapicRegs[regIndex]);
      UINT32 v = (i * 32) + bit;
      
      // Move from IRR to ISR
      gLapicRegs[regIndex] &= ~(1 << bit);
      UINT32 isrRegIndex = (0x100 + (i * 16)) / 4;
      gLapicRegs[isrRegIndex] |= (1 << bit);
      
      *Vector = v;
      return TRUE;
    }
  }
  return FALSE;
}

BOOLEAN
ApicHandleMmioAccess (
  IN UINT64  GuestPhysicalAddress,
  IN BOOLEAN IsWrite,
  IN OUT UINT64  *RegisterValue,
  IN UINT8   AccessSize
  )
{
  if (GuestPhysicalAddress < 0xFEE00000 || GuestPhysicalAddress >= 0xFEE01000) {
    return FALSE;
  }
  
  UINT32 offset = (UINT32)(GuestPhysicalAddress - 0xFEE00000);
  if (IsWrite) {
    ApicShadowWrite(offset, (UINT32)*RegisterValue);
  } else {
    *RegisterValue = ApicShadowRead(offset);
  }
  return TRUE;
}

VOID
Pic8259Write (
  IN UINT16 Port,
  IN UINT8  Value
  )
{
  if (Port == 0x21) {
    gPicMasterMask = Value;
  } else if (Port == 0xA1) {
    gPicSlaveMask = Value;
  }
  DEBUG((DEBUG_VERBOSE, "PIC: Port 0x%x <= 0x%x\n", Port, Value));
}

UINT8
Pic8259Read (
  IN UINT16 Port
  )
{
  if (Port == 0x21) {
    return gPicMasterMask;
  } else if (Port == 0xA1) {
    return gPicSlaveMask;
  }
  return 0;
}
