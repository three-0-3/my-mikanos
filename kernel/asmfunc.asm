; asmfunc.asm
;
; System V AMD64 Calling Convention
; Registers: RDI, RSI, RDX, RCX, R8, R9

bits 64
section .text

global IoOut32   ; void IoOut32(uint16_t addr, uint32_t data);
IoOut32:
	mov dx, di     ; dx = addr
	mov eax, esi   ; eax = data
	out dx, eax
	ret

global IoIn32    ; uint32_t IoIn32(uint16_t addr);
IoIn32:
	mov dx, di 		 ; dx = addr
	in eax, dx
	ret

global GetCS     ; uint16_t GetCS(void);
GetCS:
  mov ax, cs
	ret

global LoadIDT        ; void LoadIDT(uint16_t limit, uint64_t offset);
LoadIDT:
	push rbp						; save the base pointer
	mov rbp, rsp				; save the current stack pointer in rbp
  sub rsp, 10
	mov [rsp], di       ; limit
	mov [rsp + 2], rsi  ; offset
	lidt [rsp]
	mov rsp, rbp				; load the stack pointer from rbp
	pop rbp							; load the base pointer
	ret

global LoadGDT				; void LoadGDT(uint16_t limit, uint64_t offset);
LoadGDT:
	push rbp
	mov rbp, rsp
	sub rsp, 10
	mov [rsp], di       ; limit
	mov [rsp + 2], rsi  ; offset
	lgdt [rsp]
	mov rsp, rbp				; load the stack pointer from rbp
	pop rbp							; load the base pointer
	ret

global SetDSAll				; void SetDSAll(uint16_t value);
SetDSAll:
	mov ds, di
	mov es, di
	mov fs, di
	mov gs, di
	ret

global SetCSSS				; void SetCSSS(uint16_t cs, uint16_t ss);
SetCSSS:
	push rbp
	mov rbp, rsp
	mov ss, si					; ss
	mov rax, .next			; to continue from .next after retf
	push rdi						; cs
	push rax						; rip
	o64 retf						; use retf as mov cannot be used to set cs
.next:
	mov rsp, rbp
	pop rbp
	ret

global SetCR3         ; void SetCR3(uint64_t value)
SetCR3:
	mov cr3, rdi				; value
	ret

extern kernel_main_stack
extern KernelMainNewStack

global KernelMain
KernelMain:
	mov rsp, kernel_main_stack + 1024 * 1024	; set the bottom address for new stack
	call KernelMainNewStack
.fin:																				; IP never returns here but just in case
	hlt
	jmp .fin