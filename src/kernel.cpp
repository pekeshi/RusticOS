/*
 * ============================================================================
 * RusticOS Kernel Main Module
 * ============================================================================
 * 
 * This is the main kernel entry point and event loop for RusticOS.
 * 
 * Responsibilities:
 *   - Hardware initialization (VGA display, serial port, keyboard, interrupts)
 *   - System startup sequence and initialization
 *   - Main event loop with interrupt-driven keyboard input
 *   - Integration with command-line interface and filesystem
 * 
 * Architecture:
 *   - Runs in 32-bit protected mode
 *   - Uses interrupt-driven I/O (keyboard via IRQ1)
 *   - Timer interrupt (IRQ0) available for future scheduling
 *   - All hardware access via direct I/O port operations
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#include "terminal.h"
#include "types.h"
#include "keyboard.h"
#include "filesystem.h"
#include "command.h"
#include "interrupt.h"

/* ============================================================================
 * HARDWARE CONSTANTS & MACROS
 * ============================================================================ */

// VGA Text Mode Constants
#define VGA_BUFFER      ((volatile uint16_t*)0xB8000)
#define VGA_WIDTH       80
#define VGA_HEIGHT      25
#define VGA_ATTR(fg, bg) ((((((bg) << 4) | (fg))) << 8))

// VGA I/O Port Constants
#define VGA_STATUS_PORT 0x3DA  // Read-only status register
#define VGA_CRTC_INDEX  0x3D4  // Index register for cursor control
#define VGA_CRTC_DATA   0x3D5  // Data register for cursor control
#define CRTC_CURSOR_HIGH 0x0E  // Cursor location high byte
#define CRTC_CURSOR_LOW  0x0F  // Cursor location low byte

// Serial Port (COM1) Constants
#define SERIAL_PORT     0x3F8
#define SERIAL_BAUD_DIV 0x01   // Divisor for 115200 baud
#define SERIAL_IER      (SERIAL_PORT + 1)
#define SERIAL_FCR      (SERIAL_PORT + 2)
#define SERIAL_LCR      (SERIAL_PORT + 3)

// PS/2 Keyboard Port Constants
#define KBD_DATA_PORT   0x60
#define KBD_STAT_PORT   0x64
#define KBD_STATUS_HAVE_DATA 0x01

// Keyboard Commands
#define KBD_CMD_DISABLE 0xF5
#define KBD_CMD_SET_SCANCODE 0xF0
#define KBD_SCANCODE_SET_1 0x01
#define KBD_CMD_ENABLE  0xF4
#define KBD_SCANCODE_RELEASE 0x80

// VGA Color Attributes (BLACK bg, BLACK text)
#define VGA_BLACK_BLACK 0x0700
// VGA Color Attributes (GREEN bg, BLACK text)
#define VGA_GREEN_BLACK 0x2000

// Timing Constants (loop counts for delays)
#define DELAY_SHORT     10000
#define DELAY_MEDIUM    100000

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

extern CommandSystem command_system;
extern KeyboardDriver keyboard;

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

static void set_cursor_position(uint8_t row, uint8_t col);
static void serial_write(const char* str);

/* ============================================================================
 * INLINE I/O PORT OPERATIONS
 * ============================================================================ */

/**
 * Read a byte from an I/O port
 * @param port The port address to read from
 * @return The byte read from the port
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ __volatile__("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * Write a byte to an I/O port
 * @param port The port address to write to
 * @param value The byte to write
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* ============================================================================
 * SIMPLE VGA TEXT PRINTER
 * ============================================================================ */

/* ============================================================================
 * KEYBOARD INPUT HANDLING
 * ============================================================================ */

// Shift state tracking
static bool shift_pressed = false;
static bool expecting_break_code = false;  // For scan code set 2 (0xF0 prefix)
static uint8_t last_scan_code = 0;  // For handling extended codes

/**
 * Convert PS/2 scan code set 1 to ASCII character
 * This is the standard set that QEMU typically uses
 */
