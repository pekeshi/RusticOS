/*
 * ============================================================================
 * RusticOS Interrupt Handling Implementation (interrupt.cpp)
 * ============================================================================
 * 
 * Implements interrupt handling infrastructure for RusticOS, including:
 *   - Programmable Interrupt Controller (PIC) initialization
 *   - Interrupt Service Routine (ISR) handlers for exceptions and IRQs
 *   - Keyboard interrupt handling (IRQ1)
 *   - Timer interrupt handling (IRQ0) - available for future scheduling
 *   - Exception handling for CPU exceptions
 * 
 * The interrupt system enables proper hardware I/O and paves the way for
 * preemptive multitasking and real-time device handling.
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#include "interrupt.h"
#include "keyboard.h"
#include "terminal.h"

extern Terminal terminal;
extern KeyboardDriver keyboard;

// System clock tick counter (incremented on each timer interrupt)
// The PIT typically runs at ~18.2 Hz (54.9 ms per tick) by default
static volatile uint64_t system_ticks = 0;

// External references to ISR and IRQ stubs (defined in crt0.s)
extern "C" {
    extern void isr0();
    extern void isr1();
    extern void isr2();
    extern void isr3();
    extern void isr4();
    extern void isr5();
    extern void isr6();
    extern void isr7();
    extern void isr8();
    extern void isr9();
    extern void isr10();
    extern void isr11();
    extern void isr12();
    extern void isr13();
    extern void isr14();
    extern void isr15();
    extern void isr16();
    extern void isr17();
    extern void isr18();
    extern void isr19();
    extern void isr20();
    extern void isr21();
    extern void isr22();
    extern void isr23();
    extern void isr24();
    extern void isr25();
    extern void isr26();
    extern void isr27();
    extern void isr28();
    extern void isr29();
    extern void isr30();
    extern void isr31();
    extern void irq32();
    extern void irq33();
    extern void irq34();
    extern void irq35();
    extern void irq36();
    extern void irq37();
    extern void irq38();
    extern void irq39();
    extern void irq40();
    extern void irq41();
    extern void irq42();
    extern void irq43();
    extern void irq44();
    extern void irq45();
    extern void irq46();
    extern void irq47();
}

// IDT Entry structure (must match x86 IDT format)
struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

// External IDT (defined in crt0.s)
extern "C" {
    extern IDTEntry idt[256];
}

/**
 * Set an IDT entry
 */
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].offset_low  = (base & 0xFFFF);
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

/**
 * Initialize the Interrupt Descriptor Table
 */
extern "C" void init_idt() {
    // Set up exception handlers (0-31)
    idt_set_gate(0,  (uint32_t)isr0,  0x08, 0x8E);  // Divide by Zero
    idt_set_gate(1,  (uint32_t)isr1,  0x08, 0x8E);  // Debug
    idt_set_gate(2,  (uint32_t)isr2,  0x08, 0x8E);  // NMI
    idt_set_gate(3,  (uint32_t)isr3,  0x08, 0x8E);  // Breakpoint
    idt_set_gate(4,  (uint32_t)isr4,  0x08, 0x8E);  // Overflow
    idt_set_gate(5,  (uint32_t)isr5,  0x08, 0x8E);  // Bound Range
    idt_set_gate(6,  (uint32_t)isr6,  0x08, 0x8E);  // Invalid Opcode
    idt_set_gate(7,  (uint32_t)isr7,  0x08, 0x8E);  // Device Not Available
    idt_set_gate(8,  (uint32_t)isr8,  0x08, 0x8E);  // Double Fault
    idt_set_gate(9,  (uint32_t)isr9,  0x08, 0x8E);  // Coprocessor Segment
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);  // Invalid TSS
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);  // Segment Not Present
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);  // Stack Fault
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);  // General Protection Fault
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);  // Page Fault
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);  // Reserved
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);  // x87 FPU Error
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);  // Alignment Check
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);  // Machine Check
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);  // SIMD Exception
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);  // Virtualization
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);  // Control Protection
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);  // Reserved
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);  // Reserved
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);  // Reserved
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);  // Reserved
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);  // Reserved
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);  // Reserved
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);  // Hypervisor
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);  // VMM Communication
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);  // Security
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);  // Reserved
    
    // Set up IRQ handlers (32-47)
    idt_set_gate(32, (uint32_t)irq32, 0x08, 0x8E);  // Timer (IRQ0)
    idt_set_gate(33, (uint32_t)irq33, 0x08, 0x8E);  // Keyboard (IRQ1)
    idt_set_gate(34, (uint32_t)irq34, 0x08, 0x8E);  // Cascade (IRQ2)
    idt_set_gate(35, (uint32_t)irq35, 0x08, 0x8E);  // COM2 (IRQ3)
    idt_set_gate(36, (uint32_t)irq36, 0x08, 0x8E);  // COM1 (IRQ4)
    idt_set_gate(37, (uint32_t)irq37, 0x08, 0x8E);  // LPT2 (IRQ5)
    idt_set_gate(38, (uint32_t)irq38, 0x08, 0x8E);  // Floppy (IRQ6)
    idt_set_gate(39, (uint32_t)irq39, 0x08, 0x8E);  // LPT1 (IRQ7)
    idt_set_gate(40, (uint32_t)irq40, 0x08, 0x8E);  // CMOS (IRQ8)
    idt_set_gate(41, (uint32_t)irq41, 0x08, 0x8E);  // Free (IRQ9)
    idt_set_gate(42, (uint32_t)irq42, 0x08, 0x8E);  // Free (IRQ10)
    idt_set_gate(43, (uint32_t)irq43, 0x08, 0x8E);  // Free (IRQ11)
    idt_set_gate(44, (uint32_t)irq44, 0x08, 0x8E);  // PS/2 Mouse (IRQ12)
    idt_set_gate(45, (uint32_t)irq45, 0x08, 0x8E);  // FPU (IRQ13)
    idt_set_gate(46, (uint32_t)irq46, 0x08, 0x8E);  // Primary ATA (IRQ14)
    idt_set_gate(47, (uint32_t)irq47, 0x08, 0x8E);  // Secondary ATA (IRQ15)
}

/**
 * Initialize the Programmable Interrupt Controller (PIC)
 * 
 * Remaps IRQ 0-15 to interrupt vectors 32-47 to avoid conflicts
 * with CPU exceptions (0-31).
 */
void init_pic() {
    // Save masks
    uint8_t a1, a2;
    
    // Get current interrupt masks
    asm volatile("inb %1, %0" : "=a"(a1) : "Nd"(PIC1_DATA));
    asm volatile("inb %1, %0" : "=a"(a2) : "Nd"(PIC2_DATA));
    
    // Initialize PICs (cascade mode)
    // ICW1: Start initialization sequence
    asm volatile("outb %0, %1" : : "a"((uint8_t)(PIC_ICW1_INIT | PIC_ICW1_ICW4)), "Nd"(PIC1_COMMAND));
    asm volatile("outb %0, %1" : : "a"((uint8_t)(PIC_ICW1_INIT | PIC_ICW1_ICW4)), "Nd"(PIC2_COMMAND));
    
    // ICW2: Set interrupt vector offset (master: 32, slave: 40)
    asm volatile("outb %0, %1" : : "a"((uint8_t)IRQ_BASE), "Nd"(PIC1_DATA));
    asm volatile("outb %0, %1" : : "a"((uint8_t)(IRQ_BASE + 8)), "Nd"(PIC2_DATA));
    
    // ICW3: Tell master PIC that slave PIC is at IRQ2
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x04), "Nd"(PIC1_DATA));
    // Tell slave PIC its cascade identity (bit 2)
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x02), "Nd"(PIC2_DATA));
    
    // ICW4: Set 8086 mode
    asm volatile("outb %0, %1" : : "a"((uint8_t)PIC_ICW4_8086), "Nd"(PIC1_DATA));
    asm volatile("outb %0, %1" : : "a"((uint8_t)PIC_ICW4_8086), "Nd"(PIC2_DATA));
    
    // Restore masks (enable only keyboard and timer initially)
    // Mask all interrupts except timer (0) and keyboard (1)
    asm volatile("outb %0, %1" : : "a"((uint8_t)0xFC), "Nd"(PIC1_DATA)); // Enable timer and keyboard on master
    asm volatile("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"(PIC2_DATA)); // Disable all on slave for now
}

/**
 * Send End of Interrupt signal to PIC
 * This must be called at the end of each IRQ handler
 */
void send_eoi(uint8_t irq) {
    if (irq >= 8) {
        // If IRQ came from slave PIC, send EOI to both
        asm volatile("outb %0, %1" : : "a"((uint8_t)PIC_EOI), "Nd"(PIC2_COMMAND));
    }
    // Always send EOI to master PIC
    asm volatile("outb %0, %1" : : "a"((uint8_t)PIC_EOI), "Nd"(PIC1_COMMAND));
}

/**
 * ============================================================================
 * IRQ Masking Functions
 * ============================================================================
 */

/**
 * Enable a specific IRQ
 * 
 * Unmasks the specified IRQ in the PIC, allowing it to generate interrupts.
 * 
 * @param irq IRQ number (0-15)
 */
void enable_irq(uint8_t irq) {
    if (irq > 15) {
        return;  // Invalid IRQ
    }
    
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        // Master PIC
        port = PIC1_DATA;
    } else {
        // Slave PIC
        port = PIC2_DATA;
        irq -= 8;
    }
    
    // Read current mask
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    
    // Clear the bit for this IRQ (0 = enabled, 1 = disabled)
    value &= ~(1 << irq);
    
    // Write new mask
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Disable a specific IRQ
 * 
 * Masks the specified IRQ in the PIC, preventing it from generating interrupts.
 * 
 * @param irq IRQ number (0-15)
 */
void disable_irq(uint8_t irq) {
    if (irq > 15) {
        return;  // Invalid IRQ
    }
    
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        // Master PIC
        port = PIC1_DATA;
    } else {
        // Slave PIC
        port = PIC2_DATA;
        irq -= 8;
    }
    
    // Read current mask
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    
    // Set the bit for this IRQ (1 = disabled, 0 = enabled)
    value |= (1 << irq);
    
    // Write new mask
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}


/**
 * IRQ Handler - called from assembly ISR stubs
 * Routes IRQs to appropriate device drivers
 */
extern "C" void irq_handler(uint8_t irq) {
    switch (irq) {
        case IRQ_TIMER:
            // Timer interrupt - increment system clock tick counter
            // This provides a system clock for timing and scheduling
            system_ticks++;
            break;
            
        case IRQ_KEYBOARD: {
            // Keyboard interrupt - read scan code and process
            uint8_t scan_code;
            asm volatile("inb %1, %0" : "=a"(scan_code) : "Nd"((uint16_t)0x60));
            keyboard.handle_interrupt(scan_code);
            break;
        }
        
        default:
            // Unknown IRQ - acknowledge anyway
            break;
    }
    
    // Send End of Interrupt to PIC
    send_eoi(irq);
}

/**
 * ============================================================================
 * Exception Names
 * ============================================================================
 */
static const char* exception_names[32] = {
    "Divide by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved (15)",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved (22)",
    "Reserved (23)",
    "Reserved (24)",
    "Reserved (25)",
    "Reserved (26)",
    "Reserved (27)",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved (31)"
};

/**
 * Helper function to convert number to hex string
 */
static void uint32_to_hex(uint32_t value, char* buffer) {
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        uint8_t nibble = (value >> (i * 4)) & 0xF;
        buffer[9 - i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    buffer[10] = '\0';
}

/**
 * Helper function to convert number to decimal string
 */
static void uint32_to_string(uint32_t value, char* buffer) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    char rev_buf[16];
    int rev_pos = 0;
    uint32_t temp = value;
    
    while (temp > 0) {
        rev_buf[rev_pos++] = '0' + (temp % 10);
        temp /= 10;
    }
    
    int pos = 0;
    for (int i = rev_pos - 1; i >= 0; i--) {
        buffer[pos++] = rev_buf[i];
    }
    buffer[pos] = '\0';
}

/**
 * Exception Handler - called from assembly ISR stubs for CPU exceptions
 * 
 * Displays detailed exception information including:
 * - Exception name
 * - Exception vector number
 * - Error code (if applicable)
 */
