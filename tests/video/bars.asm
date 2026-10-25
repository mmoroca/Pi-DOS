; Original 8086 mode-13h fixture: eight DAC color bars and a changing corner pixel.
; READY.OK brackets graphics; hold about 3 seconds after capture acknowledgment.
; A separate BIOS-tick deadline exits if the diagnostic never acknowledges.
bits 16
org 0x100
cld

mov ax, 0x0013
int 0x10
mov dx, 0x3c8
mov al, 1
out dx, al
inc dx
mov si, palette
mov cx, 24
palette_loop:
lodsb
out dx, al
loop palette_loop

mov ax, 0xa000
mov es, ax
xor di, di
mov dx, 200
row:
mov al, 1
bar:
mov cx, 40
rep stosb
inc al
cmp al, 9
jb bar
dec dx
jnz row

xor ah, ah
int 0x1a
mov bp, dx
settle:
xor ah, ah
int 0x1a
mov ax, dx
and al, 1
inc al
mov [es:0], al
sub dx, bp
cmp dx, 9
jb settle
mov dx, ready
xor cx, cx
mov ah, 0x3c
int 0x21
jc failure
mov bx, ax
mov ah, 0x3e
int 0x21
xor ah, ah
int 0x1a
mov bp, dx
wait_frame:
xor ah, ah
int 0x1a
mov si, dx
mov ax, dx
and al, 1
inc al
mov [es:0], al
cmp byte [acked], 0
jne hold
mov dx, acknowledged
mov ax, 0x4300
int 0x21
jc deadline
mov byte [acked], 1
mov [ack_tick], si
hold:
mov dx, si
sub dx, [ack_tick]
cmp dx, 54
jae finished
deadline:
mov dx, si
sub dx, bp
cmp dx, 252
jb wait_frame
jmp failure
finished:
mov dx, ready
mov ah, 0x41
int 0x21
mov ax, 3
int 0x10
mov dx, success
mov ah, 9
int 0x21
mov ax, 0x4c00
int 0x21
failure:
mov ax, 3
int 0x10
mov ax, 0x4c01
int 0x21

ready db 'READY.OK', 0
acknowledged db 'CAPTURE.OK', 0
acked db 0
ack_tick dw 0
success db 'PI_DOS_GRAPHICS_OK', 13, 10, '$'
palette db 63,0,0, 0,63,0, 0,0,63, 63,63,0
        db 63,0,63, 0,63,63, 63,63,63, 32,32,32
