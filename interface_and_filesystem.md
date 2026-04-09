# RusticOS Command-Line Interface and Filesystem

**Version: 1.6.9**

## Overview
RusticOS includes a fully functional command-line interface with a hierarchical in-memory filesystem. Users can type commands at the kernel prompt and interact with a root filesystem mounted at "/". The system uses interrupt-driven keyboard input for responsive user interaction.

## Features Implemented

### Command-Line Interface
- **Interactive prompt**: The kernel displays a "> " prompt where users can type commands
- **Interrupt-driven input**: Keyboard input handled via IRQ1 interrupts (no polling)
- **Full keyboard support**: Character input, backspace, enter, and modifier keys
- **Command execution**: Commands are parsed and executed in real-time
- **Error handling**: Invalid commands show appropriate error messages
- **Command names**: All commands use full names (e.g., `makedir`, `makefile`, `lsd`) for clarity

### Filesystem
- **Root directory**: A filesystem mounted at "/" (root)
- **Directory operations**: Create, navigate, and remove directories
- **File operations**: Create, read, write, remove, move, and copy files
- **Dynamic memory allocation**: Uses dynamic allocation with `new`/`delete` for filesystem nodes
- **Hierarchical structure**: Supports parent-child directory relationships with full path resolution
- **Working directory tracking**: Full path resolution with `pwd` command

### Available Commands

#### `help`
- **Usage**: `help`
- **Description**: Lists all available commands with brief descriptions
- **Example**: 
  ```
  > help
  Available commands: help, clear, echo, makedir, cd, lsd, pwd, makefile, cat, write, remove, move, copy, shutdown
  ```

#### `makedir`
- **Usage**: `makedir <directory_name>`
- **Description**: Creates a new directory in the current location
- **Example**:
  ```
  > makedir testdir
  Directory created: testdir
  ```

#### `cd`
- **Usage**: `cd <path>`
- **Description**: Changes the current directory
- **Special paths**:
  - `cd /` - Go to root directory
  - `cd ..` - Go to parent directory
- **Example**:
  ```
  > cd testdir
  > cd ..
  > cd /
  ```

#### `pwd`
- **Usage**: `pwd`
- **Description**: Prints the current working directory with full path resolution
- **Example**:
  ```
  > cd projects
  > cd myproject
  > pwd
  /projects/myproject
  ```

#### `lsd`
- **Usage**: `lsd` (short for "list directory")
- **Description**: Lists contents of the current directory. Directories are shown with a trailing "/"
- **Example**:
  ```
  > lsd
  testdir/
  myfile.txt
  ```

#### `clear`
- **Usage**: `clear`
- **Description**: Clears the screen and redraws the header
- **Example**:
  ```
  > clear
  [Screen clears and shows fresh prompt]
  ```

#### `echo`
- **Usage**: `echo <text>`
- **Description**: Prints the specified text
- **Example**:
  ```
  > echo Hello, RusticOS!
  Hello, RusticOS!
  ```

#### `makefile`
- **Usage**: `makefile <filename>`
- **Description**: Creates an empty file with the specified name
- **Example**:
  ```
  > makefile myfile.txt
  File created: myfile.txt
  ```

#### `cat`
- **Usage**: `cat <filename>`
- **Description**: Displays the contents of a file
- **Example**:
  ```
  > cat myfile.txt
  Hello, World!
  ```

#### `write`
- **Usage**: `write <filename> <content>`
- **Description**: Writes content to an existing file
- **Note**: The file must already exist (created with `makefile` first)
- **Example**:
  ```
  > makefile myfile.txt
  > write myfile.txt Hello, World!
  File written: myfile.txt
  > cat myfile.txt
  Hello, World!
  ```

#### `remove`
- **Usage**: `remove <name>`
- **Description**: Removes a file or empty directory
- **Note**: Cannot remove directories that contain files or subdirectories
- **Example**:
  ```
  > remove oldfile.txt
  Removed: oldfile.txt
  > remove emptydir
  Removed: emptydir
  ```

#### `move`
- **Usage**: `move <source> <destination>`
- **Description**: Moves or renames a file or directory within the current directory
- **Example**:
  ```
  > move oldname.txt newname.txt
  Moved: oldname.txt -> newname.txt
  ```

#### `copy`
- **Usage**: `copy <source> <destination>`
- **Description**: Copies a file to a new name in the current directory
- **Note**: Currently only supports files (not directories)
- **Example**:
  ```
  > copy original.txt backup.txt
  Copied: original.txt -> backup.txt
  ```