extern "C" void exception_handler(uint8_t vector, uint32_t error_code) {
    char num_buf[32];
    char hex_buf[16];
    
    terminal.write("\n=== EXCEPTION ===\n");
    
    // Display exception name
    if (vector < 32) {
        terminal.write("Exception: ");
        terminal.write(exception_names[vector]);
        terminal.write("\n");
    }
    
    // Display vector number
    terminal.write("Vector: ");
    uint32_to_string((uint32_t)vector, num_buf);
    terminal.write(num_buf);
    terminal.write("\n");
    
    // Display error code (if applicable)
    // Some exceptions don't push an error code, but we'll display it anyway
    // The error code is meaningful for exceptions 8, 10-14, 17, 21
    if (vector == 8 || (vector >= 10 && vector <= 14) || vector == 17 || vector == 21) {
        terminal.write("Error Code: ");
        uint32_to_hex(error_code, hex_buf);
        terminal.write(hex_buf);
        terminal.write(" (");
        uint32_to_string(error_code, num_buf);
        terminal.write(num_buf);
        terminal.write(")\n");
    }
    
    terminal.write("==================\n");
    
    // Halt for exceptions (except page fault, double fault which we might want to handle)
    if (vector != 14) {  // Don't halt on page fault (not implemented yet)
        terminal.write("System halted.\n");
        asm volatile("cli; hlt");  // Disable interrupts and halt
    }
}

/**
 * Enable interrupts (STI)
 */
void enable_interrupts() {
    asm volatile("sti");
}

/**
 * Disable interrupts (CLI)
 */
void disable_interrupts() {
    asm volatile("cli");
}

/**
 * ============================================================================
 * PIT (Programmable Interval Timer) Functions
 * ============================================================================
 */

/**
 * Initialize the Programmable Interval Timer (PIT)
 * 
 * Configures the PIT channel 0 to generate interrupts at the default frequency
 * (~18.2 Hz). This is called automatically during system initialization.
 */
void init_pit() {
    set_pit_frequency(PIT_DEFAULT_FREQUENCY);
}

/**
 * Set PIT frequency
 * 
 * Configures the PIT channel 0 to generate interrupts at the specified frequency.
 * The PIT base frequency is 1193182 Hz. The divisor is calculated as:
 * divisor = 1193182 / frequency
 * 
 * @param frequency Desired frequency in Hz (must be between 19 and 1193182)
 */
void set_pit_frequency(uint32_t frequency) {
    // Clamp frequency to valid range
    if (frequency < 19) {
        frequency = 19;  // Minimum frequency
    }
    if (frequency > PIT_BASE_FREQUENCY) {
        frequency = PIT_BASE_FREQUENCY;  // Maximum frequency
    }
    
    // Calculate divisor
    uint16_t divisor = (uint16_t)(PIT_BASE_FREQUENCY / frequency);
    
    // Disable interrupts temporarily during PIT configuration
    disable_interrupts();
    
    // Configure PIT channel 0:
    // Command byte: 0x36 = 0011 0110
    //   Bits 7-6: 00 = Channel 0
    //   Bits 5-4: 11 = Access mode: low byte, then high byte
    //   Bits 3-1: 011 = Mode 3 (Square wave generator)
    //   Bit 0: 0 = Binary mode (not BCD)
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x36), "Nd"((uint16_t)PIT_COMMAND));
    
    // Set divisor (low byte then high byte)
    asm volatile("outb %0, %1" : : "a"((uint8_t)(divisor & 0xFF)), "Nd"((uint16_t)PIT_CH0_DATA));
    asm volatile("outb %0, %1" : : "a"((uint8_t)((divisor >> 8) & 0xFF)), "Nd"((uint16_t)PIT_CH0_DATA));
    
    // Re-enable interrupts
    enable_interrupts();
}

/**
 * ============================================================================
 * System Clock Functions
 * ============================================================================
 */

/**
 * Get current system tick count
 * 
 * Returns the number of timer interrupts that have occurred since boot.
 * The PIT typically runs at ~18.2 Hz (54.9 ms per tick) by default.
 * 
 * @return Current tick count
 */
uint64_t get_ticks() {
    return system_ticks;
}

/**
 * Get elapsed seconds since boot
 * 
 * Calculates seconds based on tick count. Assumes PIT frequency of ~18.2 Hz
 * (approximately 18.2 ticks per second).
 * 
 * @return Elapsed seconds since boot
 */
