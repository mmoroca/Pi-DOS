; Original 8086 fixture: record nine BIOS key words and three DOS characters.
; The host waits for READY.OK before sending events through Linux uinput.
bits 16
org 0x100
cld

drain:
mov ah, 1
int 0x16
jz ready
xor ah, ah
int 0x16
jmp drain
ready:
mov dx, ready_name
xor cx, cx
mov ah, 0x3c
int 0x21
jc failure
mov bx, ax
mov ah, 0x3e
int 0x21

mov di, keys
mov cx, 9
bios_key:
xor ah, ah
int 0x16
stosw
loop bios_key
mov cx, 3
dos_key:
mov ah, 8
int 0x21
stosb
loop dos_key

mov dx, output_name
xor cx, cx
mov ah, 0x3c
int 0x21
jc failure
mov bx, ax
mov dx, keys
mov cx, 21
mov ah, 0x40
int 0x21
jc failure
cmp ax, 21
jne failure
mov ah, 0x3e
int 0x21
mov dx, success
mov ah, 9
int 0x21
mov ax, 0x4c00
int 0x21
failure:
mov ax, 0x4c01
int 0x21

ready_name db 'READY.OK', 0
output_name db 'KEYS.BIN', 0
success db 'ARMDOS_KEYBOARD_OK', 13, 10, '$'
keys times 21 db 0
