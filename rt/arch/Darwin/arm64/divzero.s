/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

//
// _$divide_by_zero
//
// In: -
// Out: process aborted with SIGABRT
//
			
.global _$divide_by_zero
_$divide_by_zero:
	adr     x1,__str_divide_by_zero
	mov     x2,str_divide_by_zero_len
	mov	x0,#1
	mov     x16,#0x04
	svc     #0x00
	mov     x0,xzr      // 0- this process
	mov	x1,#6       // SIGABRT
	mov     x16,#0x25   // kill syscall
	svc     #0x00
	
__str_divide_by_zero:
    .string "Division by zero. Process aborted.\n"

.equ str_divide_by_zero_len,29