static char scancode_set1_to_ascii(uint8_t code, bool shift) {
    // Scan Code Set 1 - unshifted
    // Array indices match scan codes directly (0x00-0x39 = 58 elements)
    static const char unshifted[0x3A] = {
        0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  // 0x00-0x09
        '9', '0', '-', '=', '\b','\t', 'q', 'w', 'e', 'r',  // 0x0A-0x13
        't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',0,    // 0x14-0x1D (Enter = 0x1C = '\n')
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k',            // 0x1E-0x25
        'l', ';', '\'','`', 0,   '\\','z', 'x', 'c', 'v',  // 0x26-0x2F
        'b', 'n', 'm', ',', '.', '/', 0,   0,   ' ', 0     // 0x30-0x39 (Space = 0x39 = ' ')
    };
    
    // Scan Code Set 1 - shifted
    static const char shifted[0x3A] = {
        0,   0,   '!', '@', '#', '$', '%', '^', '&', '*',    // 0x00-0x09
        '(', ')', '_', '+', '\b','\t', 'Q', 'W', 'E', 'R', // 0x0A-0x13
        'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',0,    // 0x14-0x1D
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K',            // 0x1E-0x25
        'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',  // 0x26-0x2F
        'B', 'N', 'M', '<', '>', '?', 0,   0,   ' ', 0     // 0x30-0x39
    };
    
    // Ensure code is within valid range (0x00-0x39)
    if (code < 0x3A) {
        return shift ? shifted[code] : unshifted[code];
    }
    return 0;
}

/**
 * Convert PS/2 scan code set 2 to ASCII character
 * Used by some modern keyboards
 */
static char scancode_set2_to_ascii(uint8_t code, bool shift) {
    switch (code) {
    // Letters (Set 2)
    case 0x1C: return shift ? 'A' : 'a';
    case 0x32: return shift ? 'B' : 'b';
    case 0x21: return shift ? 'C' : 'c';
    case 0x23: return shift ? 'D' : 'd';
    case 0x24: return shift ? 'E' : 'e';
    case 0x2B: return shift ? 'F' : 'f';
    case 0x34: return shift ? 'G' : 'g';
    case 0x33: return shift ? 'H' : 'h';
    case 0x43: return shift ? 'I' : 'i';
    case 0x3B: return shift ? 'J' : 'j';
    case 0x42: return shift ? 'K' : 'k';
    case 0x4B: return shift ? 'L' : 'l';
    case 0x3A: return shift ? 'M' : 'm';
    case 0x31: return shift ? 'N' : 'n';
    case 0x44: return shift ? 'O' : 'o';
    case 0x4D: return shift ? 'P' : 'p';
    case 0x15: return shift ? 'Q' : 'q';
    case 0x2D: return shift ? 'R' : 'r';
    case 0x1B: return shift ? 'S' : 's';
    case 0x2C: return shift ? 'T' : 't';
    case 0x3C: return shift ? 'U' : 'u';
    case 0x2A: return shift ? 'V' : 'v';
    case 0x1D: return shift ? 'W' : 'w';
    case 0x22: return shift ? 'X' : 'x';
    case 0x35: return shift ? 'Y' : 'y';
    case 0x1A: return shift ? 'Z' : 'z';
    
    // Numbers (Set 2)
    case 0x16: return shift ? '!' : '1';
    case 0x1E: return shift ? '@' : '2';
    case 0x26: return shift ? '#' : '3';
    case 0x25: return shift ? '$' : '4';
    case 0x2E: return shift ? '%' : '5';
    case 0x36: return shift ? '^' : '6';
    case 0x3D: return shift ? '&' : '7';
    case 0x3E: return shift ? '*' : '8';
    case 0x46: return shift ? '(' : '9';
    case 0x45: return shift ? ')' : '0';
    
    // Symbols (Set 2)
    case 0x4E: return shift ? '_' : '-';
    case 0x55: return shift ? '+' : '=';
    case 0x41: return shift ? ':' : ';';
    case 0x49: return shift ? '"' : '\'';
    case 0x0E: return shift ? '~' : '`';
    case 0x5D: return shift ? '|' : '\\';
    case 0x54: return shift ? '{' : '[';
    case 0x5B: return shift ? '}' : ']';
    case 0x4C: return shift ? '<' : ',';
    case 0x52: return shift ? '>' : '.';
    case 0x4A: return shift ? '?' : '/';
    
    // Special keys (Set 2)
    case 0x29: return ' ';
    case 0x5A: return '\n';
    case 0x66: return '\b';
    case 0x0D: return '\t';
    
    default:
        return 0;
    }
}

