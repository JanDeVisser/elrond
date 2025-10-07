.align 4

.global elrond$putln
.global _elrond$putln
.globl elrond$putln
.globl _elrond$putln

//
// putln - Print string followed by a newline character
//
// In:
buffer  .req x0     // Pointer to the string buffer
len     .req w1     // Length of the string

// Out:
//   x0: Number of characters printed.

// Work:
//   x7 - characters printed
//   x16 - syscall

elrond$putln:
_elrond$putln:
    stp     fp,lr,[sp,#-16]!
    mov     fp,sp

    bl      elrond$puts
    mov     x7,x0
    bl      elrond$endln
    add     x0,x7,x0
    ldp     fp,lr,[sp],#16
    ret
