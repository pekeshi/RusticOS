/*
 * RusticOS Terminal (VGA text-mode)
 * ---------------------------------
 * Provides a simple console abstraction on top of VGA text mode. Includes
 * cursor management, scrolling, and basic line input helpers.
 */

#include "terminal.h"

// ----------------------------------------------------------------------------
// Port I/O helpers
// ----------------------------------------------------------------------------
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Basic memory routines (freestanding)
static inline void* my_memset(void* ptr, int value, uint32_t num) {
    unsigned char* p = (unsigned char*)ptr;
    for (uint32_t i = 0; i < num; ++i) {
        p[i] = (unsigned char)value;
    }
    return ptr;
}

static inline void* my_memcpy(void* destination, const void* source, uint32_t num) {
    unsigned char* dest = (unsigned char*)destination;
    const unsigned char* src = (const unsigned char*)source;
    for (uint32_t i = 0; i < num; ++i) {
        dest[i] = src[i];
    }
    return destination;
}

// ----------------------------------------------------------------------------
// Global terminal instance and VGA buffer mapping
// ----------------------------------------------------------------------------
Terminal terminal;
volatile uint16_t* const Terminal::VGA_BUFFER = (volatile uint16_t*)0xB8000;

Terminal::Terminal()
    : cursor_x(0), cursor_y(0), foreground_color(LIGHT_GREY), background_color(BLACK),
      cursor_visible(true), scroll_offset(0), input_pos(0), input_mode(false) {
    clear();
}

// Redraw title bar (always at row 0)
// This function is called to ensure the title bar is always visible
static void redraw_title_bar() {
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    uint8_t attr = (GREEN << 4) | BLACK;  // Green background, black text
    
    // Fill row 0 with spaces
    for (uint16_t x = 0; x < 80; ++x) {
        vga[x] = (uint16_t)' ' | ((uint16_t)attr << 8);
    }
    
    // Write title bar text directly to VGA buffer
    const char* title = "RusticOS";
    const char* level = "Level:Kernel";
    const char* version = "Version:1.6.9";
    
    // Write "RusticOS" at column 15
    for (uint16_t i = 0; title[i] != '\0' && (15 + i) < 80; ++i) {
        vga[15 + i] = (uint16_t)title[i] | ((uint16_t)attr << 8);
    }
    // Write "Level:Kernel" at column 31
    for (uint16_t i = 0; level[i] != '\0' && (31 + i) < 80; ++i) {
        vga[31 + i] = (uint16_t)level[i] | ((uint16_t)attr << 8);
    }
    // Write "Version:1.6.9" at column 51

    
    for (uint16_t i = 0; version[i] != '\0' && (51 + i) < 80; ++i) {
        vga[51 + i] = (uint16_t)version[i] | ((uint16_t)attr << 8);
    }
}

void Terminal::clear() {
    // Fill the screen with spaces using current colors (skip row 0 - title bar)
    // Attribute byte format: BLINK|BG[2:0]|INTENSITY|FG[2:0]
    // Correct encoding: (BG << 4 | FG) << 8
    for (uint32_t i = VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        uint16_t attr = ((uint16_t)(background_color << 4) | (uint16_t)foreground_color) << 8;
        VGA_BUFFER[i] = (uint16_t)' ' | attr;
    }
    // Always redraw title bar after clearing
    redraw_title_bar();
    // Set cursor to row 1 (not 0, to avoid title bar)
    cursor_x = 0;
    cursor_y = 1;
    update_cursor();
}

void Terminal::setColor(uint8_t fg, uint8_t bg) {
    foreground_color = fg;
    background_color = bg;
}

void Terminal::putChar(char c) {
    // Handle control characters
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            scrollUp(1);
            cursor_y = VGA_HEIGHT - 1;
        }
        // Ensure cursor doesn't go to title bar
        if (cursor_y == 0) {
            cursor_y = 1;
        }
        // Always redraw title bar to ensure it's visible
        redraw_title_bar();
        update_cursor();
        return;
    }

    if (c == '\r') {
        cursor_x = 0;
        update_cursor();
        return;
    }

    if (c == '\b') {
        // Backspace: move cursor back one position
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 1) {
            // Wrap to end of previous line (but not title bar)
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        update_cursor();
        return;
    }

    if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7; // Align to 8-character boundary
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= VGA_HEIGHT) {
                scrollUp(1);
                cursor_y = VGA_HEIGHT - 1;
                redraw_title_bar();
            }
        }
        update_cursor();
        return;
    }

    // Advance to next line if end of row reached
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            scrollUp(1);
            cursor_y = VGA_HEIGHT - 1;
        }
        // Ensure cursor doesn't go to title bar
        if (cursor_y == 0) {
            cursor_y = 1;
        }
        redraw_title_bar();
    }

    // Don't draw on title bar (row 0)
    if (cursor_y == 0) {
        cursor_y = 1;
    }

    // Draw the character at current cursor position
    const uint32_t index = cursor_y * VGA_WIDTH + cursor_x;
    if (index < VGA_WIDTH * VGA_HEIGHT && cursor_y > 0) {
        uint8_t attr = (background_color << 4) | foreground_color;
        VGA_BUFFER[index] = (uint16_t)c | ((uint16_t)attr << 8);
    }

    cursor_x++;
    update_cursor();
}

void Terminal::write(const char* str) {
    // Write a null-terminated string
    for (uint32_t i = 0; str[i] != '\0'; ++i) {
        putChar(str[i]);
    }
}

void Terminal::writeAt(const char* str, uint16_t x, uint16_t y) {
    // Write a string starting at position (x, y) without disturbing cursor state
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) return;
    // Don't write to title bar (row 0)
    if (y == 0) return;

    uint16_t old_x = cursor_x, old_y = cursor_y;
    setCursor(x, y);

    for (uint32_t i = 0; str[i] != '\0' && cursor_x < VGA_WIDTH; ++i) {
        putChar(str[i]);
    }

    setCursor(old_x, old_y);
    // Always redraw title bar after writing
    redraw_title_bar();
}

void Terminal::setCursor(uint16_t x, uint16_t y) {
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
    if (y == 0) y = 1;  // Don't allow cursor on title bar (row 0)

    cursor_x = x;
    cursor_y = y;
    update_cursor();
}

void Terminal::moveCursor(int16_t dx, int16_t dy) {
    int32_t new_x = cursor_x + dx;
    int32_t new_y = cursor_y + dy;

    if (new_x < 0) new_x = 0;
    if (new_x >= VGA_WIDTH) new_x = VGA_WIDTH - 1;
    if (new_y < 0) new_y = 0;
    if (new_y >= VGA_HEIGHT) new_y = VGA_HEIGHT - 1;

    setCursor(new_x, new_y);
}

void Terminal::showCursor(bool show) {
    cursor_visible = show;
    update_cursor();
}

