/*
 * ============================================================================
 * RusticOS Interrupt Handling Header (interrupt.h)
 * ============================================================================
 * 
 * Defines the interrupt handling system interface, including PIC configuration,
 * IRQ definitions, and interrupt handler function declarations.
 * 
 * The interrupt system remaps hardware IRQs (0-15) to interrupt vectors (32-47)
 * to avoid conflicts with CPU exception vectors (0-31).
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "types.h"

// ============================================================================
// PIC (Programmable Interrupt Controller) I/O Ports
// ============================================================================
#define PIC1_COMMAND    0x20  // Master PIC command/status port
#define PIC1_DATA       0x21  // Master PIC data/IMR (Interrupt Mask Register) port
#define PIC2_COMMAND    0xA0  // Slave PIC command/status port
#define PIC2_DATA       0xA1  // Slave PIC data/IMR port

// ============================================================================
// PIC Initialization Commands
// ============================================================================
#define PIC_EOI         0x20  // End of Interrupt command (sent after handling IRQ)
#define PIC_ICW1_INIT   0x10  // Initialization Command Word 1: Start initialization
#define PIC_ICW1_ICW4   0x01  // ICW1 flag: ICW4 is needed
#define PIC_ICW4_8086   0x01  // ICW4: 8086/8088 mode (not MCS-80/85)

// ============================================================================
// PIT (Programmable Interval Timer) I/O Ports
// ============================================================================
#define PIT_CH0_DATA    0x40  // Channel 0 data port (used for timer interrupt IRQ0)
#define PIT_CH1_DATA    0x41  // Channel 1 data port (typically used for RAM refresh)
#define PIT_CH2_DATA    0x42  // Channel 2 data port (typically used for speaker)
#define PIT_COMMAND     0x43  // PIT command/control port

// PIT Configuration
#define PIT_BASE_FREQUENCY  1193182   // PIT base frequency in Hz
#define PIT_DEFAULT_FREQUENCY 18      // Default frequency (~18.2 Hz)

// ============================================================================
// RTC (Real-Time Clock) / CMOS I/O Ports
// ============================================================================
#define CMOS_INDEX      0x70  // CMOS index/select register
#define CMOS_DATA       0x71  // CMOS data register

// RTC Register Addresses
#define RTC_SECONDS     0x00  // Seconds (0-59)
#define RTC_MINUTES     0x02  // Minutes (0-59)
#define RTC_HOURS       0x04  // Hours (0-23 or 1-12 with AM/PM)
#define RTC_DAY         0x07  // Day of month (1-31)
#define RTC_MONTH       0x08  // Month (1-12)
#define RTC_YEAR        0x09  // Year (last 2 digits, 0-99)
#define RTC_CENTURY     0x32  // Century (19 or 20, if available)
#define RTC_STATUS_A    0x0A  // Status register A
#define RTC_STATUS_B    0x0B  // Status register B

// RTC Status Register A flags
#define RTC_A_UIP       0x80  // Update in progress flag (1 = update happening, 0 = safe to read)

// RTC Status Register B flags
#define RTC_B_24HOUR    0x02  // 24-hour format (1) or 12-hour format (0)
#define RTC_B_BCD       0x04  // BCD format (1) or binary format (0)

// Timezone offset (hours from UTC)
// Helsinki, Finland: UTC+2 (EET) or UTC+3 (EEST during daylight saving)
// Adjust this value if needed: +2 for EET, +3 for EEST
#define RTC_TIMEZONE_OFFSET  2  // Hours to add to UTC time

// ============================================================================
// IRQ Definitions (Hardware Interrupt Requests)
// ============================================================================
// IRQs are remapped to interrupt vectors 32-47 (IRQ_BASE + IRQ number)
#define IRQ_BASE        32    // Base interrupt vector for IRQs (avoiding exceptions 0-31)

// Hardware IRQ assignments
#define IRQ_TIMER       0     // Timer interrupt (IRQ0) - Used for scheduling
#define IRQ_KEYBOARD    1     // Keyboard interrupt (IRQ1) - Used for input
#define IRQ_CASCADE     2     // Cascade interrupt (IRQ2) - Links slave PIC to master
#define IRQ_COM2        3     // COM2 serial port (IRQ3)
#define IRQ_COM1        4     // COM1 serial port (IRQ4)
#define IRQ_LPT2        5     // LPT2 parallel port (IRQ5)
#define IRQ_FLOPPY      6     // Floppy disk controller (IRQ6)
#define IRQ_LPT1        7     // LPT1 parallel port (IRQ7)
#define IRQ_CMOS        8     // CMOS real-time clock (IRQ8) - Slave PIC
#define IRQ_FREE1       9     // Free/redirected (IRQ9) - Slave PIC
#define IRQ_FREE2       10    // Free (IRQ10) - Slave PIC
#define IRQ_FREE3       11    // Free (IRQ11) - Slave PIC
#define IRQ_PS2         12    // PS/2 mouse (IRQ12) - Slave PIC
#define IRQ_FPU         13    // FPU coprocessor (IRQ13) - Slave PIC
#define IRQ_PRIMARY_ATA 14    // Primary ATA hard disk (IRQ14) - Slave PIC
#define IRQ_SECONDARY_ATA 15  // Secondary ATA hard disk (IRQ15) - Slave PIC

// Function declarations
extern "C" {
    void init_idt();  // Called from assembly, needs C linkage
}

// C++ functions
void init_pic();
void enable_interrupts();
void disable_interrupts();
void send_eoi(uint8_t irq);

// IRQ masking functions
void enable_irq(uint8_t irq);   // Enable a specific IRQ
void disable_irq(uint8_t irq);   // Disable a specific IRQ

// PIT (Programmable Interval Timer) functions
void init_pit();                          // Initialize PIT with default frequency
void set_pit_frequency(uint16_t frequency); // Set PIT frequency in Hz

// IRQ handler functions (called from assembly ISR stubs)
extern "C" void irq_handler(uint8_t irq);
extern "C" void exception_handler(uint8_t vector, uint32_t error_code);

// System clock functions
uint64_t get_ticks();           // Get current tick count
uint64_t get_seconds();         // Get elapsed seconds since boot
uint64_t get_milliseconds();    // Get elapsed milliseconds since boot

// Real-Time Clock (RTC) structure
struct RTCTime {
    uint8_t second;     // 0-59
    uint8_t minute;     // 0-59
    uint8_t hour;       // 0-23 (24-hour format)
    uint8_t day;        // 1-31
    uint8_t month;      // 1-12
    uint8_t year;       // 0-99 (last 2 digits)
    uint8_t century;    // 19 or 20 (if available, otherwise 0)
};

// RTC functions
uint8_t read_rtc_register(uint8_t reg);  // Read a CMOS/RTC register
bool get_rtc_time(RTCTime* time);         // Get current RTC time

#endif // INTERRUPT_H

