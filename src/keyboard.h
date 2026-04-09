/*
 * ============================================================================
 * RusticOS Keyboard Driver Header (keyboard.h)
 * ============================================================================
 * 
 * Defines the PS/2 keyboard driver interface for RusticOS.
 * Handles keyboard input via interrupts (IRQ1) and provides a clean
 * event-based interface for key presses.
 * 
 * Features:
 *   - Interrupt-driven keyboard input (IRQ1)
 *   - Scan code to ASCII conversion (handles both scan code set 1 and 2)
 *   - Modifier key tracking (Shift, Ctrl, Alt)
 *   - Event buffer for asynchronous key event handling
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

/**
 * Key Event Structure
 * 
 * Represents a keyboard event with scan code, ASCII character, and modifier states.
 * Used to pass keyboard events from the interrupt handler to the main kernel loop.
 */
struct KeyEvent {
    uint8_t scan_code;  // Raw scan code from keyboard
    uint8_t ascii;      // Converted ASCII character (0 if not printable)
    bool    pressed;    // true if key was pressed, false if released
    bool    shift;      // Shift modifier state at time of event
    bool    ctrl;       // Ctrl modifier state at time of event
    bool    alt;        // Alt modifier state at time of event
};

// Subset of set-1 scan codes used by the driver
enum KeyCode {
    KEY_ESCAPE = 0x01,
    KEY_1 = 0x02, KEY_2 = 0x03, KEY_3 = 0x04, KEY_4 = 0x05, KEY_5 = 0x06,
    KEY_6 = 0x07, KEY_7 = 0x08, KEY_8 = 0x09, KEY_9 = 0x0A, KEY_0 = 0x0B,
    KEY_MINUS = 0x0C, KEY_EQUALS = 0x0D, KEY_BACKSPACE = 0x0E,
    KEY_TAB = 0x0F,
    KEY_Q = 0x10, KEY_W = 0x11, KEY_E = 0x12, KEY_R = 0x13, KEY_T = 0x14,
    KEY_Y = 0x15, KEY_U = 0x16, KEY_I = 0x17, KEY_O = 0x18, KEY_P = 0x19,
    KEY_LEFT_BRACKET = 0x1A, KEY_RIGHT_BRACKET = 0x1B, KEY_ENTER = 0x1C,
    KEY_LEFT_CTRL = 0x1D,
    KEY_A = 0x1E, KEY_S = 0x1F, KEY_D = 0x20, KEY_F = 0x21, KEY_G = 0x22,
    KEY_H = 0x23, KEY_J = 0x24, KEY_K = 0x25, KEY_L = 0x26,
    KEY_SEMICOLON = 0x27, KEY_QUOTE = 0x28, KEY_BACKTICK = 0x29,
    KEY_LEFT_SHIFT = 0x2A, KEY_BACKSLASH = 0x2B,
    KEY_Z = 0x2C, KEY_X = 0x2D, KEY_C = 0x2E, KEY_V = 0x2F,
    KEY_B = 0x30, KEY_N = 0x31, KEY_M = 0x32,
    KEY_COMMA = 0x33, KEY_PERIOD = 0x34, KEY_SLASH = 0x35,
    KEY_RIGHT_SHIFT = 0x36,
    KEY_LEFT_ALT = 0x38, KEY_SPACE = 0x39, KEY_CAPS_LOCK = 0x3A,
    KEY_F1 = 0x3B, KEY_F2 = 0x3C, KEY_F3 = 0x3D, KEY_F4 = 0x3E,
    KEY_F5 = 0x3F, KEY_F6 = 0x40, KEY_F7 = 0x41, KEY_F8 = 0x42,
    KEY_F9 = 0x43, KEY_F10 = 0x44,
    KEY_NUM_LOCK = 0x45, KEY_SCROLL_LOCK = 0x46,
    KEY_KEYPAD_7 = 0x47, KEY_KEYPAD_8 = 0x48, KEY_KEYPAD_9 = 0x49,
    KEY_KEYPAD_MINUS = 0x4A, KEY_KEYPAD_4 = 0x4B, KEY_KEYPAD_5 = 0x4C,
    KEY_KEYPAD_6 = 0x4D, KEY_KEYPAD_PLUS = 0x4E, KEY_KEYPAD_1 = 0x4F,
    KEY_KEYPAD_2 = 0x50, KEY_KEYPAD_3 = 0x51, KEY_KEYPAD_0 = 0x52,
    KEY_KEYPAD_DECIMAL = 0x53, KEY_F11 = 0x57, KEY_F12 = 0x58
};

class KeyboardDriver {
private:
    static const uint32_t KEYBOARD_BUFFER_SIZE = 256;
    KeyEvent buffer[KEYBOARD_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    bool shift_pressed;
    bool ctrl_pressed;
    bool alt_pressed;

    uint8_t scan_code_to_ascii(uint8_t scan_code, bool shift);

public:
    KeyboardDriver();

    void init();
    void handle_interrupt(uint8_t scan_code);
    bool get_key_event(KeyEvent& event);
    bool is_key_pressed(uint8_t scan_code);
    bool is_shift_pressed() const { return shift_pressed; }
    bool is_ctrl_pressed()  const { return ctrl_pressed; }
    bool is_alt_pressed()   const { return alt_pressed; }
};

extern KeyboardDriver keyboard;

#endif // KEYBOARD_H