.global ptrs_alloca
.type ptrs_alloca, %function
ptrs_alloca:
	/*align on 16 byte boundary*/
	dec %rdi
	and $-16, %rdi
	add $16, %rdi

	/*store return address and allocate stack memory*/
	mov (%rsp), %rcx
	sub %rdi, %rsp
	mov %rsp, %rax
	jmp *(%rcx)
