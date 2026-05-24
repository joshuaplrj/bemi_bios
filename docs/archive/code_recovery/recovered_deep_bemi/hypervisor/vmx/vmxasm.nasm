BITS 64
DEFAULT REL

SECTION .text

global ASM_PFX(BemiVmxOn)
ASM_PFX(BemiVmxOn):
    vmxon   [rcx]
    jbe     .fail
    xor     eax, eax
    ret
.fail:
    mov     eax, 1
    ret

global ASM_PFX(BemiVmxOff)
ASM_PFX(BemiVmxOff):
    vmxoff
    jbe     .fail
    xor     eax, eax
    ret
.fail:
    mov     eax, 1
    ret

global ASM_PFX(BemiVmClear)
ASM_PFX(BemiVmClear):
    vmclear [rcx]
    jbe     .fail
    xor     eax, eax
    ret
.fail:
    mov     eax, 1
    ret

global ASM_PFX(BemiVmPtrLd)
ASM_PFX(BemiVmPtrLd):
    vmptrld [rcx]
    jbe     .fail
    xor     eax, eax
    ret
.fail:
    mov     eax, 1
    ret

global ASM_PFX(BemiVmRead)
ASM_PFX(BemiVmRead):
    vmread  [rdx], rcx
    jbe     .fail
    xor     eax, eax
    ret
.fail:
    mov     qword [rdx], 0
    mov     eax, 1
    ret

global ASM_PFX(BemiVmWrite)
ASM_PFX(BemiVmWrite):
    vmwrite rdx, rcx
    jbe     .fail
    xor     eax, eax
    ret
.fail:
    mov     eax, 1
    ret

global ASM_PFX(BemiVmxLaunch)
ASM_PFX(BemiVmxLaunch):
    vmlaunch
    jbe     .fail
    xor     eax, eax
    ret
.fail:
    mov     eax, 1
    ret

global ASM_PFX(BemiVmxResume)
ASM_PFX(BemiVmxResume):
    vmresume
    jbe     .fail
    xor     eax, eax
    ret
.fail:
    mov     eax, 1
    ret