#### `shutdown`
- **Usage**: `shutdown`
- **Description**: Shuts down the system gracefully and exits QEMU
- **Example**:
  ```
  > shutdown
  Shutting down RusticOS...
  System halted.
  [QEMU window closes]
  ```

## Technical Implementation

### Architecture
- **Kernel**: Main kernel loop with interrupt-driven event handling
- **Interrupts**: Full interrupt handling infrastructure (IDT, PIC, ISRs, IRQs)
- **Terminal**: VGA text-mode display (80x25) with cursor management and title bar
- **Keyboard**: PS/2 keyboard input via IRQ1 interrupts
- **Filesystem**: In-memory hierarchical filesystem with dynamic allocation
- **Command System**: Command parser and executor with argument support

### Memory Management
- **Dynamic allocation**: Filesystem nodes are allocated with `new`/`delete`
- **File data**: File contents are dynamically allocated and can be resized
- **Heap allocator**: Simple bump allocator (64 KB pool) - no free/reuse yet
- **Limitations**: Maximum of 64 entries per directory, 32 character name limit, 256 character path limit

### Boot Sequence
- **Bootloader**: First-stage bootloader (512 bytes) loads loader from sector 2
- **Loader**: Second-stage loader switches to protected mode and loads kernel
- **Kernel**: Initializes hardware, sets up interrupts, and starts command loop
- **Delays**: Visible boot messages with 1-1.5 second delays for readability

### File Structure
```
src/
├── kernel.cpp      # Main kernel with interrupt-driven event loop
├── crt0.s          # Kernel startup code and interrupt stubs (assembly)
├── interrupt.h/cpp # Interrupt handling (IDT, PIC, ISRs, IRQs)
├── terminal.h/cpp  # VGA terminal interface
├── keyboard.h/cpp  # Keyboard input handling (interrupt-driven)
├── filesystem.h/cpp # Filesystem implementation
├── command.h/cpp   # Command parsing and execution
├── types.h         # Type definitions and standard library stubs
└── cxxabi.cpp      # C++ runtime and memory allocation
```

## Usage Examples

### Basic Navigation
```
> help
> makedir projects
> cd projects
> pwd
/projects
> makedir myproject
> cd myproject
> pwd
/projects/myproject
> cd ../..
> pwd
/
```

### File Operations
```
> makefile example.txt
File created: example.txt
> write example.txt Hello, World!
File written: example.txt
> cat example.txt
Hello, World!
> copy example.txt backup.txt
Copied: example.txt -> backup.txt
> move example.txt renamed.txt
Moved: example.txt -> renamed.txt
> lsd
backup.txt
renamed.txt
> remove backup.txt
Removed: backup.txt
> echo "Testing echo command"
Testing echo command
> clear
> help
> shutdown
```

## Building and Running

### Build
```bash
make clean
make all
```

### Run
```bash
# Headless mode (no display)
make run-headless

# With VNC display
make run

# With curses display
make run-curses
```

## Future Enhancements

### Planned Features
- **Persistent storage**: Save filesystem to disk and load on boot
- **Directory copying**: Recursive directory copy operations
- **Command history**: Up/down arrow support for command history
- **Tab completion**: Auto-complete commands and paths
- **File permissions**: Basic read/write/execute permissions
- **More commands**: Additional utilities and system commands (grep, find, etc.)

### Technical Improvements
- **Proper heap allocator**: Replace bump allocator with malloc/free implementation
- **Process management**: Multitasking and process scheduling
- **Memory protection**: Virtual memory and page tables
- **Device drivers**: Additional hardware support (mouse, sound, network)
- **File system on disk**: Implement actual filesystem format (FAT32, ext2, etc.)

## Notes
- The filesystem is currently in-memory only and resets on reboot
- Keyboard input is interrupt-driven (IRQ1) for responsive interaction
- File content storage is fully implemented with dynamic allocation
- Boot messages include visible delays (1-1.5 seconds) for readability
- System shutdown properly exits QEMU using `isa-debug-exit` device
- The system is designed for educational purposes and demonstrates OS concepts including:
  - Bootloader and multi-stage loading
  - Protected mode and segmentation
  - Interrupt handling and PIC programming
  - Device drivers and hardware I/O
  - Command-line interface and filesystem design
