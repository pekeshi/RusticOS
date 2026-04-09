/*
 * ============================================================================
 * RusticOS Terminal Interface Header (terminal.h)
 * ============================================================================
 * 
 * Defines the VGA text-mode terminal interface for RusticOS.
 * Provides a high-level abstraction over VGA hardware for console output.
 * 
 * Features:
 *   - VGA text mode (80x25 characters) with color support
 *   - Cursor management and positioning
 *   - Text output with scrolling support
 *   - Color attributes for foreground and background
 *   - Title bar with version information (always displayed)
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include "types.h"
#include "keyboard.h"

// VGA color palette (foreground/background attributes)
enum TerminalColor {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    BROWN = 6,
    LIGHT_GREY = 7,
    DARK_GREY = 8,
    LIGHT_BLUE = 9,
    LIGHT_GREEN = 10,
    LIGHT_CYAN = 11,
    LIGHT_RED = 12,
    LIGHT_MAGENTA = 13,
    LIGHT_BROWN = 14,
    WHITE = 15
};

// Enhanced terminal class
class Terminal {
private:
    static const uint16_t VGA_WIDTH = 80;
    static const uint16_t VGA_HEIGHT = 25;
    static volatile uint16_t* const VGA_BUFFER; // mapped at 0xB8000

    uint16_t cursor_x;               // current cursor column (0..79)
    uint16_t cursor_y;               // current cursor row (0..24)
    uint8_t foreground_color;        // text color
    uint8_t background_color;        // background color
    bool cursor_visible;             // whether to render the cursor

    // Simple scroll buffer (reserved for future use)
    static const uint16_t SCROLL_BUFFER_SIZE = VGA_HEIGHT * 2;
    uint16_t scroll_buffer[SCROLL_BUFFER_SIZE];
    uint16_t scroll_offset;

    // Input buffer for simple line input mode
    static const uint16_t INPUT_BUFFER_SIZE = 256;
    char input_buffer[INPUT_BUFFER_SIZE];
    uint16_t input_pos;
    bool input_mode;

    // Internal helpers
    void scroll_up();
    void scroll_down();
    void update_cursor();
    void clear_line(uint16_t y);

public:
    Terminal();

    // Basic terminal output
    void clear();
    void setColor(uint8_t fg, uint8_t bg = BLACK);
    void putChar(char c);
    void write(const char* str);
    void writeAt(const char* str, uint16_t x, uint16_t y);

    // Cursor control
    void setCursor(uint16_t x, uint16_t y);
    void moveCursor(int16_t dx, int16_t dy);
    void showCursor(bool show);
    void blinkCursor();

    // Scrolling API (thin wrappers around internal functions)
    void scrollUp(uint16_t lines = 1);
    void scrollDown(uint16_t lines = 1);
    void setScrollOffset(uint16_t offset);

    // Input handling
    void enableInput(bool enable);
    bool getInput(char* buffer, uint16_t max_length);
    void processKeyEvent(const KeyEvent& event);

    // Query terminal state
    uint16_t getWidth() const { return VGA_WIDTH; }
    uint16_t getHeight() const { return VGA_HEIGHT; }
    uint16_t getCursorX() const { return cursor_x; }
    uint16_t getCursorY() const { return cursor_y; }

    // Drawing helpers
    void drawBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, char border_char = '#');
    void fillArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, char fill_char = ' ');
};

// Global terminal instance
extern Terminal terminal;

#endif // TERMINAL_H
