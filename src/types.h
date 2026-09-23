/*
 * ============================================================================
 * RusticOS Type Definitions (types.h)
 * ============================================================================
 * 
 * This header provides basic type definitions for the freestanding environment.
 * Since we don't have a standard library, we define our own fixed-width
 * integer types and minimal C library function declarations.
 * 
 * These types are used throughout the kernel for consistent data sizes
 * across different platforms and architectures.
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#ifndef TYPES_H
#define TYPES_H

// ============================================================================
// Fixed-Width Integer Types
// ============================================================================
// Standard fixed-width integer types for 32-bit architecture
// These ensure consistent sizes regardless of compiler settings

// Unsigned integer types
typedef unsigned char       uint8_t;   // 8-bit unsigned integer (0 to 255)
typedef unsigned short      uint16_t;  // 16-bit unsigned integer (0 to 65535)
typedef unsigned int        uint32_t;  // 32-bit unsigned integer (0 to 4,294,967,295)
typedef unsigned long long  uint64_t;  // 64-bit unsigned integer

// Signed integer types
typedef signed char         int8_t;    // 8-bit signed integer (-128 to 127)
typedef signed short        int16_t;   // 16-bit signed integer (-32768 to 32767)
typedef signed int          int32_t;   // 32-bit signed integer
typedef signed long long    int64_t;   // 64-bit signed integer

// ============================================================================
// Standard Types
// ============================================================================
typedef unsigned int        size_t;    // 32-bit size type used by the kernel ABI

// Note: bool, true, and false are built-in C++ types/constants
// They are available in C++ without needing to define them here

// ============================================================================
// Minimal C Library Function Declarations
// ============================================================================
// These functions are implemented in cxxabi.cpp
// They provide basic memory and string operations needed by C++ code
// ============================================================================
extern "C" {
    // Memory operations
    void* memcpy(void* dst, const void* src, size_t n);  // Copy memory block
    void* memset(void* p, int c, size_t n);              // Fill memory block with value
    
    // String operations
    int strcmp(const char* a, const char* b);            // Compare two strings
    char* strncpy(char* dst, const char* src, size_t n); // Copy string (with length limit)
    size_t strlen(const char* s);                        // Get string length
}

#endif // TYPES_H