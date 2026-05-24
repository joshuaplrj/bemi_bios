BITS 64
DEFAULT REL

SECTION .text

extern ASM_PFX(SvmExitDispatch)

global ASM_PFX(BemiSvmExitTrampoline)
ASM_PFX(BemiSvmExitTrampoline):
    ; Save guest general purpose registers
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

    mov     rcx, rsp        ; Arg1: GuestGprBase pointer
    ; In SVM, the VMCB address is usually passed or stored.
    ; We can pass 0 for now or derive it if needed.
    xor     rdx, rdx        ; Arg2: VmcbBase (NULL for now)

    sub     rsp, 32         ; Shadow space for x64 ABI
    call    ASM_PFX(SvmExitDispatch)
    add     rsp, 32

    ; Restore registers
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

    clgi                    ; Disable interrupts
    vmrun                   ; Re-enter guest
    jmp     ASM_PFX(BemiSvmExitTrampoline) ; In case vmrun fails
