; Original Pi-DOS fixture: MZ EXE with one load-segment relocation.
bits 16
org 0

dw 0x5a4d                         ; MZ signature
dw (file_end - $$) % 512         ; bytes in final page
dw (file_end - $$ + 511) / 512   ; file pages
dw 1                            ; relocation count
dw (payload - $$) / 16          ; header paragraphs
dw 16                           ; extra paragraphs for the stack
dw 0xffff                       ; maximum extra allocation
dw 0                            ; initial SS relative to load segment
dw (file_end - payload) + 256    ; initial SP
dw 0                            ; checksum (unused)
dw entry - payload              ; initial IP
dw 0                            ; initial CS relative to load segment
dw 28                           ; relocation table offset in header
dw 0                            ; overlay number
dw relocated_segment - payload ; relocation offset within load segment
dw 0                            ; relocation segment

payload:
entry:
mov ax, [cs:relocated_segment - payload]
mov ds, ax
mov dx, message - payload
mov ah, 0x09
int 0x21
mov ax, 0x4c0b
int 0x21

relocated_segment dw 0           ; loader must add the actual load segment
message db 'ARMDOS_EXE_OK', 13, 10, '$'
file_end:
