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