/**
 * Convert PS/2 scan code to ASCII character
 * Handles both scan code set 1 and set 2, with proper break code handling
 * 
 * @param scan_code Raw PS/2 scan code from keyboard
 * @return ASCII character (0 if invalid/release code)
 */
static char scancode_to_char(uint8_t scan_code) {
    // Handle extended key prefix (0xE0) - skip it and wait for next byte
    if (scan_code == 0xE0) {
        last_scan_code = 0xE0;
        return 0;
    }
    
    // Ignore scan code set 2 break code prefix (0xF0) if it appears
    // We're using set 1, so this shouldn't happen, but ignore it just in case
    if (scan_code == 0xF0) {
        expecting_break_code = true;
        return 0;
    }
    
    // If we saw 0xF0, ignore the next byte (set 2 break code)
    if (expecting_break_code) {
        expecting_break_code = false;
        return 0;
    }
    
    // Scan code set 1 uses bit 7 (0x80) to indicate key release
    // Check for scan code set 1 release
    bool released = (scan_code & 0x80) != 0;
    uint8_t key_code = scan_code & 0x7F;
    
    // Handle shift keys - Set 1 only (0x2A = left shift, 0x36 = right shift)
    if (key_code == 0x2A || key_code == 0x36) {
        shift_pressed = !released;
        return 0;  // Don't return a character for shift keys
    }
    
    // Ignore key releases for all other keys
    if (released) {
        return 0;
    }
    
    // Explicit handling for Space (0x39), Enter (0x1C), and Backspace (0x0E) to ensure they work
    if (key_code == 0x39) {
        return ' ';  // Space key
    }
    if (key_code == 0x1C) {
        return '\n';  // Enter key
    }
    if (key_code == 0x0E) {
        return '\b';  // Backspace key
    }
    
    // Use scan code set 1 only (QEMU default, numbers work with this)
    return scancode_set1_to_ascii(key_code, shift_pressed);
}

/**
 * ============================================================================
 * Poll Keyboard (Legacy Function - Now Unused)
 * ============================================================================
 * 
 * NOTE: This function is deprecated. Keyboard input is now handled via
 * interrupts (IRQ1). The interrupt handler processes scan codes and fills
 * a buffer, which is read in the main event loop.
 * 
 * This function is kept for reference but is no longer called in the
 * interrupt-driven kernel.
 * ============================================================================
 * 
 * Legacy behavior:
 * - Polled keyboard status port for available data
 * - Read scan codes directly from keyboard port
 * - Converted scan codes to ASCII
 * - Processed input in real-time
 * 
 * @return true if at least one key was processed, false if no key available
 */
bool poll_keyboard() {
    bool processed_any = false;
    
    // Read all available scan codes from keyboard buffer
    while ((inb(KBD_STAT_PORT) & KBD_STATUS_HAVE_DATA) != 0) {
        uint8_t scan_code = inb(KBD_DATA_PORT);
        
        // Process the scan code (this updates shift state even if no character is returned)
        char ascii = scancode_to_char(scan_code);
        
        // Only process if we got a valid character
        if (ascii != 0) {
            processed_any = true;
            
            // Send the input character to the command system for processing
            command_system.process_input(ascii);
            
            // Check if a complete command has been entered (user pressed Enter)
            if (command_system.is_input_complete()) {
                command_system.execute_command();
                command_system.reset_input();
                terminal.write("> ");  // Display prompt for next command (newline already printed by process_input)
            }
        }
    }
    
    return processed_any;
}

