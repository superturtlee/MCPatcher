;directly go into the callback
push    rdi                         ; 57
sub     rsp, 20h                    ; 48 83 EC 20
mov     rdi, rdx                    ; 48 89 D7
xor     rax, rax                    ; 48 31 C0
mov     qword ptr [rdi+18h], 0      ; 48 C7 47 18 00 00 00 00
mov     rcx, rdi                    ; 48 89 F9
mov     rax, [rdi+10h]              ; 48 8B 47 10
test    rax, rax                    ; 48 85 C0
jz      short skip                  ; 74 02
call    rax;                         ; FF D0
skip:
xor     eax, eax                    ; 31 C0
add     rsp, 20h                    ; 48 83 C4 20
pop     rdi                         ; 5F
ret                                 ; C3
