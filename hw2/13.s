mov eax, [0x600004]
neg eax
mov ebx, [0x600008]
xor edx, edx
cdq
idiv ebx
mov ecx, edx
mov eax, [0x600000]
imul eax, -5
xor edx, edx
cdq
idiv ecx
mov [0x60000c], eax