void Terminal::update_cursor() {
    // Program VGA hardware cursor to follow cursor_x/cursor_y
    uint16_t pos = (uint16_t)(cursor_y * VGA_WIDTH + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void Terminal::scrollUp(uint16_t lines) {
    if (lines == 0) return;

    // Move content up by 'lines' (skip row 0 which is the title bar)
    for (uint16_t y = 1; y < VGA_HEIGHT - lines; ++y) {
        for (uint16_t x = 0; x < VGA_WIDTH; ++x) {
            uint32_t src_index = (y + lines) * VGA_WIDTH + x;
            uint32_t dst_index = y * VGA_WIDTH + x;
            VGA_BUFFER[dst_index] = VGA_BUFFER[src_index];
        }
    }

    // Clear bottom lines (but not row 0)
    for (uint16_t y = VGA_HEIGHT - lines; y < VGA_HEIGHT; ++y) {
        if (y > 0) {  // Don't clear the title bar
            clear_line(y);
        }
    }

    // Redraw title bar to ensure it stays visible
    redraw_title_bar();

    // Adjust cursor (ensure it doesn't go to row 0)
    if (cursor_y >= lines) {
        cursor_y -= lines;
        if (cursor_y == 0) {
            cursor_y = 1;  // Don't allow cursor on title bar
        }
    } else {
        cursor_y = 1;  // Don't go to row 0 (title bar)
    }

    update_cursor();
}

void Terminal::scrollDown(uint16_t lines) {
    if (lines == 0) return;

    // Move content down by 'lines'
    for (int16_t y = VGA_HEIGHT - 1; y >= lines; --y) {
        for (uint16_t x = 0; x < VGA_WIDTH; ++x) {
            uint32_t src_index = (y - lines) * VGA_WIDTH + x;
            uint32_t dst_index = y * VGA_WIDTH + x;
            VGA_BUFFER[dst_index] = VGA_BUFFER[src_index];
        }
    }

    // Clear top lines
    for (uint16_t y = 0; y < lines; ++y) {
        clear_line(y);
    }

    // Adjust cursor
    cursor_y += lines;
    if (cursor_y >= VGA_HEIGHT) {
        cursor_y = VGA_HEIGHT - 1;
    }

    update_cursor();
}

void Terminal::clear_line(uint16_t y) {
    for (uint16_t x = 0; x < VGA_WIDTH; ++x) {
        uint32_t index = y * VGA_WIDTH + x;
        uint16_t attr = ((uint16_t)(background_color << 4) | (uint16_t)foreground_color) << 8;
        VGA_BUFFER[index] = (uint16_t)' ' | attr;
    }
}

void Terminal::enableInput(bool enable) {
    input_mode = enable;
    if (enable) {
        input_pos = 0;
        my_memset(input_buffer, 0, INPUT_BUFFER_SIZE);
    }
}

bool Terminal::getInput(char* buffer, uint16_t max_length) {
    if (!input_mode) return false;

    uint16_t len = (input_pos < max_length - 1) ? input_pos : max_length - 1;
    my_memcpy(buffer, input_buffer, len);
    buffer[len] = '\0';

    return true;
}

void Terminal::processKeyEvent(const KeyEvent& event) {
    if (!input_mode) return;

    if (event.pressed) {
        if (event.ascii >= 32 && event.ascii <= 126) {
            // Printable character
            if (input_pos < INPUT_BUFFER_SIZE - 1) {
                input_buffer[input_pos++] = event.ascii;
                putChar(event.ascii);
            }
        } else if (event.scan_code == KEY_BACKSPACE) {
            if (input_pos > 0) {
                input_pos--;
                input_buffer[input_pos] = '\0';
                moveCursor(-1, 0);
                putChar(' ');
                moveCursor(-1, 0);
            }
        } else if (event.scan_code == KEY_ENTER) {
            // Finish line input
            putChar('\n');
            input_mode = false;
        }
    }
}

void Terminal::drawBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, char border_char) {
    if (x1 >= VGA_WIDTH || y1 >= VGA_HEIGHT || x2 >= VGA_WIDTH || y2 >= VGA_HEIGHT) return;

    // Horizontal borders
    for (uint16_t x = x1; x <= x2; ++x) {
        writeAt(&border_char, x, y1);
        writeAt(&border_char, x, y2);
    }

    // Vertical borders
    for (uint16_t y = y1; y <= y2; ++y) {
        writeAt(&border_char, x1, y);
        writeAt(&border_char, x2, y);
    }
}

void Terminal::fillArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, char fill_char) {
    if (x1 >= VGA_WIDTH || y1 >= VGA_HEIGHT || x2 >= VGA_WIDTH || y2 >= VGA_HEIGHT) return;

    for (uint16_t y = y1; y <= y2; ++y) {
        for (uint16_t x = x1; x <= x2; ++x) {
            writeAt(&fill_char, x, y);
        }
    }
}
