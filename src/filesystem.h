/*
 * ============================================================================
 * RusticOS Filesystem Header (filesystem.h)
 * ============================================================================
 * 
 * Defines the filesystem data structures and interface for RusticOS.
 * Provides an in-memory hierarchical filesystem with directory and file operations.
 * 
 * Features:
 *   - Hierarchical directory structure (parent-child relationships)
 *   - File and directory creation, deletion, and navigation
 *   - Dynamic memory allocation for filesystem nodes
 *   - File content storage with dynamic sizing
 * 
 * Limitations:
 *   - Maximum 64 entries per directory
 *   - Maximum 32 characters per name
 *   - Maximum 256 characters per path
 *   - In-memory only (not persistent across reboots)
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "types.h"

// ============================================================================
// Filesystem Constants
// ============================================================================
#define MAX_NAME_LENGTH         32      // Maximum length for file/directory names
#define MAX_PATH_LENGTH         256     // Maximum length for full paths
#define MAX_DIRECTORY_ENTRIES   64      // Maximum children per directory

// File type constants
#define FILE_TYPE_FILE          1       // Regular file
#define FILE_TYPE_DIRECTORY     0       // Directory

// ============================================================================
// Filesystem Data Structures
// ============================================================================

/**
 * FileNode - Represents a file or directory in the filesystem
 * 
 * This structure is used for both files and directories. For directories,
 * the children array contains pointers to child nodes. For files, the
 * data pointer contains file content.
 */
struct FileNode {
    char name[MAX_NAME_LENGTH];                    // File or directory name (null-terminated)
    uint8_t type;                                  // FILE_TYPE_FILE or FILE_TYPE_DIRECTORY
    bool is_directory;                             // True if this is a directory
    uint32_t child_count;                          // Number of children (0 for files)
    uint32_t size;                                 // File size in bytes (for files), 0 for directories
    char* data;                                    // File content pointer (null for directories, allocated for files)
    uint32_t data_capacity;                        // Allocated capacity for data buffer (for dynamic resizing)
    FileNode* children[MAX_DIRECTORY_ENTRIES];     // Array of child node pointers (for directories)
    FileNode* parent;                              // Pointer to parent directory (null for root)
};

/**
 * DiskEntry - Structure for potential disk-based storage (future use)
 * 
 * This structure is defined for future implementation of persistent storage.
 * Currently not used, but reserved for filesystem serialization to disk.
 */
struct DiskEntry {
    char name[MAX_NAME_LENGTH];
    uint8_t type;
    uint32_t size;
    uint32_t child_count;
    char data[1024];                               // Fixed-size data buffer for disk storage
};

/**
 * ============================================================================
 * FileSystem Class
 * ============================================================================
 * 
 * Manages the in-memory filesystem hierarchy. Provides operations for
 * creating, deleting, reading, writing, and navigating files and directories.
 * 
 * The filesystem is tree-structured with a root directory ("/") and
 * supports parent-child relationships for navigation.
 * ============================================================================
 */
class FileSystem {
private:
    FileNode* root;                 // Root directory node (always exists)
    FileNode* current_dir;          // Current working directory pointer
    
    // Private helper functions
    FileNode* find_child(FileNode* parent, const char* name);  // Find child by name
    void free_node(FileNode* node);                            // Recursively free node and children
    void print_tree(FileNode* node, int depth);                // Debug: print directory tree
    
public:
    // Constructor and destructor
    FileSystem();       // Initialize filesystem with root directory
    ~FileSystem();      // Clean up all filesystem nodes
    
    // Directory operations
    bool mkdir(const char* path);   // Create a new directory
    bool rmdir(const char* path);   // Remove an empty directory
    bool cd(const char* path);      // Change current directory (supports "/", "..", and relative paths)
    void ls();                      // List contents of current directory
    bool pwd();                     // Print current working directory path
    
    // File operations
    bool create_file(const char* name, const char* content);  // Create a new file with optional content
    bool delete_file(const char* name);                       // Delete a file
    bool read_file(const char* name, char* buffer, uint32_t max_size);  // Read file contents into buffer
    bool write_file(const char* name, const char* content);   // Write content to existing file
    
    // Advanced file operations
    bool remove(const char* name);           // Remove file or empty directory (unified interface)
    bool move(const char* src, const char* dest);    // Move/rename file or directory
    bool copy_file(const char* src, const char* dest);  // Copy file (directories not yet supported)
    
    // Persistent storage (future implementation)
    void save_to_disk();        // Save filesystem to disk (stub - not implemented)
    bool load_from_disk();      // Load filesystem from disk (stub - not implemented)
    
    // Accessor
    FileNode* get_current_dir() const { return current_dir; }  // Get current directory pointer
};

extern FileSystem filesystem;

#endif // FILESYSTEM_H