/*
 * ============================================================================
 * RusticOS Filesystem Implementation (filesystem.cpp)
 * ============================================================================
 * 
 * Implements the in-memory hierarchical filesystem for RusticOS.
 * Provides file and directory operations including creation, deletion,
 * navigation, reading, and writing.
 * 
 * The filesystem uses dynamic memory allocation for nodes and file data.
 * All operations work on the in-memory tree structure.
 * 
 * Version: 1.6.9
 * ============================================================================
 */

#include "filesystem.h"
#include "terminal.h"

extern Terminal terminal;

/**
 * FileSystem Constructor
 * 
 * Initializes the filesystem by creating the root directory ("/").
 * The root directory has no name and no parent (it is its own parent conceptually).
 * Sets the current working directory to root.
 */
FileSystem::FileSystem() : root(nullptr), current_dir(nullptr) {
    // Create root directory node
    root = new FileNode();
    root->name[0] = '\0';              // Root has empty name
    root->type = FILE_TYPE_DIRECTORY;
    root->is_directory = true;
    root->child_count = 0;             // Root starts empty
    root->size = 0;
    root->data = nullptr;              // Directories don't have data
    root->parent = nullptr;            // Root has no parent
    current_dir = root;                // Start in root directory
}

/**
 * FileSystem Destructor
 * 
 * Cleans up all filesystem nodes by recursively freeing the entire tree.
 * This deallocates all memory used by the filesystem.
 */
FileSystem::~FileSystem() {
    if (root) {
        free_node(root);  // Recursively free entire tree starting from root
    }
}

/**
 * Free Node (Recursive Helper)
 * 
 * Recursively frees a node and all its children. Used for cleanup.
 * 
 * @param node Pointer to node to free (can be null for safety)
 */
void FileSystem::free_node(FileNode* node) {
    if (!node) return;  // Safety check: null pointer
    
    // Recursively free all children first
    for (uint32_t i = 0; i < node->child_count; ++i) {
        free_node(node->children[i]);
    }
    
    // Free file data if this is a file
    if (node->data) {
        delete[] node->data;
    }
    
    // Free the node itself
    delete node;
}

/**
 * Find Child Node
 * 
 * Searches for a child node with the given name in a parent directory.
 * 
 * @param parent Parent directory to search in
 * @param name Name of child to find (null-terminated string)
 * @return Pointer to child node if found, nullptr otherwise
 */
FileNode* FileSystem::find_child(FileNode* parent, const char* name) {
    if (!parent || !name) return nullptr;  // Safety check
    
    // Linear search through children array
    for (uint32_t i = 0; i < parent->child_count; ++i) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];  // Found!
        }
    }
    return nullptr;  // Not found
}

/**
 * Create Directory
 * 
 * Creates a new directory in the current working directory.
 * 
 * @param name Name of the directory to create (must be unique in current directory)
 * @return true if directory created successfully, false on error
 * 
 * Error conditions:
 *   - Invalid name (null pointer or empty)
 *   - Directory full (MAX_DIRECTORY_ENTRIES reached)
 *   - Name already exists
 */
bool FileSystem::mkdir(const char* name) {
    // Validate inputs
    if (!name || !current_dir) {
        return false;  // Invalid parameters
    }
    
    // Check if directory is full
    if (current_dir->child_count >= MAX_DIRECTORY_ENTRIES) {
        return false;  // Directory full
    }
    
    // Check if name already exists
    if (find_child(current_dir, name)) {
        return false;  // Name already exists
    }
    
    // Create new directory node
    FileNode* new_dir = new FileNode();
    strncpy(new_dir->name, name, MAX_NAME_LENGTH - 1);  // Copy name with safety
    new_dir->name[MAX_NAME_LENGTH - 1] = '\0';          // Ensure null termination
    new_dir->type = FILE_TYPE_DIRECTORY;
    new_dir->is_directory = true;
    new_dir->child_count = 0;           // New directory starts empty
    new_dir->size = 0;
    new_dir->data = nullptr;            // Directories don't have data
    new_dir->parent = current_dir;      // Set parent pointer
    
    // Add to parent's children array
    current_dir->children[current_dir->child_count++] = new_dir;
    return true;  // Success!
}

bool FileSystem::rmdir(const char* name) {
    if (!name || !current_dir) return false;
    
    FileNode* dir = find_child(current_dir, name);
    if (!dir || dir->type != FILE_TYPE_DIRECTORY || dir->child_count > 0) {
        return false;
    }
    
    // Find and remove from parent
    for (uint32_t i = 0; i < current_dir->child_count; ++i) {
        if (current_dir->children[i] == dir) {
            for (uint32_t j = i; j < current_dir->child_count - 1; ++j) {
                current_dir->children[j] = current_dir->children[j + 1];
            }
            current_dir->child_count--;
            break;
        }
    }
    
    free_node(dir);
    return true;
}

