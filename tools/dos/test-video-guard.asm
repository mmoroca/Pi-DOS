bits 16
org 100h
; Count BIOS text presentation and mode resets around the real setup EXE.
; Queue a test key after one frame has been presented, without host SDL hooks.
%ifndef PAGE
%define PAGE 0
%endif
%ifndef KEY
%define KEY 011bh
%endif
%ifndef FIRSTBOOT
%define FIRSTBOOT 1
%endif
start:
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,stack_top
    mov bx,(stack_top-$$+100h+15)/16
    mov ah,4ah
    int 21h
    jc failed
    mov ax,3
    int 10h
    mov ax,0500h+PAGE
    int 10h
    mov ax,3510h
    int 21h
    mov [old_vector],bx
    mov [old_vector+2],es
    mov dx,handler
    mov ax,2510h
    int 21h
    mov ax,cs
    mov es,ax
    mov [params+4],ax
    mov [params+8],ax
    mov [params+12],ax
    mov bx,params
    mov dx,program
    mov ax,4b00h
    int 21h
    jc failed
    mov ah,4dh
    int 21h
    mov [child_exit],al
    mov ah,0fh
    int 10h
    mov [final_mode],al
    mov [final_page],bh
    mov [final_cols],ah
    mov dx,[old_vector]
    mov ds,[old_vector+2]
    mov ax,2510h
    int 21h
    push cs
    pop ds
    mov dx,filename
    xor cx,cx
    mov ah,3ch
    int 21h
    jc failed
    mov bx,ax
    mov dx,counter
    mov cx,8
    mov ah,40h
    int 21h
    mov ah,3eh
    int 21h
    mov ax,4c00h
    int 21h
failed:
    mov ax,4c01h
    int 21h
handler:
    pushf
    cmp ah,13h
    jne check_mode
    inc word [cs:bios_writes]
    cmp word [cs:bios_writes],25
    jne check_mode
    push ax
    push cx
    mov ah,5
    mov cx,KEY
    int 16h
    pop cx
    pop ax
check_mode:
    cmp ah,0
    jne chain
    inc word [cs:counter]
chain:
    popf
    jmp far [cs:old_vector]
old_vector: dd 0
counter: dw 0
final_mode: db 0
final_page: db 0
final_cols: db 0
child_exit: db 0
bios_writes: dw 0
params: dw 0,tail,0,5ch,0,6ch,0
tail: db tail_end-tail-2
%if FIRSTBOOT
    db ' /FIRSTBOOT',13
%else
    db 13
%endif
tail_end:
program: db 'PIDOSCFG.EXE',0
filename: db 'MODE.OUT',0
    times 512 db 0
stack_top:
