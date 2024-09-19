mov rdi, 24
call r
jmp exit
r:
  push r12
  mov r12d, 1
  push rbp
  xor ebp, ebp
  push rbx
  mov rbx, rdi
.L3:
  test rbx, rbx
  je .L2
  cmp rbx, 1
  je .L4
  lea rdi, [rbx-1]
  sub rbx, 2
  call r
  imul rax, r12
  lea r12, [r12+r12*2]
  add rax, rax
  add rbp, rax
  jmp .L3
.L4:
  mov rbx, r12
.L2:
  lea rax, [rbp+0+rbx]
  pop rbx
  pop rbp
  pop r12
  ret
exit:
