
option casemap:none
.code
XStoreQueryGameLicenseResult proc
    push rdi
    mov rdi, rdx
    mov rax, 04C56513458355039h    ; Hexadecimal literal with leading `0` and `h` suffix
    stosq
    mov rax, 052583243h            ; Hexadecimal literal for Store product ID
    stosq
    mov rax, 0CCCC000001010000h    ; License details: not trial, is valid
    stosq
    xor rax, rax 
    stosq
    mov ecx, 7                     ; Repeat next `stosq` 7 times
    rep stosq
    mov eax, 0CCCCCCCCh            ; Fill data with commonly used pattern
    stosd
    xor eax, eax
    stosd
    mov rax, 07FFFFFFFFFFFFFFFh    ; Expire time QWORD
    stosq
    pop rdi
    xor eax, eax                   ; Return 0
    ret
XStoreQueryGameLicenseResult endp
end
