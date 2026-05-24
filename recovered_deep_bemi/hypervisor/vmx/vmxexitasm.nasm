BITS 64
DEFAULT REL

SECTION .text

extern ASM_PFX(VmxExitDispatch)

global ASM_PFX(BemiVmxExitTrampoline)
ASM_PFX(BemiVmxExitTrampoline):
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rcx, rsp

    sub     rsp, 32

    call    ASM_PFX(VmxExitDispatch)

    add     rsp, 32

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    vmresume
    jbe     .resume_fail
    xor     eax, eax
    ret
.resume_fail:
    sub     rsp, 8
    mov     ecx, 0x00004400
    vmread  [rsp], rcx
    add     rsp, 8
    mov     eax, 1
    ret
