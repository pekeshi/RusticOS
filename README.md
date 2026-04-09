# RusticOS - Simple x86 Operating System

**Version: 1.6.9**

A minimal operating system development environment built with NASM and GCC for learning OS development concepts. Features a complete bootloader, 32-bit protected mode kernel, interrupt handling, command-line interface, and in-memory filesystem.

## Features

- **Multi-stage Bootloader**: Two-stage bootloader (16-bit real mode → 32-bit protected mode)
- **32-bit Protected Mode Kernel**: C++ kernel with comprehensive interrupt handling
- **Interrupt System**: Full IDT, PIC remapping, and interrupt-driven I/O
- **Command-Line Interface**: Interactive shell with full command set
- **In-Memory Filesystem**: Hierarchical filesystem with directory and file operations
- **Keyboard Driver**: Interrupt-driven PS/2 keyboard input (IRQ1)
- **VGA Terminal**: 80x25 text mode with color support and title bar
- **Build System**: Makefile for easy development and QEMU integration
- **Graceful Shutdown**: Proper system shutdown with QEMU exit support

## Prerequisites

### Required Software

1. **NASM** (Netwide Assembler) - for compiling assembly code
2. **QEMU** - for emulating the x86_64 system
3. **Make** - for building the project
4. **dd** - for creating disk images (usually pre-installed on Linux)

### Installation

#### Arch Linux
```bash
sudo pacman -S nasm qemu make
```

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install nasm qemu-system-x86 make
```

#### macOS
```bash
brew install nasm qemu make
```

## Project Structure

- `boot/`: Bootloader and loader assembly sources and binaries
  - `bootloader.asm`: First-stage bootloader (512 bytes, loads loader)
  - `loader.asm`: Second-stage loader (switches to protected mode, loads kernel)
- `src/`: Kernel sources (C++ and assembly), startup code, and system components
  - `kernel.cpp`: Main kernel entry point and interrupt-driven event loop
  - `crt0.s`: Kernel startup code and interrupt service routine stubs
  - `interrupt.h/cpp`: Interrupt handling (IDT, PIC, ISRs, IRQs)
  - `terminal.h/cpp`: VGA text-mode display interface (80x25)
  - `keyboard.h/cpp`: PS/2 keyboard driver (interrupt-driven, IRQ1)
  - `command.h/cpp`: Command parsing and execution system
  - `filesystem.h/cpp`: In-memory hierarchical filesystem
  - `types.h`: Type definitions and standard library stubs
  - `cxxabi.cpp`: C++ runtime and memory allocation (bump allocator)
- `build/`: Intermediate object files and build artifacts (generated)
- `scripts/`: Helper scripts for building and running

## Common commands

```bash
# Build everything
make all

# Run with VNC display
make run

# Run in debug mode (no reboot on halt)
make run-debug

# Run test with serial output
make run-test

# Clean build artifacts
make clean

# Clean everything (including source object files)
make distclean
```

## How It Works

### 1. Bootloader (bootloader.asm)
- Loaded by BIOS at memory address `0x7c00`
- Runs in 16-bit real mode
- Loads the second-stage loader from disk

### 2. Loader (loader.asm)
- Loaded by the bootloader
- Switches to 32-bit protected mode
- Loads the kernel from disk into memory
- Jumps to the kernel entry point

### 3. Kernel (kernel.cpp + crt0.s)
- Written in C++ with assembly startup code (crt0.s)
- Runs in 32-bit protected mode with interrupt support
- Initializes IDT (Interrupt Descriptor Table) and PIC (Programmable Interrupt Controller)
- Provides interactive command-line interface
- Handles keyboard input via interrupts (IRQ1)
- Manages an in-memory hierarchical filesystem
- Supports commands: `help`, `clear`, `echo`, `makedir`, `cd`, `lsd`, `pwd`, `makefile`, `cat`, `write`, `remove`, `move`, `copy`, `shutdown`

### 4. Build Process
1. NASM compiles bootloader and loader assembly files to binary format
2. NASM compiles crt0.s (kernel startup) to object file
3. GCC/G++ compiles C++ kernel sources to object files
4. Linker creates the kernel ELF binary, then converts to flat binary
5. Build script calculates kernel size and generates sector count includes
6. `dd` creates a disk image (512 bytes per sector)
7. Bootloader, loader, and kernel are written to the image in sequence:
   - Sector 0: Bootloader (512 bytes)
   - Sectors 1-2: Loader (variable size, padded)
   - Sectors 3+: Kernel (variable size)
8. QEMU boots from the image and runs RusticOS

## Development

### Adding Features

#### Extending the Kernel
- Modify `kernel.cpp` to add new kernel functionality
- Add new C++ source files in `src/`
- Update the Makefile `KERNEL_SOURCES` variable to include new files
- Add new commands in `command.cpp`

#### Adding New Commands
1. Implement command handler in `command.cpp` (e.g., `cmd_newcommand`)
2. Add command dispatch in `CommandSystem::execute_command()`
3. Update `cmd_help()` to include the new command
4. Optionally update `interface_and_filesystem.md` documentation

### Debugging

#### QEMU Debug Mode
```bash
make run-debug
```
This starts QEMU in debug mode where the system won't reboot on halt. For full GDB integration, you can manually start QEMU with:
```bash
qemu-system-x86_64 -drive file=build/disk.img,format=raw,if=ide -boot c -m 512M -serial stdio -s -S
```
Then connect GDB with:
```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

#### Common Issues
- **"Permission denied"**: Make sure `build.sh` is executable (`chmod +x build.sh`)
- **"Command not found"**: Install required packages (NASM, QEMU)
- **Build errors**: Check assembly syntax and Makefile dependencies

## Learning Resources

- [NASM Documentation](https://www.nasm.us/doc/)
- [QEMU Documentation](https://qemu.readthedocs.io/)
- [OSDev Wiki](https://wiki.osdev.org/)
- [x86 Assembly Guide](https://www.cs.virginia.edu/~evans/cs216/guides/x86.html)

## Documentation

For detailed information about the command-line interface and filesystem, see [interface_and_filesystem.md](interface_and_filesystem.md).

## Version History

### Version 1.6.9 (Current)
- Added interrupt handling system (IDT, PIC, ISRs, IRQs)
- Implemented interrupt-driven keyboard input (IRQ1)
- Added `remove`, `move`, and `copy` commands
- Fixed `pwd` command with full path resolution
- Added `shutdown` command with QEMU exit support
- Improved bootloader and loader with visible delays
- Comprehensive code documentation and comments
- Improved source code readability and organization

### Version 1.0.0
- Initial release with basic CLI and filesystem
- Multi-stage bootloader implementation
- Command-line interface with help system
- In-memory hierarchical filesystem

## Next Steps

Future enhancements to consider:
- **Persistent storage**: Save filesystem to disk and load on boot
- **Process management**: Multitasking and process scheduling
- **Memory management**: Proper heap allocator with malloc/free
- **Command history**: Up/down arrow support
- **Tab completion**: Auto-complete commands and paths
- **File system format**: Implement actual disk-based filesystem (FAT32, ext2, etc.)
- **Networking stack**: TCP/IP stack and network drivers
- **Device drivers**: Additional hardware support (mouse, sound, USB)
- **Memory protection**: Virtual memory and page tables

## License

This project is open source and available under the MIT License.

## Contributing

Feel free to submit issues, feature requests, or pull requests to improve this OS development environment. 