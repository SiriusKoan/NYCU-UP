check_eax:
    cmp eax, 0
    jge positive_a
    mov dword ptr [0x600000], -1
    jmp check_ebx
positive_a:
    mov dword ptr [0x600000], 1
check_ebx:
    cmp ebx, 0
    jge positive_b
    mov dword ptr [0x600004], -1
    jmp check_ecx
positive_b:
    mov dword ptr [0x600004], 1
check_ecx:
    cmp ecx, 0
    jge positive_c
    mov dword ptr [0x600008], -1
    jmp check_edx
positive_c:
    mov dword ptr [0x600008], 1
check_edx:
    cmp edx, 0
    jge positive_d
    mov dword ptr [0x60000c], -1
    jmp end
positive_d:
    mov dword ptr [0x60000c], 1
end:
