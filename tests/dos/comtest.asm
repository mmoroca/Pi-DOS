; Original Pi-DOS fixture: 8086 COM program, DOS output and exit status.
bits 16
org 0x100

mov dx, message
mov ah, 0x09
int 0x21
mov ax, 0x4c07
int 0x21

message db 'PI_DOS_COM_OK', 13, 10, '$'
