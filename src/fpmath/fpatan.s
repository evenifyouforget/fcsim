.global fpatan

fpatan:
    movsd    %xmm0, -16(%rsp)
    fldl    -16(%rsp)
    movsd    %xmm1, -16(%rsp)
    fldl    -16(%rsp)
    fpatan
    fstpl    -16(%rsp)
    movsd    -16(%rsp), %xmm0
    ret

/* tell the linker that we don't want an executable stack */
.section .note.GNU-stack,"",@progbits