; ####### $BOOT
org 7c00h
start:
cli
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 07c00h
sti
mov al, 3
int 10h
mov dh, 0
mov dl, 0
xor bh, bh
int 10h
push 0x50
pop es
mov dx, 0; disk A, head 0 (dl - disk, dh - head)
mov ch, 0; 10bit - cyl, 6bit - sector
mov cl, 2; sector
mov al, 7
xor bx, bx
mov ah, 02h
int 13h
mov dl, 0
mov dh, 1
mov bx, 0x0E00
mov ch, 0
mov cl, 1
mov al, 8
mov ah, 02h
int 13h
jmp 0080:0000
jmp $
fd db 'error!'
times 510 - ($ - 7c00h) db 0
db 055h, 0AAh; ƒ¿À‹ÿ≈ «¿√–”«◊»  Õ≈ »ƒ®“
ORG 500h
jmp HOME
var_it dd 0
var_RAM:
 db 82, 97, 109, 111, 110, 116, 101, 114, 33, 0

; ####### $MEAN WIDTH_T 80
; ####### func dword len : dword Str do:
fnv_len_i dd 0
fnv_len_Str dd 0
fnv_len_len dd 0
fnc_len:
; ####### init dword i = 0
; ####### until *(Str + i) = 0 do:
til_1:
mov ebx, [fnv_len_i]
mov eax, [fnv_len_Str]
add eax, ebx
push eax
xor ebx, ebx
pop eax
xor edx, edx
cmp eax, ebx
mov eax, edx
jne cnd_0_1
inc al
cnd_0_1:
test eax, eax
jnz tle_1
; ####### inc i
inc [fnv_len_i]
; ####### end.
jmp til_1
tle_1:
; ####### equ len = i
mov eax, [fnv_len_i]
mov [fnv_len_len], eax
; ####### end.
ret
; ####### HOME: 
HOME:
; ####### init str byte RAM = "Ramonter!\0"
; ####### init dword it
; ####### equ it = 80 * len {*RAM}
xor eax, eax
mov al, [var_RAM]
mov di, ax
xor ax, ax
shr eax, 4
push es, ax
mov eax, [es:di]
push [fnv_len_Str]
mov [fnv_len_Str], eax
call fnc_len
mov ebx, [fnv_len_len]
pop [fnv_len_Str]
mov eax, 80
mul ebx
mov [var_it], eax
; ####### HEND.
hlt
HEND:
