option casemap:none
.code

XStoreQueryGameLicenseAsync proc
    push rdi
    mov rdi, rdx
    mov rax, 0x4C56514334585035
    stosq
    mov rax, 0x52583243
    stosq ;set Store product ID
    mov rax, 0x0CCCC000001010000 ;license details not trial is valid
    stosq
    xor rax, rax 
    stosq
    mov ecx, 7  
    rep stosq
    mov eax, 0xCCCCCCCC ;fill
    stosd
    xor eax, eax
    stosd
    mov rax, 0x7FFFFFFFFFFFFFFF ;expire time QWORD
    stosq
    pop rdi
    xor eax, eax ;return 0
    ret
XStoreQueryGameLicenseAsync endp
end