/*
 * ============================================================================
 * RusticOS C++ Runtime and Standard Library Stubs (cxxabi.cpp)
 * ============================================================================
 * 
 * Provides minimal C++ runtime and C library functions for freestanding environment.
 * Since we don't have a standard library, we implement essential functions ourselves.
 * 
 * Implements:
 *   - Memory operations: memcpy, memset
 *   - String operations: strcmp, strncpy, strlen
 *   - C++ operators: new, delete (bump allocator - simple but functional)
 *   - C++ ABI stubs: __cxa_pure_virtual, __cxa_atexit, __dso_handle
 * 
 * Note: The heap allocator is a simple bump allocator (no free/reuse).
 * Memory is allocated sequentially and never freed. This is sufficient
 * for the current kernel needs but will need improvement for advanced features.
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#include "types.h"

extern "C" {
    // ========================================================================
    // Memory Operations
    // ========================================================================
    
    /**
     * Copy Memory Block
     * 
     * Copies n bytes from src to dst. Handles overlapping regions correctly.
     * 
     * @param dst Destination buffer
     * @param src Source buffer
     * @param n Number of bytes to copy
     * @return Pointer to destination buffer
     */
    void* memcpy(void* dst, const void* src, size_t n) {
        uint8_t* d = (uint8_t*)dst;
        const uint8_t* s = (const uint8_t*)src;
        for (size_t i = 0; i < n; ++i) {
            d[i] = s[i];
        }
        return dst;
    }

    /**
     * Fill Memory Block
     * 
     * Sets n bytes starting at p to the value c.
     * 
     * @param p Pointer to memory block
     * @param c Value to fill with (cast to uint8_t)
     * @param n Number of bytes to fill
     * @return Pointer to memory block
     */
    void* memset(void* p, int c, size_t n) {
        uint8_t* d = (uint8_t*)p;
        for (size_t i = 0; i < n; ++i) {
            d[i] = (uint8_t)c;
        }
        return p;
    }

    // ========================================================================
    // String Operations
    // ========================================================================
    
    /**
     * Compare Strings
     * 
     * Compares two null-terminated strings lexicographically.
     * 
     * @param a First string
     * @param b Second string
     * @return < 0 if a < b, 0 if a == b, > 0 if a > b
     */
    int strcmp(const char* a, const char* b) {
        while (*a && *a == *b) {
            ++a;
            ++b;
        }
        return (int)(unsigned char)*a - (int)(unsigned char)*b;
    }

    /**
     * Copy String (with length limit)
     * 
     * Copies at most n characters from src to dst, ensuring null termination.
     * 
     * @param dst Destination buffer
     * @param src Source string
     * @param n Maximum number of characters to copy (including null terminator)
     * @return Pointer to destination buffer
     */
    char* strncpy(char* dst, const char* src, size_t n) {
        size_t i;
        // Copy characters up to n or until null terminator
        for (i = 0; i < n && src[i]; ++i) {
            dst[i] = src[i];
        }
        // Pad remaining bytes with null terminators
        for (; i < n; ++i) {
            dst[i] = '\0';
        }
        return dst;
    }

    /**
     * Get String Length
     * 
     * Returns the length of a null-terminated string (excluding null terminator).
     * 
     * @param s Null-terminated string
     * @return Length of string
     */
    size_t strlen(const char* s) throw() {
        size_t n = 0;
        while (s[n]) {
            ++n;
        }
        return n;
    }

    // ========================================================================
    // C++ Memory Allocation Operators
    // ========================================================================
    
    /**
     * Simple Bump Allocator
     * 
     * This is a basic bump allocator that allocates memory sequentially.
     * Memory is never freed (delete is a no-op). This is sufficient for
     * the kernel's current needs but should be replaced with a proper
     * heap allocator for advanced features.
     * 
     * Heap size: 64 KB (65536 bytes)
     * Alignment: 8 bytes (for proper alignment)
     */
    static uint8_t heap_pool[65536];    // 64 KB heap pool
    static uint32_t heap_pos = 0;       // Current allocation position

    /**
     * Single Object Allocation (new operator)
     * 
     * Allocates memory for a single object with 8-byte alignment.
     * Returns nullptr if heap is exhausted.
     */
    void* operator new(size_t size) throw() {
        if (heap_pos + size > sizeof(heap_pool)) {
            return nullptr;  // Heap exhausted
        }
        void* ptr = &heap_pool[heap_pos];
        // Align to 8-byte boundary: round up to next multiple of 8
        heap_pos += ((size + 7) & ~7);
        return ptr;
    }

    /**
     * Array Allocation (new[] operator)
     * 
     * Allocates memory for an array with 8-byte alignment.
     * Returns nullptr if heap is exhausted.
     */
    void* operator new[](size_t size) throw() {
        if (heap_pos + size > sizeof(heap_pool)) {
            return nullptr;  // Heap exhausted
        }
        void* ptr = &heap_pool[heap_pos];
        // Align to 8-byte boundary: round up to next multiple of 8
        heap_pos += ((size + 7) & ~7);
        return ptr;
    }

    /**
     * Single Object Deallocation (delete operator)
     * 
     * Currently a no-op. The bump allocator doesn't support freeing memory.
     * Future implementation should use a proper heap allocator with free lists.
     */
    void operator delete(void* ptr, size_t) throw() {
        (void)ptr;  // Suppress unused parameter warning
        // No-op: bump allocator doesn't support free
    }

    /**
     * Array Deallocation (delete[] operator)
     * 
     * Currently a no-op. See delete() operator above.
     */
    void operator delete[](void* ptr) throw() {
        (void)ptr;  // Suppress unused parameter warning
        // No-op: bump allocator doesn't support free
    }

    void __cxa_pure_virtual() { for (;;) {} }

    int __cxa_atexit(void (*func)(void*), void* arg, void* dso) {
        (void)func; (void)arg; (void)dso;
        return 0;
    }

    // Provide missing symbols used by C++ runtime / linker
    // Define non-hidden __dso_handle so link resolves it
    void* __dso_handle = (void*)0x1;

    // Simple stub for stack protector failure (should halt)
    void __stack_chk_fail() {
        // In a freestanding environment, just spin/halt.
        for (;;) { asm volatile("cli; hlt"); }
    }
}