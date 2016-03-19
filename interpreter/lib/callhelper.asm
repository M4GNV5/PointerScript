global ptrs_call_amd64ABI

section .text

							; https://en.wikipedia.org/wiki/X86_calling_conventions#System_V_AMD64_ABI
							; RDI, RSI, RDX, RCX, R8, R9
							; XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7
ptrs_call_amd64ABI:			; intptr_t ptrs_call_amd64ABI(void *func, int64_t *intArgv, uint8_t floatArgc, double *floatArgv);
	sub		rsp, 32
	mov		[rsp], rdi		; func
	mov		[rsp + 8], rsi	; intArgv
	mov		[rsp + 16], dl	; floatArgc
	mov		[rsp + 24], rcx	; floatArgv

	mov		rax, qword [rsp + 8]
	mov		rdi, qword [rax]
	mov		rsi, qword [rax + 8]
	mov		rdx, qword [rax + 16]
	mov		rcx, qword [rax + 24]
	mov		r8, qword [rax + 32]
	mov		r9, qword [rax + 40]
	
	mov		rax, qword [rsp + 24]
	movsd	XMM0, qword [rax]
	movsd	XMM1, qword [rax + 8]
	movsd	XMM2, qword [rax + 16]
	movsd	XMM3, qword [rax + 24]
	movsd	XMM4, qword [rax + 32]
	movsd	XMM5, qword [rax + 40]
	movsd	XMM6, qword [rax + 48]
	movsd	XMM7, qword [rax + 56]
	
	mov		rdx, qword [rsp]
	mov		al, byte [rsp + 16]
	add		rsp, 24
	call	rdx
	add		rsp, 8
	ret
	
	