# ============================================================================
# RusticOS - 32-bit Kernel Startup (crt0.s)
# ============================================================================
# 
# This file contains the kernel entry point and interrupt handling setup.
# It is the first code executed after the loader transfers control to the kernel.
#
# Responsibilities:
#   - Set up kernel stack
#   - Initialize Interrupt Descriptor Table (IDT)
#   - Set up interrupt service routines (ISRs) for exceptions and IRQs
#   - Load IDT and enable interrupt handling infrastructure
#   - Call global constructors and initialization arrays
#   - Transfer control to kernel_main() (C++ entry point)
#
# Version: 1.6.9
# ============================================================================

.global _start
.global idt_flush
.extern kernel_main
.extern exception_handler
.extern irq_handler
.extern init_idt

.section .text
.code32

# ============================================================================
# Kernel Entry Point (_start)
# ============================================================================
# This is the first instruction executed after the loader jumps to the kernel.
# The loader has already:
#   - Switched to 32-bit protected mode
#   - Loaded the GDT (Global Descriptor Table)
#   - Set up segment selectors
#   - Loaded the kernel code to address 0x00001000
# ============================================================================
_start:
    # Disable interrupts during kernel initialization
    cli
    
    # Debug marker: Write 'R' to VGA buffer to confirm kernel entry
    # This helps verify the kernel loaded correctly during development
    movl $0xb8000, %edi
    movl $0x1f52, %eax   # 'R' character (0x52) with white text (0x1F attribute)
    movl %eax, (%edi)
    
    # GDT (Global Descriptor Table) already loaded by loader
    # Segment registers already set up by loader in protected mode
    # We're already in 32-bit protected mode, so we can proceed

    # ========================================================================
    # Set Up Kernel Stack
    # ========================================================================
    # Stack grows downward from high addresses to low addresses
    # Kernel code is loaded at 0x00001000, so we place the stack below it
    # Stack pointer: 0x00088000 (gives us plenty of stack space)
    movl $0x00088000, %esp
    andl $~0xF, %esp          # Align stack to 16-byte boundary (required by ABI)

    # movl $0xb800A, %edi
    # movl $0x1f42, %eax   # B in green
    # movl %eax, (%edi)

    # Clear .bss
    # cld
    # movl $__bss_start, %edi
    # movl $__bss_end, %ecx
    # subl %edi, %ecx
    # jbe .after_bss
    # xorl %eax, %eax
    # rep stosb
# .after_bss:

    # movl $0xb8002, %edi
    # movl $0x1f4d, %eax   # D in green
    # movl %eax, (%edi)

    # movl $0xb800C, %edi
    # movl $0x1f4D, %eax   # M in green
    # movl %eax, (%edi)

    # Call constructors
    movl $__ctors_start, %esi
    movl $__ctors_end, %edi
.ctors_loop:
    cmpl %edi, %esi
    jge .ctors_done
    call *(%esi)
    addl $4, %esi
    jmp .ctors_loop
.ctors_done:

    # Call init_array
    movl $__init_array_start, %esi
    movl $__init_array_end, %edi
.init_loop:
    cmpl %edi, %esi
    jge .init_done
    call *(%esi)
    addl $4, %esi
    jmp .init_loop
.init_done:

    # QUICK VGA TEST: write a visible title directly to VGA memory
    # This forces visible text even if higher-level code has an issue.
    #movl $0xb8000, %edi
    #movb $'R', (%edi)
    #movb $0x20, 1(%edi)
    #addl $2, %edi
    #movb $'u', (%edi)
    #movb $0x20, 1(%edi)
    #addl $2, %edi
    #movb $'s', (%edi)
    #movb $0x20, 1(%edi)
    #addl $2, %edi
    #movb $'t', (%edi)
    #movb $0x20, 1(%edi)
    #addl $2, %edi
    #movb $'i', (%edi)
    #movb $0x20, 1(%edi)
    #addl $2, %edi
    #movb $'c', (%edi)
    #movb $0x20, 1(%edi)
    #addl $2, %edi
    #movb $'O', (%edi)
    #movb $0x20, 1(%edi)
    #addl $2, %edi
    #movb $'S', (%edi)
    #movb $0x20, 1(%edi)

    # Initialize IDT (this sets up the interrupt descriptor table)
    call init_idt
    
    # Load IDT
    lea idt_ptr, %eax
    lidt (%eax)
    
    call kernel_main

.hang:
    hlt
    jmp .hang

# ============================================================================
# Interrupt Descriptor Table (IDT) Setup
# ============================================================================

# IDT (exported so C++ code can access it)
.section .data
.align 8
.global idt
idt:
    .space 256*8, 0
.global idt_ptr
idt_ptr:
    .word (256*8 - 1)
    .long idt

# ============================================================================
# Interrupt Service Routines (ISRs)
# ============================================================================

# Macro to create ISR stub without error code
.macro ISR_NOERRCODE num
    .global isr\num
    isr\num:
        cli
        pushl $0              # Push dummy error code
        pushl $\num           # Push interrupt number
        jmp isr_common_stub
.endm

# Macro to create ISR stub with error code
.macro ISR_ERRCODE num
    .global isr\num
    isr\num:
        cli
        pushl $\num           # Push interrupt number (error code already on stack)
        jmp isr_common_stub
