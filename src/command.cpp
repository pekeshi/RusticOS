/*
 * ============================================================================
 * RusticOS Command System Implementation (command.cpp)
 * ============================================================================
 * 
 * Implements the command-line interface for RusticOS. Handles command parsing,
 * input processing, and command execution. All user commands are processed here.
 * 
 * Supported Commands:
 *   - help, clear, echo
 *   - makedir, cd, lsd, pwd
 *   - makefile, cat, write
 *   - remove, move, copy
 *   - shutdown
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#include "command.h"
#include "terminal.h"
#include "filesystem.h"
#include "interrupt.h"

extern Terminal terminal;
extern FileSystem filesystem;

CommandSystem::CommandSystem()
    : input_pos(0), input_complete(false)
{
    for (int i = 0; i < MAX_COMMAND_LENGTH; ++i) {
        input_buffer[i] = '\0';
    }
    clear_command(current_command);
}

void CommandSystem::process_input(char c)
{
    // Handle backspace/delete
    if (c == '\b' || c == 0x08 || c == 0x7F) {
        if (input_pos > 0) {
            input_pos--;
            input_buffer[input_pos] = '\0';
            // Erase the character: move cursor back, write space, move cursor back again
            terminal.putChar('\b');  // Move cursor back
            terminal.putChar(' ');   // Erase character with space
            terminal.putChar('\b');  // Move cursor back again
        }
        return;
    }
    
    // Handle Enter/Return key
    if (c == '\n' || c == '\r') {
        input_complete = true;
        terminal.write("\n");
        return;
    }
    
    // Handle printable characters (including space)
    if (c >= 32 && c <= 126 && input_pos < MAX_COMMAND_LENGTH - 1) {
        input_buffer[input_pos++] = c;
        input_buffer[input_pos] = '\0';
        char str[2] = {c, '\0'};
        terminal.write(str);
    }
}

void CommandSystem::execute_command()
{
    parse_command(input_buffer, current_command);
    
    if (current_command.name[0] == '\0') {
        return;
    }
    
    // Command dispatch
    if (strcmp(current_command.name, "help") == 0) {
        cmd_help();
    } else if (strcmp(current_command.name, "clear") == 0) {
        cmd_clear();
    } else if (strcmp(current_command.name, "echo") == 0) {
        cmd_echo();
    } else if (strcmp(current_command.name, "makedir") == 0) {
        if (current_command.arg_count >= 1) {
            cmd_mkdir(current_command.args[0]);
        }
    } else if (strcmp(current_command.name, "cd") == 0) {
        if (current_command.arg_count >= 1) {
            cmd_cd(current_command.args[0]);
        }
    } else if (strcmp(current_command.name, "lsd") == 0) {
        cmd_ls();
    } else if (strcmp(current_command.name, "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(current_command.name, "makefile") == 0) {
        if (current_command.arg_count >= 1) {
            cmd_touch(current_command.args[0]);
        }
    } else if (strcmp(current_command.name, "cat") == 0) {
        if (current_command.arg_count >= 1) {
            cmd_cat(current_command.args[0]);
        }
    } else if (strcmp(current_command.name, "write") == 0) {
        if (current_command.arg_count >= 2) {
            char content[256] = {0};
            uint32_t pos = 0;
            for (uint32_t ai = 1; ai < current_command.arg_count && pos < 255; ++ai) {
                const char* part = current_command.args[ai];
                for (uint32_t pi = 0; part[pi] && pos < 255; ++pi) {
                    content[pos++] = part[pi];
                }
                if (ai + 1 < current_command.arg_count && pos < 255) {
                    content[pos++] = ' ';
                }
            }
            content[pos] = '\0';
            cmd_write(current_command.args[0], content);
        }
    } else if (strcmp(current_command.name, "remove") == 0) {
        if (current_command.arg_count >= 1) {
            cmd_remove(current_command.args[0]);
        } else {
            terminal.write("Usage: remove <filename>\n");
        }
    } else if (strcmp(current_command.name, "move") == 0) {
        if (current_command.arg_count >= 2) {
            cmd_move(current_command.args[0], current_command.args[1]);
        } else {
            terminal.write("Usage: move <source> <destination>\n");
        }
    } else if (strcmp(current_command.name, "copy") == 0) {
        if (current_command.arg_count >= 2) {
            cmd_copy(current_command.args[0], current_command.args[1]);
        } else {
            terminal.write("Usage: copy <source> <destination>\n");
        }
    } else if (strcmp(current_command.name, "time") == 0) {
        cmd_time();
    } else if (strcmp(current_command.name, "shutdown") == 0) {
        cmd_shutdown();
    } else {
        terminal.write("Unknown command: ");
        terminal.write(current_command.name);
        terminal.write("\n");
    }
}

void CommandSystem::reset_input()
{
    input_pos = 0;
    input_complete = false;
    for (int i = 0; i < MAX_COMMAND_LENGTH; ++i) {
        input_buffer[i] = '\0';
    }
    clear_command(current_command);
}

void CommandSystem::parse_command(const char* input, Command& cmd)
{
    clear_command(cmd);
    
    uint32_t i = 0;
    uint32_t arg_index = 0;
    
    // Parse command name
    uint32_t np = 0;
    while (input[i] && input[i] != ' ' && np < 63) {
        cmd.name[np++] = input[i++];
    }
    cmd.name[np] = '\0';
    
    // Skip spaces
    while (input[i] == ' ') i++;
    
    // Parse arguments
    while (input[i] && arg_index < MAX_ARGS) {
        uint32_t ap = 0;
        while (input[i] && input[i] != ' ' && ap < 63) {
            cmd.args[arg_index][ap++] = input[i++];
        }
        cmd.args[arg_index][ap] = '\0';
        
        if (ap > 0) {
            arg_index++;
        }
        
        while (input[i] == ' ') i++;
    }
    
    cmd.arg_count = arg_index;
}

void CommandSystem::clear_command(Command& cmd)
{
    cmd.name[0] = '\0';
    cmd.arg_count = 0;
    for (int i = 0; i < MAX_ARGS; ++i) {
        cmd.args[i][0] = '\0';
    }
}

/**
 * Helper function to convert uint64_t to string
 * @param value The number to convert
 * @param buffer Buffer to store the string (must be at least 32 bytes)
 */
