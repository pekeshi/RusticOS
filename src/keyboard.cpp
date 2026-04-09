/*
 * ============================================================================
 * RusticOS Keyboard Driver Implementation (keyboard.cpp)
 * ============================================================================
 * 
 * Implements the PS/2 keyboard driver with interrupt-driven input handling.
 * Converts scan codes to ASCII characters and maintains modifier key state.
 * 
 * Features:
 *   - Scan code to ASCII conversion (US QWERTY layout)
 *   - Support for both scan code set 1 and set 2
 *   - Modifier key state tracking (Shift, Ctrl, Alt)
 *   - Circular event buffer for key events
 *   - Real-time key state tracking
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#include "keyboard.h"

// Internal key state table - tracks which keys are currently pressed
// Used for modifier key tracking and key state queries
static bool key_state[256];

KeyboardDriver::KeyboardDriver()
    : head(0), tail(0), shift_pressed(false), ctrl_pressed(false), alt_pressed(false)
{
    for (int i = 0; i < 256; ++i) {
        key_state[i] = false;
    }
}

void KeyboardDriver::init()
{
    head = 0;
    tail = 0;
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    
    for (int i = 0; i < 256; ++i) {
        key_state[i] = false;
    }
}

uint8_t KeyboardDriver::scan_code_to_ascii(uint8_t code, bool shift)
{
    // Minimal US QWERTY layout mapping for set-1 scan codes
    switch (code) {
    // Letters
    case KEY_A: return shift ? 'A' : 'a';
    case KEY_B: return shift ? 'B' : 'b';
    case KEY_C: return shift ? 'C' : 'c';
    case KEY_D: return shift ? 'D' : 'd';
    case KEY_E: return shift ? 'E' : 'e';
    case KEY_F: return shift ? 'F' : 'f';
    case KEY_G: return shift ? 'G' : 'g';
    case KEY_H: return shift ? 'H' : 'h';
    case KEY_I: return shift ? 'I' : 'i';
    case KEY_J: return shift ? 'J' : 'j';
    case KEY_K: return shift ? 'K' : 'k';
    case KEY_L: return shift ? 'L' : 'l';
    case KEY_M: return shift ? 'M' : 'm';
    case KEY_N: return shift ? 'N' : 'n';
    case KEY_O: return shift ? 'O' : 'o';
    case KEY_P: return shift ? 'P' : 'p';
    case KEY_Q: return shift ? 'Q' : 'q';
    case KEY_R: return shift ? 'R' : 'r';
    case KEY_S: return shift ? 'S' : 's';
    case KEY_T: return shift ? 'T' : 't';
    case KEY_U: return shift ? 'U' : 'u';
    case KEY_V: return shift ? 'V' : 'v';
    case KEY_W: return shift ? 'W' : 'w';
    case KEY_X: return shift ? 'X' : 'x';
    case KEY_Y: return shift ? 'Y' : 'y';
    case KEY_Z: return shift ? 'Z' : 'z';

    // Numbers
    case KEY_1: return shift ? '!' : '1';
    case KEY_2: return shift ? '@' : '2';
    case KEY_3: return shift ? '#' : '3';
    case KEY_4: return shift ? '$' : '4';
    case KEY_5: return shift ? '%' : '5';
    case KEY_6: return shift ? '^' : '6';
    case KEY_7: return shift ? '&' : '7';
    case KEY_8: return shift ? '*' : '8';
    case KEY_9: return shift ? '(' : '9';
    case KEY_0: return shift ? ')' : '0';

    // Symbols
    case KEY_MINUS: return shift ? '_' : '-';
    case KEY_EQUALS: return shift ? '+' : '=';
    case KEY_COMMA: return shift ? '<' : ',';
    case KEY_PERIOD: return shift ? '>' : '.';
    case KEY_SLASH: return shift ? '?' : '/';
    case KEY_SEMICOLON: return shift ? ':' : ';';
    case KEY_QUOTE: return shift ? '"' : '\'';
    case KEY_BACKTICK: return shift ? '~' : '`';
    case KEY_BACKSLASH: return shift ? '|' : '\\';
    case KEY_LEFT_BRACKET: return shift ? '{' : '[';
    case KEY_RIGHT_BRACKET: return shift ? '}' : ']';

    // Special keys
    case KEY_SPACE: return ' ';
    case KEY_ENTER: return '\n';
    case KEY_TAB: return '\t';
    case KEY_BACKSPACE: return 0x08;

    default:
        return 0;
    }
}

void KeyboardDriver::handle_interrupt(uint8_t scan_code)
{
    // Top bit indicates release (0x80 set = key released)
    bool released = (scan_code & 0x80) != 0;
    uint8_t code = scan_code & 0x7F;

    // Update modifier state (shift, ctrl, alt)
    if (code == KEY_LEFT_SHIFT || code == KEY_RIGHT_SHIFT) {
        shift_pressed = !released;
    } else if (code == KEY_LEFT_CTRL) {
        ctrl_pressed = !released;
    } else if (code == KEY_LEFT_ALT) {
        alt_pressed = !released;
    }

    // Update global key state table
    key_state[code] = !released;

    // Only queue events for key press (not release)
    if (!released) {
        KeyEvent event;
        event.scan_code = code;
        event.ascii = scan_code_to_ascii(code, shift_pressed);
        event.pressed = true;
        event.shift = shift_pressed;
        event.ctrl = ctrl_pressed;
        event.alt = alt_pressed;

        // Push to circular buffer
        uint32_t next = (head + 1) % KEYBOARD_BUFFER_SIZE;
        if (next == tail) {
            // Buffer full, drop oldest event
            tail = (tail + 1) % KEYBOARD_BUFFER_SIZE;
        }
        buffer[head] = event;
        head = next;
    }
}

bool KeyboardDriver::get_key_event(KeyEvent& event)
{
    if (head == tail) {
        return false; // Buffer empty
    }

    event = buffer[tail];
    tail = (tail + 1) % KEYBOARD_BUFFER_SIZE;
    return true;
}

bool KeyboardDriver::is_key_pressed(uint8_t scan_code)
{
    return key_state[scan_code & 0x7F];
}

// Global keyboard driver instance
KeyboardDriver keyboard;