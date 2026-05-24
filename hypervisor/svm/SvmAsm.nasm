BITS 64
DEFAULT REL

SECTION .text

global ASM_PFX(BemiSvmVmRun)
ASM_PFX(BemiSvmVmRun):
    mov     rax, [rcx]
    vmrun
    xor     eax, eax
    ret

global ASM_PFX(BemiSvmVmSave)
ASM_PFX(BemiSvmVmSave):
    mov     rax, [rcx]
    vmsave
    xor     eax, eax
    ret

global ASM_PFX(BemiSvmVmLoad)
ASM_PFX(BemiSvmVmLoad):
    mov     rax, [rcx]
    vmload
    xor     eax, eax
    ret

global ASM_PFX(BemiSvmStgi)
ASM_PFX(BemiSvmStgi):
    stgi
    xor     eax, eax
    ret

global ASM_PFX(BemiSvmClgi)
ASM_PFX(BemiSvmClgi):
    clgi
    xor     eax, eax
    ret

global ASM_PFX(BemiSvmInvlpga)
ASM_PFX(BemiSvmInvlpga):
    mov     rax, rcx
    mov     ecx, edx
    invlpga
    xor     eax, eax
    ret
