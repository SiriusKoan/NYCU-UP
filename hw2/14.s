mov eax, [0x600000]
mov ecx, [0x600004]
neg ecx
imul eax, ecx

mov ecx, [0x600008]
sub ecx, ebx

xor edx, edx
cdq
idiv ecx
mov [0x600008], eax
