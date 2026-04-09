; ============================================================================
; RusticOS Bootloader (Stage 1)
; ============================================================================
; This is the first-stage bootloader loaded by BIOS at address 0x7C00.
; It loads the second-stage loader from disk and transfers control to it.
;
; Boot Sequence:
;   1. Initialize segment registers and stack
;   2. Display boot messages with delays
;   3. Recalibrate disk drive
;   4. Load loader from sector 2 into memory at 0x7E00
;   5. Transfer control to loader via far jump
;
; Memory Layout:
;   0x7C00 - 0x7DFF: Bootloader (512 bytes, this code)
;   0x7E00+:        Loader (loaded from disk)
; ============================================================================

[org 0x7c00]
[bits 16]

; ============================================================================
; Includes
; ============================================================================
%include "boot/kernel_sectors.inc"
%include "boot/loader_sectors.inc"

%ifndef LOADER_SECTORS
%assign LOADER_SECTORS 1
%endif

; ============================================================================
; Bootloader Entry Point
; ============================================================================
start:
    ; Disable interrupts during initialization
    cli
    
    ; Initialize segment registers to zero (real mode flat memory)
    xor ax, ax
    mov ds, ax          ; Data segment
    mov es, ax          ; Extra segment
    mov ss, ax          ; Stack segment
    mov sp, 0x7000      ; Stack pointer (stack grows downward from 0x7000)
    
    ; Re-enable interrupts
    sti

    ; Display boot start message
    mov si, msg_start
    call print_string
    call delay

    ; Save boot drive number (BIOS passes it in DL)
    ; DL = 0x00 for floppy, 0x80 for hard drive
    mov [boot_drive], dl

    ; Display loading message
    mov si, msg_loading
    call print_string
    call delay

    ; ========================================================================
    ; Load Second-Stage Loader from Disk
    ; ========================================================================
    ; Use CHS (Cylinder-Head-Sector) addressing to read loader
    ; Loader is located at sector 2 (LBA 1) on the boot disk
    
    ; Recalibrate the drive (ensures drive heads are positioned correctly)
    mov ah, 0x00            ; INT 13h AH=0x00: Reset/Recalibrate drive
    mov dl, 0x80            ; Drive number (0x80 = first hard drive)
    int 0x13
    
    ; Set up destination address in memory (ES:BX = 0x0000:0x7E00)
    mov ax, 0x0000
    mov es, ax              ; ES = segment (0x0000)
    mov bx, 0x7E00          ; BX = offset (0x7E00)
    
    ; Read sectors from disk using INT 13h AH=0x02 (Read Sectors)
    mov ah, 0x02            ; Function: Read sectors from drive
    mov al, LOADER_SECTORS  ; Number of sectors to read
    mov ch, 0               ; Cylinder number (0)
    mov cl, 2               ; Sector number (2 = LBA 1, 1-indexed)
    mov dh, 0               ; Head number (0)
    mov dl, 0x80            ; Drive number (0x80 = first hard drive)
    int 0x13                ; Call BIOS disk service
    
    ; Check for errors (carry flag set on error)
    jc .load_error

    ; ========================================================================
    ; Load Success - Transfer Control to Loader
    ; ========================================================================
.load_success:
    ; Display success message
    mov si, msg_success
    call print_string
    call delay

    ; Ensure data segment is correct before transfer
    xor ax, ax
    mov ds, ax

    ; Disable interrupts during control transfer
    cli

    ; Transfer control to loader via far jump (far return)
    ; Push segment first (0x0000), then offset (0x7E00)
    ; retf will pop both and jump to loader at 0x7E00
    push 0x0000     ; Segment
    push 0x7E00     ; Offset
    retf            ; Far return = far jump to loader
    
    ; Should never reach here
    cli
    hlt

    ; ========================================================================
    ; Error Handling
    ; ========================================================================
.load_error:
    ; If disk read failed, halt the system
    ; In a production system, you might want to display an error message
    cli
    hlt

; ============================================================================
; Helper Functions
; ============================================================================

; ----------------------------------------------------------------------------
; Print String Function
; ----------------------------------------------------------------------------
; Input:  SI = pointer to null-terminated string
; Output: String printed to screen via BIOS INT 10h
; Preserves: All registers except AX, BX, SI
; ----------------------------------------------------------------------------
print_string:
    mov ah, 0x0E            ; INT 10h AH=0x0E: Teletype output (prints character)
    xor bx, bx              ; BH = page number (0), BL = color (0 = default)
.ps_loop:
    lodsb                   ; Load byte from [SI] into AL, increment SI
    test al, al             ; Check if AL is zero (end of string)
    jz .ps_done             ; If zero, string is done
    int 0x10                ; Print character in AL
    jmp .ps_loop            ; Continue with next character
.ps_done:
    ret                     ; Return to caller

; ============================================================================
; Delay Function
; ============================================================================
; Provides a visible delay for boot messages (approximately 250ms)
; Uses triple-nested loops to create a consistent delay across different systems
; Preserves all registers
; ============================================================================
delay:
    push ax
    push cx
    push dx
    mov ax, 0x0008      ; Outer loop counter (8 iterations for ~250ms delay)
.delay_outer:
    mov cx, 0xFFFF      ; Middle loop counter (65535 iterations)
.delay_middle:
    mov dx, 0x00FF      ; Inner loop counter (255 iterations)
.delay_inner:
    dec dx
    jnz .delay_inner
    dec cx
    jnz .delay_middle
    dec ax
    jnz .delay_outer
    pop dx
    pop cx
    pop ax
    ret

; ============================================================================
; Boot Messages
; ============================================================================
msg_start:          db "[BOOT] Bootloader started", 0x0D, 0x0A, 0
msg_loading:        db "[BOOT] Loading kernel loader...", 0x0D, 0x0A, 0
msg_success:        db "[BOOT] Loader loaded successfully!", 0x0D, 0x0A, 0

; ============================================================================
; Bootloader Data
; ============================================================================
boot_drive:         db 0        ; Drive number saved from BIOS (DL register)

; ============================================================================
; Bootloader Padding and Boot Signature
; ============================================================================
; The bootloader must be exactly 512 bytes. Fill remaining space with zeros,
; then add the boot signature (0xAA55) at the end. BIOS checks for this
; signature to determine if the disk is bootable.
times (512 - 2 - ($-$$)) db 0
dw 0xAA55                       ; Boot signature (little-endian: 55 AA on disk)