static void uint64_to_string(uint64_t value, char* buffer) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    char rev_buf[32];
    int rev_pos = 0;
    uint64_t temp = value;
    
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

// Stub implementations
void CommandSystem::cmd_help() {
    terminal.write("Available commands:\n");
    terminal.write("  help, clear, echo\n");
    terminal.write("  makedir - Create directory\n");
    terminal.write("  cd - Change directory\n");
    terminal.write("  lsd - List directory\n");
    terminal.write("  pwd - Print working directory\n");
    terminal.write("  makefile - Create file\n");
    terminal.write("  cat - Display file contents\n");
    terminal.write("  write - Write to file\n");
    terminal.write("  remove - Remove file or empty directory\n");
    terminal.write("  move - Move/rename file or directory\n");
    terminal.write("  copy - Copy file\n");
    terminal.write("  time - Display system clock (uptime) and real-time clock\n");
    terminal.write("  shutdown - Shutdown the system\n");
}

void CommandSystem::cmd_clear() {
    terminal.clear();
}

void CommandSystem::cmd_echo() {
    if (current_command.arg_count > 0) {
        for (uint32_t i = 0; i < current_command.arg_count; ++i) {
            terminal.write(current_command.args[i]);
            if (i + 1 < current_command.arg_count) terminal.write(" ");
        }
    }
    terminal.write("\n");
}

void CommandSystem::cmd_mkdir(const char* name) {
    if (filesystem.mkdir(name)) {
        terminal.write("Directory created: ");
        terminal.write(name);
        terminal.write("\n");
    } else {
        terminal.write("Error: could not create directory ");
        terminal.write(name);
        terminal.write("\n");
    }
}

void CommandSystem::cmd_cd(const char* path) {
    filesystem.cd(path);
}

void CommandSystem::cmd_ls() {
    filesystem.ls();
}

void CommandSystem::cmd_pwd() {
    filesystem.pwd();
}

void CommandSystem::cmd_touch(const char* name) {
    if (filesystem.create_file(name, "")) {
        terminal.write("File created: ");
        terminal.write(name);
        terminal.write("\n");
    } else {
        terminal.write("Error: could not create file ");
        terminal.write(name);
        terminal.write("\n");
    }
}

void CommandSystem::cmd_cat(const char* name) {
    char buffer[512] = {0};
    if (filesystem.read_file(name, buffer, 511)) {
        terminal.write(buffer);
        terminal.write("\n");
    }
}

void CommandSystem::cmd_write(const char* name, const char* content) {
    filesystem.write_file(name, content);
}

void CommandSystem::cmd_remove(const char* name) {
    if (filesystem.remove(name)) {
        terminal.write("Removed: ");
        terminal.write(name);
        terminal.write("\n");
    } else {
        terminal.write("Error: could not remove ");
        terminal.write(name);
        terminal.write("\n");
    }
}

void CommandSystem::cmd_move(const char* src, const char* dest) {
    if (filesystem.move(src, dest)) {
        terminal.write("Moved: ");
        terminal.write(src);
        terminal.write(" -> ");
        terminal.write(dest);
        terminal.write("\n");
    } else {
        terminal.write("Error: could not move ");
        terminal.write(src);
        terminal.write(" to ");
        terminal.write(dest);
        terminal.write("\n");
    }
}

