; Original 8086 fixture: PC speaker ~440 Hz, OPL2 ~660 Hz, SB16 DMA 1000 Hz.
; Each tone lasts about two seconds, separated by one-second silent intervals.
bits 16
org 0x100

mov dx, active_name
call create_marker
mov cx, 18
call wait_ticks

; PIT channel 2, divisor 2712, speaker enabled.
mov al, 0xb6
out 0x43, al
mov ax, 2712
out 0x42, al
mov al, ah
out 0x42, al
in al, 0x61
mov [speaker_control], al
or al, 3
out 0x61, al
mov cx, 36
call wait_ticks
mov al, [speaker_control]
out 0x61, al
mov cx, 18
call wait_ticks

; OPL channel 0: silent modulator, audible sine carrier, sustained envelope.
mov si, opl_registers
mov cx, (opl_end - opl_registers) / 2
opl_setup:
lodsw
call opl_write
loop opl_setup
mov cx, 36
call wait_ticks
mov ax, 0x13b0                 ; key off, same block/f-number
call opl_write
mov cx, 18
call wait_ticks

; Reset SB DSP and require its ready byte.
mov dx, 0x226
mov al, 1
out dx, al
mov cx, 100
reset_delay:
in al, dx
loop reset_delay
xor al, al
out dx, al
mov cx, 0xffff
reset_ready:
mov dx, 0x22e
in al, dx
test al, 0x80
jnz reset_read
loop reset_ready
jmp failure
reset_read:
mov dx, 0x22a
in al, dx
cmp al, 0xaa
jne failure

; ISA DMA channel 1, 16-byte aligned auto-init buffer (cannot cross 64 KiB).
mov al, 5
out 0x0a, al                  ; mask channel 1
xor al, al
out 0x0c, al                  ; reset flip-flop
mov ax, ds
mov dx, ax
mov cl, 12
shr dx, cl
mov cl, 4
shl ax, cl
add ax, samples
adc dx, 0
out 0x02, al
mov al, ah
out 0x02, al
mov al, dl
out 0x83, al                  ; page
mov ax, 15
out 0x03, al
mov al, ah
out 0x03, al
mov al, 0x59                  ; single, auto-init, memory -> device, channel 1
out 0x0b, al
mov al, 1
out 0x0a, al                  ; unmask channel 1

mov al, 0xd1                  ; speaker on
call dsp_write
mov al, 0x41                  ; SB16 output sample rate 16000 Hz
call dsp_write
mov al, 0x3e
call dsp_write
mov al, 0x80
call dsp_write
mov al, 0xc6                  ; 8-bit auto-init DAC
call dsp_write
xor al, al                    ; mono unsigned
call dsp_write
mov al, 0xff                  ; long DSP block: no IRQ during this short tone
call dsp_write
mov al, 0xff
call dsp_write
mov cx, 36
call wait_ticks
mov al, 0xd0                  ; halt 8-bit DMA
call dsp_write
mov al, 0xd3                  ; speaker off
call dsp_write
mov al, 5
out 0x0a, al
mov cx, 18
call wait_ticks
mov dx, done_name
call create_marker
mov dx, active_name
mov ah, 0x41
int 0x21
mov dx, success
mov ah, 9
int 0x21
mov ax, 0x4c00
int 0x21

create_marker:
xor cx, cx
mov ah, 0x3c
int 0x21
jc failure
mov bx, ax
mov ah, 0x3e
int 0x21
ret

wait_ticks:
push ax
push bx
push cx
push dx
push si
mov si, cx                    ; INT 1Ah overwrites CX with the tick high word
xor ah, ah
int 0x1a
mov bx, dx
.wait:
xor ah, ah
int 0x1a
sub dx, bx
cmp dx, si
jb .wait
pop si
pop dx
pop cx
pop bx
pop ax
ret

; AL = register, AH = value. Include real AdLib address/data settling reads.
opl_write:
push cx
push dx
push ax
mov dx, 0x388
out dx, al
mov cx, 6
.address_delay:
in al, dx
loop .address_delay
pop ax
mov al, ah
inc dx
out dx, al
dec dx
mov cx, 35
.data_delay:
in al, dx
loop .data_delay
pop dx
pop cx
ret

dsp_write:
push ax
push cx
push dx
mov ah, al
mov dx, 0x22c
mov cx, 0xffff
.ready:
in al, dx
test al, 0x80
jz .write
loop .ready
jmp failure
.write:
mov al, ah
out dx, al
pop dx
pop cx
pop ax
ret

failure:
mov ax, 0x4c01
int 0x21

speaker_control db 0
active_name db 'ACTIVE.OK', 0
done_name db 'DONE.OK', 0
success db 'PI_DOS_AUDIO_OK', 13, 10, '$'
opl_registers:
db 0x01,0x20, 0x20,0x21, 0x23,0x21, 0x40,0x3f, 0x43,0x20
db 0x60,0xf0, 0x63,0xf0, 0x80,0x07, 0x83,0x07, 0xc0,0x00
db 0xa0,0x66, 0xb0,0x33
opl_end:
align 16, db 0
samples: times 8 db 96
         times 8 db 160
