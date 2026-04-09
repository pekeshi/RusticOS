; ============================================================================
; RusticOS Loader (Stage 2)
; ============================================================================
; This is the second-stage loader loaded by the bootloader at address 0x7E00.
; It loads the kernel from disk into memory and switches to 32-bit protected mode.
;
; Loader Sequence:
;   1. Initialize segment registers and stack in real mode
;   2. Display loader messages with delays
;   3. Load kernel from disk using LBA (Logical Block Addressing)
;   4. Set up Global Descriptor Table (GDT) for protected mode
;   5. Switch to 32-bit protected mode
;   6. Transfer control to kernel at 0x00001000
;
; Memory Layout:
;   0x7E00 - 0x8FFF: Loader code and data (this code)
;   0x9000:          Stack (grows downward)
;   0x1000+:         Kernel (loaded from disk)
; ============================================================================

[org 0x7E00]
[bits 16]

; ============================================================================
; Includes
; ============================================================================
%include "boot/kernel_sectors.inc"

; ============================================================================
; Loader Entry Point (16-bit Real Mode)
; ============================================================================
loader_start:
    ; Disable interrupts and clear direction flag during initialization
    cli                     ; Disable interrupts
    cld                     ; Clear direction flag (increment for string ops)
    
    ; Initialize segment registers to zero (flat memory model)
    mov ax, 0x0000
    mov ds, ax              ; Data segment
    mov es, ax              ; Extra segment
    mov ss, ax              ; Stack segment
    mov sp, 0x9000          ; Stack pointer (stack grows downward from 0x9000)
    
    ; Skip over protected mode code (we're in 16-bit mode now)
    jmp real_mode_start
    
    ; ========================================================================
    ; Protected Mode Entry Point (32-bit)
    ; ========================================================================
    ; This code is reached after we switch to protected mode via far jump.
    ; It initializes 32-bit segment selectors and jumps to the kernel.
    ; ========================================================================
    align 4                 ; Align to 4-byte boundary for protected mode code
pm_entry_32:
    bits 32                 ; Switch assembler to 32-bit mode
    
    ; Load data segment selectors (all point to flat memory at offset 0x10 in GDT)
    mov ax, 0x10            ; Data segment selector (GDT entry 2, offset 0x10)
    mov ds, ax              ; Data segment
    mov es, ax              ; Extra segment
    mov fs, ax              ; FS segment
    mov gs, ax              ; GS segment
    mov ss, ax              ; Stack segment
    
    ; Set up 32-bit stack pointer
    mov esp, 0x00089000     ; Stack pointer (below loader area)
    
    ; Far jump to kernel entry point at 0x00001000
    ; CS selector 0x08 = code segment (GDT entry 1, offset 0x08)
    jmp 0x08:0x00001000
    
    ; Unreachable - return to 16-bit mode for NASM segment tracking
    bits 16

    ; ========================================================================
    ; Real Mode Execution (16-bit)
    ; ========================================================================
real_mode_start:
    ; Re-enable interrupts (disabled during initialization)
    sti

    ; Save boot drive number passed from bootloader (in DL register)
    mov [boot_drive], dl

    ; ========================================================================
    ; Display Loader Messages
    ; ========================================================================
    mov si, msg_loader_start
    call print_string
    call delay

    mov si, msg_kernel_loading
    call print_string
    call delay

    mov si, msg_chs
    call print_string
    call delay
    
    ; Hide cursor during loading for cleaner output
    mov ah, 0x01            ; INT 10h AH=0x01: Set cursor shape
    mov ch, 0x20            ; Start scanline > end scanline = hide cursor
    mov cl, 0x00            ; End scanline
    int 0x10
    
    ; ========================================================================
    ; Initialize Disk Read Parameters
    ; ========================================================================
    mov ax, 0x0000
    mov es, ax              ; ES = destination segment (0x0000)
    xor bx, bx              ; BX = destination offset (0x0000, but DAP will override)
    
    ; Set disk geometry parameters (standard hard disk geometry)
    mov byte [sectors_per_track], 63    ; Sectors per track
    mov byte [heads], 16                ; Number of heads
    
    ; Small delay to allow disk to stabilize
    mov cx, 0xFFFF
.delay_loop:
    loop .delay_loop
    
    ; ========================================================================
    ; Load Kernel Using LBA (Logical Block Addressing)
    ; ========================================================================
    ; Kernel starts at sector 3 (LBA 3 = after bootloader (sector 0) and loader (sectors 1-2))
    ; The DAP (Disk Address Packet) structure is set up below
    mov word [dap + 8], 3       ; LBA address low word (sector 3)
    mov word [dap + 10], 0      ; LBA address high word (0 for < 2TB drives)
    
    ; Use INT 13h Extended Read (AH=0x42) with LBA addressing
    mov ah, 0x42                ; Extended Read Sectors
    mov dl, [boot_drive]        ; Drive number
    mov si, dap                 ; Pointer to Disk Address Packet
    int 0x13                    ; Call BIOS disk service
    
    ; Check for read errors
    jc .read_error
    
    ; ========================================================================
    ; Kernel Loaded Successfully
    ; ========================================================================
.kernel_loaded:
    mov si, msg_kernel_loaded
    call print_string
    call delay

    ; Proceed to protected mode setup
    jmp .kernel_valid
    
    ; ========================================================================
    ; Error Handling
    ; ========================================================================
.read_error:
    ; Display error message and halt if kernel couldn't be loaded
    mov si, msg_kernel_err
    call print_string
    hlt                         ; Halt system on critical error
    
    ; ========================================================================
    ; Enter Protected Mode
    ; ========================================================================
.kernel_valid:
    ; Display protected mode transition message
    mov si, msg_entering_pm
    call print_string
    call delay

    ; Load Global Descriptor Table (GDT) pointer
    ; The GDT defines memory segments for protected mode
    lgdt [gdt_ptr]

    ; Disable interrupts during mode switch (critical section)
    cli

    ; Enable protected mode by setting the PE (Protection Enable) bit in CR0
    mov eax, cr0               ; Load current CR0 register
    or eax, 0x00000001         ; Set bit 0 (PE bit) to enable protected mode
    mov cr0, eax               ; Write back to CR0 (protected mode is now enabled!)

    ; CRITICAL: Immediate far jump required after enabling protected mode
    ; This forces the CPU to reload CS with the protected mode selector and
    ; flush the instruction pipeline. The jump target is the 32-bit entry point.
    jmp 0x08:pm_entry_32       ; Far jump: selector 0x08 (code segment), offset pm_entry_32

; ============================================================================
; Boot Messages
; ============================================================================
msg_loader_start:   db "[LOADER] Loader started", 0x0D, 0x0A, 0
msg_kernel_loading: db "[LOADER] Loading kernel from disk...", 0x0D, 0x0A, 0
msg_chs:            db "[LOADER] Loading kernel with LBA...", 0x0D, 0x0A, 0
msg_kernel_loaded:  db "[LOADER] Kernel loaded successfully!", 0x0D, 0x0A, 0
msg_entering_pm:    db "[LOADER] Entering protected mode...", 0x0D, 0x0A, 0
msg_kernel_err:     db "[LOADER] Failed to read kernel from disk", 0x0D, 0x0A, 0

; Unused messages (kept for potential future use)
msg_loader_here:    db "[LOADER] Loader executing", 0
msg_lba:            db "[LOADER] Trying LBA read...", 0x0D, 0x0A, 0
msg_debug_sector:   db "[LOADER] Kernel starts at sector: ", 0
msg_debug_lba:      db "[LOADER] LBA address: ", 0
msg_newline:        db 0x0D, 0x0A, 0

; ============================================================================
; Helper Functions
; ============================================================================

; ----------------------------------------------------------------------------
; Print String Function
; ----------------------------------------------------------------------------
; Input:  SI = pointer to null-terminated string
; Output: String printed to screen via BIOS INT 10h
; Preserves: All registers except AX, BX, SI
; Uses the same implementation as bootloader for consistency
; ----------------------------------------------------------------------------
print_string:
    mov ah, 0x0E            ; INT 10h AH=0x0E: Teletype output (prints character)
    xor bx, bx              ; BH = page number (0), BL = color (0 = default)
.loop:
    lodsb                   ; Load byte from [SI] into AL, increment SI
    test al, al             ; Check if AL is zero (end of string)
    jz .done                ; If zero, string is done
    int 0x10                ; Print character in AL
    jmp .loop               ; Continue with next character
.done:
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
    mov ax, 0x0008          ; Outer loop counter (8 iterations for ~250ms delay)
.delay_outer:
    mov cx, 0xFFFF          ; Middle loop counter (65535 iterations)
.delay_middle:
    mov dx, 0x00FF          ; Inner loop counter (255 iterations)
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

; ----------------------------------------------------------------------------
; Print 32-bit Hexadecimal Number
; ----------------------------------------------------------------------------
; Input:  EAX = 32-bit value to print in hexadecimal
; Output: Hexadecimal representation printed to screen
; Preserves: EAX, ECX (after printing)
; Format: Prints 8 hex digits (e.g., "00001234")
; ----------------------------------------------------------------------------
print_hex_dword:
    push eax
    push ecx
    mov ecx, 8              ; 8 hex digits for 32-bit value
.hex_loop:
    rol eax, 4              ; Rotate left by 4 bits (move next nibble to low 4 bits)
    mov dl, al              ; Copy AL to DL
    and dl, 0x0F            ; Mask to get only low 4 bits (nibble)
    add dl, '0'             ; Convert to ASCII (0-9)
    cmp dl, '9'             ; Check if > 9
    jle .print_digit        ; If <= 9, it's a digit
    add dl, 7               ; Adjust for A-F (add 7 to get from '9' to 'A')
.print_digit:
    mov ah, 0x0E            ; INT 10h AH=0x0E: Teletype output
    int 0x10                ; Print character
    loop .hex_loop          ; Decrement ECX and loop if not zero
    pop ecx
    pop eax
    ret

; ----------------------------------------------------------------------------
; LBA to CHS Conversion
; ----------------------------------------------------------------------------
; Converts Logical Block Address (LBA) to Cylinder-Head-Sector (CHS) format
; Input:  AX = LBA address (logical sector number)
; Output: CH = cylinder number
;         CL = sector number (1-based)
;         DH = head number
; Preserves: BX, DX (other than output values)
; Note: This function is currently unused (we use LBA directly), but kept
;       for potential future use or debugging.
; ----------------------------------------------------------------------------
lba_to_chs:
    push bx
    push dx
    xor bx, bx
    mov bl, [sectors_per_track]     ; Load sectors per track
    xor dx, dx
    div bx                          ; Divide LBA by sectors per track
                                   ; AX = (LBA / sectors_per_track)
                                   ; DX = (LBA % sectors_per_track) = sector within track
    mov cl, dl
    inc cl                          ; CL = sector number (1-based, since sectors are 1-indexed)
    
    ; Now divide by number of heads to get cylinder and head
    xor bx, bx
    mov bl, [heads]                 ; Load number of heads
    xor dx, dx
    div bx                          ; Divide by number of heads
                                   ; AX = cylinder number
                                   ; DX = head number
    mov ch, al                      ; CH = cylinder (low 8 bits)
    mov dh, dl                      ; DH = head number
    pop dx
    pop bx
    ret

; ============================================================================
; Serial Debug Helper (Unused - Kept for Reference)
; ============================================================================
; This function provides serial port output for debugging purposes.
; Currently unused, but available if serial debugging is needed in the future.
; ============================================================================
; serial_puts:
;     push ax
;     push bx
;     push si
;     mov dx, 0x3F8              ; COM1 base port
; .s_loop:
;     lodsb                       ; Load byte from [SI]
;     test al, al                 ; Check for null terminator
;     jz .s_done
;     mov bl, al                  ; Save character
; .s_wait:
;     in al, 0x3FD                ; Read serial port status
;     test al, 0x20               ; Check if transmit buffer is empty (bit 5)
;     jz .s_wait                  ; Wait until ready
;     mov al, bl                  ; Restore character
;     out dx, al                  ; Send character to serial port
;     jmp .s_loop
; .s_done:
;     pop si
;     pop bx
;     pop ax
;     ret

; ============================================================================
; Global Descriptor Table (GDT)
; ============================================================================
; The GDT defines memory segments for protected mode operation.
; We use a flat memory model where all segments cover the entire 4GB address space.
;
; GDT Structure (each entry is 8 bytes):
;   Offset 0x00: Null descriptor (required, must be zero)
;   Offset 0x08: Code segment (executable, readable, ring 0)
;   Offset 0x10: Data segment (readable, writable, ring 0)
;
; Segment Descriptor Format:
;   Bits 0-15:   Segment Limit (bits 0-15)
;   Bits 16-31:  Base Address (bits 0-15)
;   Bits 32-39:  Base Address (bits 16-23)
;   Bits 40-47:  Access Byte (P, DPL, S, Type)
;   Bits 48-51:  Limit (bits 16-19)
;   Bits 52-55:  Flags (AVL, L, D/B, G)
;   Bits 56-63:  Base Address (bits 24-31)
; ============================================================================
gdt:
    ; GDT Entry 0: Null Descriptor (required by x86 architecture)
    ; Must be zero - used as a safety mechanism
    dq 0x0000000000000000
    
    ; GDT Entry 1 (Selector 0x08): 32-bit Code Segment
    ; Base=0x00000000, Limit=0xFFFFF (4GB with granularity)
    ; Type=Code (executable, readable), DPL=0 (kernel level), Present=1
    ; Flags: G=1 (4KB granularity), D/B=1 (32-bit), L=0 (not 64-bit)
    dw 0xFFFF           ; Limit bits 15:0 (0xFFFF)
    dw 0x0000           ; Base bits 15:0 (0x0000)
    db 0x00             ; Base bits 23:16 (0x00)
    db 0x9A             ; Access: P=1, DPL=00, S=1, Type=1010 (Code, Execute/Read)
    db 0xCF             ; Flags: G=1, D/B=1, L=0, AVL=0, Limit bits 19:16=1111
    db 0x00             ; Base bits 31:24 (0x00)
    
    ; GDT Entry 2 (Selector 0x10): 32-bit Data Segment
    ; Base=0x00000000, Limit=0xFFFFF (4GB with granularity)
    ; Type=Data (readable, writable), DPL=0 (kernel level), Present=1
    ; Flags: G=1 (4KB granularity), D/B=1 (32-bit), L=0 (not 64-bit)
    dw 0xFFFF           ; Limit bits 15:0 (0xFFFF)
    dw 0x0000           ; Base bits 15:0 (0x0000)
    db 0x00             ; Base bits 23:16 (0x00)
    db 0x92             ; Access: P=1, DPL=00, S=1, Type=0010 (Data, Read/Write)
    db 0xCF             ; Flags: G=1, D/B=1, L=0, AVL=0, Limit bits 19:16=1111
    db 0x00             ; Base bits 31:24 (0x00)

; ============================================================================
; Loader Data and Variables
; ============================================================================

; Disk geometry and loading parameters
sectors_per_track: db 63       ; Sectors per track (standard hard disk)
heads:             db 16       ; Number of heads (standard hard disk)
boot_drive:        db 0        ; Boot drive number (saved from bootloader)

; Variables for kernel loading (currently unused but kept for reference)
kernel_start_sector: dq 0x1122334455667788  ; Placeholder (patched by build script if needed)
current_lba:        dw 0                     ; Current LBA being read
remaining:          dw 0                     ; Remaining sectors to read

; ============================================================================
; Disk Address Packet (DAP) for LBA Reading
; ============================================================================
; Used with INT 13h Extended Read (AH=0x42) for reading sectors using LBA.
; This allows reading from disks larger than 8GB and is more reliable than CHS.
;
; DAP Structure (16 bytes):
;   Offset 0:  Size of DAP (0x10 = 16 bytes)
;   Offset 1:  Reserved (must be 0)
;   Offset 2:  Number of sectors to read
;   Offset 4:  Destination offset (where to load in memory)
;   Offset 6:  Destination segment
;   Offset 8:  LBA address (64-bit, low 32 bits)
;   Offset 12: LBA address (64-bit, high 32 bits)
; ============================================================================
dap:
    db 0x10                ; Size of DAP (16 bytes)
    db 0x00                ; Reserved/unused (must be 0)
    dw KERNEL_SECTORS      ; Number of sectors to read (from kernel_sectors.inc)
    dw 0x1000              ; Destination offset in segment (0x1000 = where kernel loads)
    dw 0x0000              ; Destination segment (0x0000)
    dq 0x0000000000000000  ; LBA address (64-bit) - set at runtime to sector 3

; ============================================================================
; GDT Pointer (for LGDT instruction)
; ============================================================================
; The LGDT instruction loads the GDT from this structure.
; Format: 2-byte limit, 4-byte base address
; ============================================================================
gdt_ptr:
    dw 0x0017              ; GDT limit (size - 1): 3 entries * 8 bytes - 1 = 23 = 0x0017
    dd gdt                 ; GDT base address (32-bit pointer to gdt)

gdt_end:                   ; Label marking end of GDT (for reference)