/* ============================================================================
 * VGA TEXT MODE HELPERS
 * ============================================================================
/* ============================================================================
 * HARDWARE INITIALIZATION
 * ============================================================================ */

/**
 * Write a string to serial port for debugging
 * 
 * Used during boot to provide status messages.
 * Output appears on COM1 (115200 baud).
 * 
 * @param str Null-terminated string to output
 */
static void serial_write(const char* str) {
    while (*str) {
        outb(SERIAL_PORT, *str);
        // Small delay to ensure output completes
        for (volatile int i = 0; i < DELAY_SHORT; i++);
        str++;
    }
}

/**
 * ============================================================================
 * Initialize Serial Port (COM1)
 * ============================================================================
 * 
 * Configures COM1 (serial port) for debugging output at 115200 baud.
 * Serial output appears on COM1 and can be captured via QEMU's -serial option.
 * 
 * Configuration:
 *   - Port: COM1 (0x3F8)
 *   - Baud rate: 115200 (divisor = 1)
 *   - Data bits: 8
 *   - Parity: None
 *   - Stop bits: 1
 *   - FIFO: Enabled (14-byte threshold)
 * 
 * All kernel initialization messages are sent here for debugging.
 * ============================================================================
 */
static void init_serial() {
    outb(SERIAL_IER, 0x00);        // Disable all interrupts
    outb(SERIAL_LCR, 0x80);        // Enable DLAB (set baud rate divisor)
    outb(SERIAL_PORT, SERIAL_BAUD_DIV);     // Set divisor low byte (115200 baud)
    outb(SERIAL_IER, 0x00);        // Set divisor high byte to 0
    outb(SERIAL_LCR, 0x03);        // Disable DLAB, set 8 bits, no parity, 1 stop
    outb(SERIAL_FCR, 0xC7);        // Enable FIFO, clear it, set level to 14 bytes
}

/**
 * ============================================================================
 * Initialize VGA Text Mode Display
 * ============================================================================
 * 
 * Initializes VGA text mode (80x25 characters) for display output.
 * This function is CRITICAL for QEMU compatibility.
 * 
 * IMPORTANT: Must write to VGA memory buffer FIRST before accessing
 * any VGA control registers. This triggers QEMU's display initialization.
 * 
 * Initialization Steps:
 *   1. Fill entire VGA buffer (80x25 = 2000 characters) with spaces
 *      This ensures the display is initialized and visible
 *   2. Wait for hardware to stabilize (delay loop)
 *   3. Access VGA status register to confirm display is active
 *   4. Set initial cursor position to (0, 0) via CRTC registers
 * 
 * VGA Memory Layout:
 *   - Buffer address: 0xB8000 (linear address)
 *   - Each character: 2 bytes (character byte + attribute byte)
 *   - Attribute format: (BG << 4) | FG
 * ============================================================================
 */
static void init_vga() {
    volatile uint16_t* vga = VGA_BUFFER;
    
    // Step 1: Write to VGA memory to trigger QEMU display initialization
    // MUST happen before any register access
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (uint16_t)' ' | VGA_BLACK_BLACK;  // BLACK text on BLACK bg
    }
    
    // Small delay to ensure writes are processed
    for (volatile int delay = 0; delay < DELAY_MEDIUM; delay++);
    
    // Step 2: Access VGA status register to confirm display is active
    // uint8_t status = inb(VGA_STATUS_PORT);
    // (void)status;  // Suppress unused variable warning
    for (volatile int i = 0; i < DELAY_SHORT; i++);
    
    // Step 3: Set initial cursor position to 0,0 via CRTC registers
    outb(VGA_CRTC_INDEX, CRTC_CURSOR_HIGH);
    outb(VGA_CRTC_DATA, 0x00);
    outb(VGA_CRTC_INDEX, CRTC_CURSOR_LOW);
    outb(VGA_CRTC_DATA, 0x00);
    
    for (volatile int i = 0; i < DELAY_SHORT; i++);
}

