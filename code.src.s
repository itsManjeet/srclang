.intel_syntax noprefix
.data
.L.data.0:
  .byte 72
  .byte 101
  .byte 108
  .byte 108
  .byte 111
  .byte 32
  .byte 87
  .byte 111
  .byte 114
  .byte 108
  .byte 100
  .byte 10
  .byte 0
EXIT_FAILURE:
  .4byte 10
.text
.global main
main:
  push rbp
  mov rbp, rsp
  sub rsp, 0
  push offset .L.data.0
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .Lcall1
  mov rax, 0
  call printf
  jmp .Lend1
.Lcall1:
  sub rsp, 8
  mov rax, 0
  call printf
  add rsp, 8
.Lend1:
  push rax
  pop rax
  movsxd rax, eax
  push rax
  add rsp, 8
  push offset EXIT_FAILURE
  pop rax
  movsxd rax, dword ptr [rax]
  push rax
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .Lcall2
  mov rax, 0
  call exit
  jmp .Lend2
.Lcall2:
  sub rsp, 8
  mov rax, 0
  call exit
  add rsp, 8
.Lend2:
  push rax
  pop rax
  movsxd rax, eax
  push rax
  add rsp, 8
.Lreturn.main:
  mov rsp, rbp
  pop rbp
  ret