bool FileSystem::cd(const char* path) {
    if (!path || !current_dir) return false;
    
    if (strcmp(path, "/") == 0) {
        current_dir = root;
        return true;
    } else if (strcmp(path, "..") == 0) {
        if (current_dir->parent) {
            current_dir = current_dir->parent;
        }
        return true;
    }
    
    FileNode* target = find_child(current_dir, path);
    if (target && target->type == FILE_TYPE_DIRECTORY) {
        current_dir = target;
        return true;
    }
    return false;
}

void FileSystem::ls() {
    if (!current_dir) {
        terminal.write("Error: no current directory\n");
        return;
    }
    
    for (uint32_t i = 0; i < current_dir->child_count; i++) {
        FileNode* child = current_dir->children[i];
        terminal.write(child->name);
        if (child->type == FILE_TYPE_DIRECTORY) {
            terminal.write("/");
        }
        terminal.write("\n");
    }
}

bool FileSystem::pwd() {
    if (!current_dir || !root) return false;
    
    // If we're at root, just return "/"
    if (current_dir == root) {
        terminal.write("/\n");
        return true;
    }
    
    // Build path by traversing from current_dir up to root
    // Store directory names in reverse order, then reverse them
    char dir_names[32][MAX_NAME_LENGTH];
    uint32_t dir_count = 0;
    
    FileNode* node = current_dir;
    
    // Collect directory names from current to root (excluding root)
    while (node && node != root && dir_count < 32) {
        uint32_t name_len = strlen(node->name);
        if (name_len > 0 && name_len < MAX_NAME_LENGTH) {
            // Copy directory name (including null terminator)
            strncpy(dir_names[dir_count], node->name, MAX_NAME_LENGTH - 1);
            dir_names[dir_count][MAX_NAME_LENGTH - 1] = '\0';
            dir_count++;
        }
        node = node->parent;
    }
    
    // Build the path string by reversing the directory names
    char path[MAX_PATH_LENGTH] = {0};
    uint32_t path_pos = 0;
    
    path[path_pos++] = '/';  // Always start with "/"
    
    // Add directories in reverse order (from root to current)
    for (int i = dir_count - 1; i >= 0; i--) {
        uint32_t name_len = strlen(dir_names[i]);
        
        // Check if we have space
        if (path_pos + name_len + 1 >= MAX_PATH_LENGTH) {
            break;
        }
        
        // Add directory name
        for (uint32_t j = 0; j < name_len; j++) {
            path[path_pos++] = dir_names[i][j];
        }
        
        // Add trailing slash if not last directory
        if (i > 0) {
            path[path_pos++] = '/';
        }
    }
    
    path[path_pos] = '\0';
    
    terminal.write(path);
    terminal.write("\n");
    return true;
}

bool FileSystem::create_file(const char* name, const char* content) {
    if (!name || !current_dir || current_dir->child_count >= MAX_DIRECTORY_ENTRIES) {
        return false;
    }
    
    if (find_child(current_dir, name)) {
        return false;
    }
    
    FileNode* new_file = new FileNode();
    strncpy(new_file->name, name, MAX_NAME_LENGTH - 1);
    new_file->name[MAX_NAME_LENGTH - 1] = '\0';
    new_file->type = FILE_TYPE_FILE;
    new_file->is_directory = false;
    new_file->child_count = 0;
    new_file->parent = current_dir;
    
    uint32_t content_len = content ? strlen(content) : 0;
    new_file->data_capacity = (content_len > 0) ? content_len + 1 : 64;
    new_file->data = new char[new_file->data_capacity];
    new_file->size = content_len;
    
    if (content && content_len > 0) {
        strncpy(new_file->data, content, new_file->data_capacity - 1);
        new_file->data[new_file->data_capacity - 1] = '\0';
    } else {
        new_file->data[0] = '\0';
    }
    
    current_dir->children[current_dir->child_count++] = new_file;
    return true;
}

bool FileSystem::delete_file(const char* name) {
    if (!name || !current_dir) return false;
    
    FileNode* file = find_child(current_dir, name);
    if (!file || file->type != FILE_TYPE_FILE) {
        return false;
    }
    
    for (uint32_t i = 0; i < current_dir->child_count; ++i) {
        if (current_dir->children[i] == file) {
            for (uint32_t j = i; j < current_dir->child_count - 1; ++j) {
                current_dir->children[j] = current_dir->children[j + 1];
            }
            current_dir->child_count--;
            break;
        }
    }
    
    free_node(file);
    return true;
}