/**
 * Set cursor position via VGA CRTC registers
 * Position is calculated as: row * 80 + col
 * 
 * @param row Row number (0-24)
 * @param col Column number (0-79)
 */
static void set_cursor_position(uint8_t row, uint8_t col) {
    uint16_t pos = row * VGA_WIDTH + col;
    
    // Set high byte of cursor position
    outb(VGA_CRTC_INDEX, CRTC_CURSOR_HIGH);
    outb(VGA_CRTC_DATA, (uint8_t)(pos >> 8));
    
    // Set low byte of cursor position
    outb(VGA_CRTC_INDEX, CRTC_CURSOR_LOW);
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    
    for (volatile int i = 0; i < DELAY_SHORT; i++);
}

/**
 * Wait for keyboard controller input buffer to be empty
 * Returns true if ready, false if timeout
 */
static bool wait_kbd_ready() {
    for (volatile int i = 0; i < DELAY_MEDIUM; i++) {
        // Check if input buffer is empty (bit 1 = 0 means input buffer empty, safe to write)
        uint8_t status = inb(KBD_STAT_PORT);
        if ((status & 0x02) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Wait for keyboard data to be available
 * Returns true if data available, false if timeout
 */
static bool wait_kbd_data() {
    for (volatile int i = 0; i < DELAY_MEDIUM; i++) {
        if ((inb(KBD_STAT_PORT) & KBD_STATUS_HAVE_DATA) != 0) {
            return true;
        }
    }
    return false;
}

/**
 * ============================================================================
 * Initialize PS/2 Keyboard Controller
 * ============================================================================
 * 
 * Initializes and clears the PS/2 keyboard controller to ensure a clean state.
 * This function clears any stale scan codes that might be in the keyboard buffer
 * from boot-time key presses.
 * 
 * Initialization Steps:
 *   1. Wait for keyboard controller to be ready
 *   2. Clear keyboard buffer (read and discard up to 10 pending bytes)
 *   3. Reset internal shift state tracking variables
 * 
 * Note: We don't force a specific scan code set since our driver handles
 * both scan code set 1 (standard) and set 2 (modern keyboards).
 * The keyboard interrupt handler (IRQ1) processes all scan codes.
 * ============================================================================
 */
static void init_keyboard() {
    // Wait for keyboard controller to be ready
    for (volatile int i = 0; i < DELAY_MEDIUM; i++);
    
    // Clear any pending data from keyboard buffer
    // This is important to avoid processing stale scan codes
    for (int i = 0; i < 10; i++) {  // Clear up to 10 bytes
        if ((inb(KBD_STAT_PORT) & KBD_STATUS_HAVE_DATA) != 0) {
            inb(KBD_DATA_PORT);
        } else {
            break;
        }
        for (volatile int j = 0; j < DELAY_SHORT; j++);
    }
    
    // Reset shift state
    shift_pressed = false;
    expecting_break_code = false;
    last_scan_code = 0;
}

/* ============================================================================
 * KERNEL MAIN ENTRY POINT
 * ============================================================================ */

/**
 * ============================================================================
 * Kernel Main Entry Point
 * ============================================================================
 * 
 * This is the main entry point called by the loader after entering protected mode.
 * It performs all system initialization and then enters the main event loop.
 * 
 * Initialization Sequence:
 *   1. Initialize serial port (COM1) for debug output
 *   2. Initialize VGA text mode display
 *   3. Set up terminal interface and display welcome screen
 *   4. Initialize interrupt handling (PIC, IDT)
 *   5. Initialize keyboard driver and clear buffer
 *   6. Enable interrupts (STI)
 *   7. Enter main event loop (interrupt-driven)
 * 
 * The kernel runs in 32-bit protected mode with interrupts enabled.
 * Hardware I/O is interrupt-driven (keyboard via IRQ1, timer via IRQ0).
 * ============================================================================
 */
extern "C" void kernel_main() {
    // ========================================================================
    // Phase 1: Serial Port Initialization
    // ========================================================================
    // Initialize serial port for debugging output (COM1, 115200 baud)
    // All initialization messages are sent to serial for debugging purposes
    init_serial();
    serial_write("===== RusticOS Kernel Starting (v1.6.9) =====\n");
    
    // ========================================================================
    // Phase 2: VGA Display Initialization
    // ========================================================================
    // Initialize VGA text mode (80x25 characters)
    // CRITICAL: Must write to VGA buffer BEFORE accessing control registers
    // This triggers QEMU/VGA hardware initialization
    serial_write("Initializing VGA text mode display...\n");
    init_vga();
    
    // ========================================================================
    // Phase 3: Terminal Setup
    // ========================================================================
    // Set up the terminal interface and display welcome screen
    // The clear() function automatically redraws the title bar with version info
    serial_write("Setting up terminal interface...\n");
    terminal.clear();
    
    // Display welcome message and instructions
    terminal.setColor(GREEN, BLACK);
    terminal.writeAt("Welcome to RusticOS v1.6.9!", 0, 2);
    terminal.writeAt("Type 'help' for available commands.", 0, 3);
    terminal.writeAt("Root filesystem mounted at '/'", 0, 4);
    
    // Display initial command prompt
    terminal.writeAt("> ", 0, 5);
    terminal.setCursor(2, 5);  // Position cursor after "> " prompt
    serial_write("Terminal interface ready.\n");
    
    // ========================================================================
    // Phase 4: Interrupt System Initialization
    // ========================================================================
    // Set up interrupt handling for hardware devices
    serial_write("Initializing interrupt handling system...\n");
    init_pic();              // Initialize Programmable Interrupt Controller (remap IRQs)
    init_pit();              // Initialize Programmable Interval Timer (PIT)
    // Note: IDT is already initialized in crt0.s via init_idt() call
    
    // ========================================================================
    // Phase 5: Keyboard Driver Initialization
    // ========================================================================
    // Initialize keyboard driver and clear any stale scan codes
    serial_write("Initializing keyboard driver...\n");
    keyboard.init();         // Initialize keyboard driver internal state
    init_keyboard();         // Clear keyboard buffer and reset controller state
    
    // ========================================================================
    // Phase 6: Enable Interrupts and Start System
    // ========================================================================
    // Enable interrupts (STI) - system is now fully operational
    serial_write("Enabling interrupts...\n");
    enable_interrupts();
    serial_write("===== RusticOS Kernel Ready (Interrupt-driven) =====\n");
    
    // ========================================================================
    // Main Kernel Event Loop
    // ========================================================================
    // This loop processes keyboard events from the interrupt-driven buffer.
    // The keyboard interrupt handler (IRQ1) fills the buffer, and this loop
    // processes events and executes commands. This is much more efficient
    // than polling the keyboard port directly.
    // ========================================================================
    while (true) {
        // Check for keyboard events (filled by interrupt handler)
        KeyEvent event;
        if (keyboard.get_key_event(event)) {
            // Process the key event if it contains a valid ASCII character
            if (event.ascii != 0) {
                // Send the input character to the command system for processing
                command_system.process_input(event.ascii);
                
                // Check if a complete command has been entered (user pressed Enter)
                if (command_system.is_input_complete()) {
                    command_system.execute_command();
                    command_system.reset_input();
                    terminal.write("> ");  // Display prompt for next command
                }
            }
        }
        
        // Small delay to prevent excessive CPU usage when no events are pending
        // This allows the CPU to enter a lower power state between events
        // Much more efficient than the previous polling approach
        for (volatile int i = 0; i < 500; i++);  // Reduced delay: interrupts handle I/O efficiently
    }
}
