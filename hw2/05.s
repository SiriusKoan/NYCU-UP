xor ebx, ebx
L:
    mov r8d, eax
    mov ecx, 15
    sub ecx, ebx
    shr r8d, cl
    and r8d, 1
    add r8d, 0x30
    mov byte ptr [0x600000 + rbx], r8b
    inc ebx
    cmp ebx, 16
    jne L
