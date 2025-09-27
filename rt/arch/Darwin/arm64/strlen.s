/*
 * Copyright (c) 2023, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

.align 4
.global elrond$strlen

//
// elrond$strlen - Return the length of a null-terminated string
//
// In:
s       .req x0     // String pointer

// Out:
//   x0: String length

// Work:
ptr     .req x9
ch      .req w10

elrond$strlen:
    stp         fp,lr,[sp,#-16]!
    mov         fp,sp

    mov         ptr,s
 loop:
    ldrb        ch,[ptr],#1
    cbnz        ch,loop

done:
    sub         x0,ptr,s
    sub         x0,x0,#1
    mov         sp,fp
    ldp         fp,lr,[sp],#16
    ret