void CommandSystem::cmd_copy(const char* src, const char* dest) {
    if (filesystem.copy_file(src, dest)) {
        terminal.write("Copied: ");
        terminal.write(src);
        terminal.write(" -> ");
        terminal.write(dest);
        terminal.write("\n");
    } else {
        terminal.write("Error: could not copy ");
        terminal.write(src);
        terminal.write(" to ");
        terminal.write(dest);
        terminal.write("\n");
    }
}

void CommandSystem::cmd_time() {
    char num_buf[32];
    char time_buf[64];
    
    // Display system clock information (uptime)
    terminal.write("System Clock (Uptime):\n");
    
    uint64_t ticks = get_ticks();
    uint64_t seconds = get_seconds();
    uint64_t milliseconds = get_milliseconds();
    
    // Display ticks
    terminal.write("  Ticks: ");
    uint64_to_string(ticks, num_buf);
    terminal.write(num_buf);
    terminal.write("\n");
    
    // Display seconds
    terminal.write("  Seconds: ");
    uint64_to_string(seconds, num_buf);
    terminal.write(num_buf);
    terminal.write("\n");
    
    // Display milliseconds
    terminal.write("  Milliseconds: ");
    uint64_to_string(milliseconds, num_buf);
    terminal.write(num_buf);
    terminal.write("\n");
    
    // Display Real-Time Clock
    terminal.write("\nReal-Time Clock:\n");
    RTCTime rtc_time;
    if (get_rtc_time(&rtc_time)) {
        // Format time as HH:MM:SS
        time_buf[0] = '0' + (rtc_time.hour / 10);
        time_buf[1] = '0' + (rtc_time.hour % 10);
        time_buf[2] = ':';
        time_buf[3] = '0' + (rtc_time.minute / 10);
        time_buf[4] = '0' + (rtc_time.minute % 10);
        time_buf[5] = ':';
        time_buf[6] = '0' + (rtc_time.second / 10);
        time_buf[7] = '0' + (rtc_time.second % 10);
        time_buf[8] = '\0';
        
        terminal.write("  Time: ");
        terminal.write(time_buf);
        terminal.write("\n");
        
        // Format date as YYYY-MM-DD or MM/DD/YY
        if (rtc_time.century > 0) {
            // Full year format: YYYY-MM-DD
            uint64_to_string((uint64_t)rtc_time.century * 100 + rtc_time.year, num_buf);
            terminal.write("  Date: ");
            terminal.write(num_buf);
            terminal.write("-");
            if (rtc_time.month < 10) terminal.write("0");
            uint64_to_string((uint64_t)rtc_time.month, num_buf);
            terminal.write(num_buf);
            terminal.write("-");
            if (rtc_time.day < 10) terminal.write("0");
            uint64_to_string((uint64_t)rtc_time.day, num_buf);
            terminal.write(num_buf);
            terminal.write("\n");
        } else {
            // Short format: MM/DD/YY
            terminal.write("  Date: ");
            if (rtc_time.month < 10) terminal.write("0");
            uint64_to_string((uint64_t)rtc_time.month, num_buf);
            terminal.write(num_buf);
            terminal.write("/");
            if (rtc_time.day < 10) terminal.write("0");
            uint64_to_string((uint64_t)rtc_time.day, num_buf);
            terminal.write(num_buf);
            terminal.write("/");
            if (rtc_time.year < 10) terminal.write("0");
            uint64_to_string((uint64_t)rtc_time.year, num_buf);
            terminal.write(num_buf);
            terminal.write("\n");
        }
    } else {
        terminal.write("  Error: Could not read RTC\n");
    }
}

void CommandSystem::cmd_shutdown() {
    terminal.write("Shutting down RusticOS...\n");
    terminal.write("System halted.\n");
    
    // Disable interrupts
    disable_interrupts();
    
    // Method 1: Try QEMU isa-debug-exit device (port 0xf4)
    // Exit code 0x31 = 49 (non-zero exit code for QEMU)
    asm volatile("outl %0, %1" : : "a"((uint32_t)0x31), "Nd"((uint16_t)0xf4));
    
    // Small delay to allow exit to process
    for (volatile int i = 0; i < 10000; i++);
    
    // Method 2: Try ACPI shutdown (port 0x604)
    // Write 0x2000 to ACPI PM1a_CNT register to trigger shutdown
    asm volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    
    // Small delay to allow shutdown to process
    for (volatile int i = 0; i < 10000; i++);
    
    // If neither worked, halt the CPU (fallback)
    for (;;) {
        asm volatile("cli; hlt");
    }
}

CommandSystem command_system;