.endm

# Macro to create IRQ stub
.macro IRQ num, irq_num
    .global irq\num
    irq\num:
        cli
        pushl $0              # Push dummy error code
        pushl $\irq_num       # Push IRQ number
        jmp irq_common_stub
.endm

# Create ISR stubs for exceptions (0-31)
ISR_NOERRCODE 0   # Divide by Zero
ISR_NOERRCODE 1   # Debug
ISR_NOERRCODE 2   # Non-Maskable Interrupt
ISR_NOERRCODE 3   # Breakpoint
ISR_NOERRCODE 4   # Overflow
ISR_NOERRCODE 5   # Bound Range Exceeded
ISR_NOERRCODE 6   # Invalid Opcode
ISR_NOERRCODE 7   # Device Not Available
ISR_ERRCODE 8     # Double Fault
ISR_NOERRCODE 9   # Coprocessor Segment Overrun
ISR_ERRCODE 10    # Invalid TSS
ISR_ERRCODE 11    # Segment Not Present
ISR_ERRCODE 12    # Stack Fault
ISR_ERRCODE 13    # General Protection Fault
ISR_ERRCODE 14    # Page Fault
ISR_NOERRCODE 15  # Reserved
ISR_NOERRCODE 16  # x87 FPU Floating-Point Error
ISR_ERRCODE 17    # Alignment Check
ISR_NOERRCODE 18  # Machine Check
ISR_NOERRCODE 19  # SIMD Floating-Point Exception
ISR_NOERRCODE 20  # Virtualization Exception
ISR_NOERRCODE 21  # Control Protection Exception
ISR_NOERRCODE 22  # Reserved
ISR_NOERRCODE 23  # Reserved
ISR_NOERRCODE 24  # Reserved
ISR_NOERRCODE 25  # Reserved
ISR_NOERRCODE 26  # Reserved
ISR_NOERRCODE 27  # Reserved
ISR_NOERRCODE 28  # Hypervisor Injection Exception
ISR_NOERRCODE 29  # VMM Communication Exception
ISR_ERRCODE 30    # Security Exception
ISR_NOERRCODE 31  # Reserved

# Create IRQ stubs for hardware interrupts (32-47)
IRQ 32, 0   # Timer
IRQ 33, 1   # Keyboard
IRQ 34, 2   # Cascade
IRQ 35, 3   # COM2
IRQ 36, 4   # COM1
IRQ 37, 5   # LPT2
IRQ 38, 6   # Floppy
IRQ 39, 7   # LPT1
IRQ 40, 8   # CMOS
IRQ 41, 9   # Free
IRQ 42, 10  # Free
IRQ 43, 11  # Free
IRQ 44, 12  # PS/2 Mouse
IRQ 45, 13  # FPU
IRQ 46, 14  # Primary ATA
IRQ 47, 15  # Secondary ATA

# Common exception handler stub
isr_common_stub:
    # Save all registers
    pusha
    
    # Save segment registers
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    
    # Load kernel data segments
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    # Call C exception handler
    # Stack layout: [gs] [fs] [es] [ds] [edi] [esi] [ebp] [esp] [ebx] [edx] [ecx] [eax] [err] [vector]
    # Vector is at [esp + 48], error code at [esp + 44]
    movl 48(%esp), %eax  # Get vector number
    movl 44(%esp), %edx  # Get error code
    pushl %edx           # Push error code
    pushl %eax           # Push vector
    call exception_handler
    addl $8, %esp
    
    # Restore segment registers
    popl %gs
    popl %fs
    popl %es
    popl %ds
    
    # Restore all registers
    popa
    
    # Remove error code and interrupt number
    addl $8, %esp
    
    # Return from interrupt
    iret

# Common IRQ handler stub
irq_common_stub:
    # Save all registers
    pusha
    
    # Save segment registers
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    
    # Load kernel data segments
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    # Call C IRQ handler
    # Stack layout: [gs] [fs] [es] [ds] [edi] [esi] [ebp] [esp] [ebx] [edx] [ecx] [eax] [err] [irq]
    # IRQ number is at [esp + 48] (48 = 16 bytes for pusha + 16 bytes for segment regs + 16 bytes for err/irq)
    movl 48(%esp), %eax  # Get IRQ number from stack
    pushl %eax           # Push IRQ number as argument
    call irq_handler
    addl $4, %esp        # Remove argument
    
    # Restore segment registers
    popl %gs
    popl %fs
    popl %es
    popl %ds
    
    # Restore all registers
    popa
    
    # Remove error code and IRQ number
    addl $8, %esp
    
    # Return from interrupt
    iret


# Load IDT function
idt_flush:
    movl 4(%esp), %eax  # Get IDT pointer from stack
    lidt (%eax)
    ret

# Serial message used by early kernel trace
.align 1
serial_msg:
    .ascii "KERNEL PM\n\0"

# GDT (if needed, but loader already sets this up)
.align 8
gdt:
    .quad 0x0000000000000000     # Null descriptor
    .quad 0x00cf9a000000ffff     # Code segment
    .quad 0x00cf92000000ffff     # Data segment

gdt_ptr:
    .word (3*8 - 1)
    .long gdt