bool FileSystem::read_file(const char* name, char* buffer, uint32_t max_size) {
    if (!name || !buffer || !current_dir) return false;
    
    FileNode* file = find_child(current_dir, name);
    if (!file || file->type != FILE_TYPE_FILE || !file->data) {
        return false;
    }
    
    uint32_t copy_len = (file->size < max_size) ? file->size : max_size - 1;
    strncpy(buffer, file->data, copy_len);
    buffer[copy_len] = '\0';
    return true;
}

bool FileSystem::write_file(const char* name, const char* content) {
    if (!name || !content || !current_dir) return false;
    
    FileNode* file = find_child(current_dir, name);
    if (!file || file->type != FILE_TYPE_FILE) {
        return false;
    }
    
    uint32_t content_len = strlen(content);
    if (content_len > file->data_capacity) {
        delete[] file->data;
        file->data_capacity = content_len + 1;
        file->data = new char[file->data_capacity];
    }
    
    strncpy(file->data, content, file->data_capacity - 1);
    file->data[file->data_capacity - 1] = '\0';
    file->size = content_len;
    return true;
}

bool FileSystem::remove(const char* name) {
    if (!name || !current_dir) return false;
    
    FileNode* node = find_child(current_dir, name);
    if (!node) return false;
    
    // If it's a directory, check if it's empty
    if (node->type == FILE_TYPE_DIRECTORY) {
        if (node->child_count > 0) {
            terminal.write("Error: directory not empty\n");
            return false;
        }
        return rmdir(name);
    } else {
        // It's a file
        return delete_file(name);
    }
}

bool FileSystem::move(const char* src, const char* dest) {
    if (!src || !dest || !current_dir) return false;
    
    // Check if source exists
    FileNode* src_node = find_child(current_dir, src);
    if (!src_node) {
        terminal.write("Error: source not found\n");
        return false;
    }
    
    // Check if destination already exists
    if (find_child(current_dir, dest)) {
        terminal.write("Error: destination already exists\n");
        return false;
    }
    
    // Check if we have space
    if (current_dir->child_count >= MAX_DIRECTORY_ENTRIES) {
        terminal.write("Error: directory full\n");
        return false;
    }
    
    // Simply rename: update the name field
    strncpy(src_node->name, dest, MAX_NAME_LENGTH - 1);
    src_node->name[MAX_NAME_LENGTH - 1] = '\0';
    
    return true;
}

bool FileSystem::copy_file(const char* src, const char* dest) {
    if (!src || !dest || !current_dir) return false;
    
    // Check if source exists and is a file
    FileNode* src_file = find_child(current_dir, src);
    if (!src_file || src_file->type != FILE_TYPE_FILE) {
        terminal.write("Error: source file not found\n");
        return false;
    }
    
    // Check if destination already exists
    if (find_child(current_dir, dest)) {
        terminal.write("Error: destination already exists\n");
        return false;
    }
    
    // Check if we have space
    if (current_dir->child_count >= MAX_DIRECTORY_ENTRIES) {
        terminal.write("Error: directory full\n");
        return false;
    }
    
    // Create new file with the same content
    const char* content = src_file->data ? src_file->data : "";
    if (create_file(dest, content)) {
        // If source file has data, copy it
        if (src_file->data && src_file->size > 0) {
            FileNode* dest_file = find_child(current_dir, dest);
            if (dest_file) {
                // Allocate and copy data
                dest_file->data_capacity = src_file->size + 1;
                dest_file->data = new char[dest_file->data_capacity];
                strncpy(dest_file->data, src_file->data, src_file->size);
                dest_file->data[src_file->size] = '\0';
                dest_file->size = src_file->size;
            }
        }
        return true;
    }
    
    return false;
}

void FileSystem::save_to_disk() {
    // Stub: save filesystem to disk
}

bool FileSystem::load_from_disk() {
    // Stub: load filesystem from disk
    return true;
}

void FileSystem::print_tree(FileNode* node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; ++i) terminal.write("  ");
    terminal.write(node->name);
    if (node->type == FILE_TYPE_DIRECTORY) terminal.write("/");
    terminal.write("\n");
    for (uint32_t i = 0; i < node->child_count; ++i) {
        print_tree(node->children[i], depth + 1);
    }
}

FileSystem filesystem;
