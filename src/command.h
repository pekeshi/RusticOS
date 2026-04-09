/*
 * ============================================================================
 * RusticOS Command System Header (command.h)
 * ============================================================================
 * 
 * Defines the command-line interface system for parsing and executing user commands.
 * Handles command parsing, argument extraction, and command dispatch.
 * 
 * Features:
 *   - Command parsing with argument support
 *   - Input buffer management with backspace support
 *   - Command execution and dispatch system
 *   - Help system for user assistance
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#ifndef COMMAND_H
#define COMMAND_H

#include "types.h"

// ============================================================================
// Command System Constants
// ============================================================================
#define MAX_COMMAND_LENGTH   256     // Maximum length of command input
#define MAX_ARGS             16      // Maximum number of command arguments

/**
 * Command Structure
 * 
 * Represents a parsed command with its name and arguments.
 * Used internally by the command system for execution.
 */
struct Command {
    char name[64];                  // Command name (e.g., "mkdir", "lsd", "help")
    char args[MAX_ARGS][64];        // Array of argument strings
    uint32_t arg_count;             // Number of arguments provided
};

/**
 * ============================================================================
 * CommandSystem Class
 * ============================================================================
 * 
 * Manages the command-line interface, including input processing, command
 * parsing, and command execution. Integrates with the filesystem and terminal
 * to provide a complete shell experience.
 * ============================================================================
 */
class CommandSystem {
private:
    char input_buffer[MAX_COMMAND_LENGTH];  // Buffer for user input
    uint32_t input_pos;                     // Current position in input buffer
    bool input_complete;                    // Flag: true when Enter is pressed
    Command current_command;                // Parsed command structure
    
    // Private helper functions
    void parse_command(const char* input, Command& cmd);  // Parse input string into command structure
    void clear_command(Command& cmd);                     // Clear/reset command structure
    
public:
    // Constructor
    CommandSystem();
    
    // Input processing
    void process_input(char c);        // Process a single character input (handles backspace, enter, etc.)
    void execute_command();            // Execute the currently parsed command
    void reset_input();                // Reset input buffer and state for next command
    
    // Accessors
    bool is_input_complete() const { return input_complete; }         // Check if command is ready to execute
    const char* get_input_buffer() const { return input_buffer; }     // Get current input buffer
    uint32_t get_input_pos() const { return input_pos; }              // Get current input position
    
    // Command implementations
    void cmd_help();
    void cmd_clear();
    void cmd_echo();
    void cmd_mkdir(const char* name);
    void cmd_cd(const char* path);
    void cmd_ls();
    void cmd_pwd();
    void cmd_touch(const char* name);
    void cmd_cat(const char* name);
    void cmd_write(const char* name, const char* content);
    void cmd_remove(const char* name);
    void cmd_move(const char* src, const char* dest);
    void cmd_copy(const char* src, const char* dest);
    void cmd_time();
    void cmd_shutdown();
};

extern CommandSystem command_system;

#endif // COMMAND_H