uint64_t get_seconds() {
    // PIT runs at ~18.2 Hz, so divide ticks by 18.2 (or multiply by 10/182)
    // Using integer math: ticks * 10 / 182 ≈ ticks / 18.2
    return (system_ticks * 10) / 182;
}

/**
 * Get elapsed milliseconds since boot
 * 
 * Calculates milliseconds based on tick count. Assumes PIT frequency of ~18.2 Hz
 * (approximately 54.9 ms per tick).
 * 
 * @return Elapsed milliseconds since boot
 */
uint64_t get_milliseconds() {
    // PIT runs at ~18.2 Hz, so each tick is ~54.9 ms
    // Using integer math: ticks * 549 / 10 ≈ ticks * 54.9
    return (system_ticks * 549) / 10;
}

/**
 * ============================================================================
 * Real-Time Clock (RTC) Functions
 * ============================================================================
 */

/**
 * Convert BCD (Binary Coded Decimal) to binary
 * BCD format: each digit is stored in 4 bits (0x23 = 23 decimal)
 * 
 * @param bcd BCD value to convert
 * @return Binary value
 */
static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
 * Read a CMOS/RTC register
 * 
 * The CMOS is accessed through two I/O ports:
 *   - 0x70: Index/select register (specifies which register to read)
 *   - 0x71: Data register (contains the register value)
 * 
 * Important: The NMI (Non-Maskable Interrupt) must be disabled when accessing
 * CMOS registers. Bit 7 of port 0x70 controls NMI.
 * 
 * @param reg Register address to read (e.g., RTC_SECONDS, RTC_HOURS)
 * @return Register value
 */
uint8_t read_rtc_register(uint8_t reg) {
    // Disable NMI by setting bit 7, then select register
    asm volatile("outb %0, %1" : : "a"((uint8_t)(0x80 | reg)), "Nd"((uint16_t)CMOS_INDEX));
    
    // Small delay to ensure CMOS is ready
    for (volatile int i = 0; i < 10; i++);
    
    // Read the register value
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"((uint16_t)CMOS_DATA));
    
    return value;
}

/**
 * Wait for RTC update to complete
 * 
 * The RTC updates its registers periodically. We must wait for the
 * update-in-progress flag to clear before reading to avoid corrupted values.
 * 
 * @return true if update completed, false on timeout
 */
static bool wait_rtc_update() {
    // Wait for update-in-progress flag to clear
    // Status register A bit 7 (UIP) indicates update in progress
    for (int i = 0; i < 1000; i++) {
        uint8_t status_a = read_rtc_register(RTC_STATUS_A);
        if ((status_a & RTC_A_UIP) == 0) {
            return true;  // Update complete
        }
        // Small delay
        for (volatile int j = 0; j < 100; j++);
    }
    return false;  // Timeout
}

/**
 * Get current Real-Time Clock time
 * 
 * Reads the RTC registers from CMOS and converts them to a usable format.
 * Handles both BCD and binary formats, and 12/24-hour formats.
 * 
 * Uses double-read method to ensure consistency: read all registers twice
 * and verify they match (to catch updates that occur during reading).
 * 
 * @param time Pointer to RTCTime structure to fill
 * @return true if successful, false on error
 */
bool get_rtc_time(RTCTime* time) {
    if (!time) {
        return false;
    }
    
    // Wait for RTC update to complete
    if (!wait_rtc_update()) {
        return false;  // RTC update timeout
    }
    
    // Read status register B to determine format
    uint8_t status_b = read_rtc_register(RTC_STATUS_B);
    // DM bit (bit 2): 0 = BCD format, 1 = binary format
    bool is_bcd = (status_b & RTC_B_BCD) == 0;
    // 24/12 bit (bit 1): 0 = 12-hour format, 1 = 24-hour format
    bool is_24hour = (status_b & RTC_B_24HOUR) != 0;
    
    // Read all registers twice to ensure consistency
    // This catches updates that occur during reading
    uint8_t second1, minute1, hour1, day1, month1, year1;
    uint8_t second2, minute2, hour2, day2, month2, year2;
    
    // First read
    second1 = read_rtc_register(RTC_SECONDS);
    minute1 = read_rtc_register(RTC_MINUTES);
    hour1 = read_rtc_register(RTC_HOURS);
    day1 = read_rtc_register(RTC_DAY);
    month1 = read_rtc_register(RTC_MONTH);
    year1 = read_rtc_register(RTC_YEAR);
    
    // Second read (to verify consistency)
    second2 = read_rtc_register(RTC_SECONDS);
    minute2 = read_rtc_register(RTC_MINUTES);
    hour2 = read_rtc_register(RTC_HOURS);
    day2 = read_rtc_register(RTC_DAY);
    month2 = read_rtc_register(RTC_MONTH);
    year2 = read_rtc_register(RTC_YEAR);
    
    // If values don't match, an update occurred during reading - retry once
    if (second1 != second2 || minute1 != minute2 || hour1 != hour2 ||
        day1 != day2 || month1 != month2 || year1 != year2) {
        // Wait and retry
        if (!wait_rtc_update()) {
            return false;
        }
        second1 = read_rtc_register(RTC_SECONDS);
        minute1 = read_rtc_register(RTC_MINUTES);
        hour1 = read_rtc_register(RTC_HOURS);
        day1 = read_rtc_register(RTC_DAY);
        month1 = read_rtc_register(RTC_MONTH);
        year1 = read_rtc_register(RTC_YEAR);
    }
    
    uint8_t second = second1;
    uint8_t minute = minute1;
    uint8_t hour = hour1;
    uint8_t day = day1;
    uint8_t month = month1;
    uint8_t year = year1;
    
    // Convert BCD to binary if needed
    if (is_bcd) {
        second = bcd_to_binary(second);
        minute = bcd_to_binary(minute);
        hour = bcd_to_binary(hour);
        day = bcd_to_binary(day);
        month = bcd_to_binary(month);
        year = bcd_to_binary(year);
    }
    
    // Validate values are in reasonable ranges
    if (second > 59 || minute > 59 || hour > 23 || 
        day < 1 || day > 31 || month < 1 || month > 12 || year > 99) {
        return false;  // Invalid values
    }
    
    // Handle 12-hour format (convert to 24-hour)
    if (!is_24hour) {
        // Bit 7 of hour register indicates PM in 12-hour format
        uint8_t hour_12 = hour & 0x7F;  // Mask out PM bit
        if (hour & 0x80) {
            // PM: 12 PM stays as 12, others add 12
            if (hour_12 == 12) {
                hour = 12;
            } else {
                hour = hour_12 + 12;
            }
        } else {
            // AM: 12 AM becomes 0, others stay the same
            if (hour_12 == 12) {
                hour = 0;
            } else {
                hour = hour_12;
            }
        }
        // Validate after conversion
        if (hour > 23) {
            return false;
        }
    }
    
    // Try to read century register (may not be available on all systems)
    uint8_t century = 0;
    uint8_t century_reg = read_rtc_register(RTC_CENTURY);
    // Validate century register (should be 19 or 20, or 0xFF if not available)
    if (century_reg != 0xFF && century_reg != 0x00) {
        if (is_bcd) {
            century = bcd_to_binary(century_reg);
        } else {
            century = century_reg;
        }
        // Validate century is reasonable (19 or 20)
        if (century != 19 && century != 20) {
            century = 0;  // Invalid century, ignore it
        }
    }
    
    // Apply timezone offset (RTC typically stores UTC time)
    hour += RTC_TIMEZONE_OFFSET;
    if (hour >= 24) {
        hour -= 24;
        // Handle day rollover
        day++;
        
        // Get number of days in current month
        uint8_t days_in_month;
        if (month == 2) {
            // February: check for leap year (simplified - assumes 2000s)
            bool is_leap = (year % 4 == 0);
            days_in_month = is_leap ? 29 : 28;
        } else if (month == 4 || month == 6 || month == 9 || month == 11) {
            // April, June, September, November
            days_in_month = 30;
        } else {
            // All other months
            days_in_month = 31;
        }
        
        // Handle month rollover
        if (day > days_in_month) {
            day = 1;
            month++;
            if (month > 12) {
                month = 1;
                year++;
                if (year > 99) {
                    year = 0;
                    if (century > 0) {
                        century++;
                    }
                }
            }
        }
    }
    
    // Fill the time structure
    time->second = second;
    time->minute = minute;
    time->hour = hour;
    time->day = day;
    time->month = month;
    time->year = year;
    time->century = century;
    
    return true;
}

