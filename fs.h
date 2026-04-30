#ifndef FS_H
#define FS_H

/*
 * fs.h — portable filesystem abstraction
 *
 * Wraps platform-specific directory traversal behind a unified API.
 * On POSIX (Linux/macOS): uses opendir/readdir/stat.
 * On Windows:             uses FindFirstFile/FindNextFile/GetFileAttributes.
 *
 * Consumers never touch platform APIs directly.
 */

#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Entry type                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    FS_ENTRY_FILE    = 0,
    FS_ENTRY_DIR     = 1,
    FS_ENTRY_SYMLINK = 2,
    FS_ENTRY_OTHER   = 3
} fs_entry_type_t;

/*
 * fs_entry_t — a single directory entry returned by the iterator.
 * `name` is a pointer into internal storage; do not free it.
 */
typedef struct {
    const char     *name;        /* entry name (no path prefix)     */
    fs_entry_type_t type;        /* file / dir / symlink / other    */
} fs_entry_t;

/* ------------------------------------------------------------------ */
/*  Directory iterator (opaque handle)                                 */
/* ------------------------------------------------------------------ */

typedef struct fs_dir fs_dir_t;

/*
 * fs_opendir  — open a directory for iteration.
 *               Returns NULL on failure (errno set on POSIX).
 */
fs_dir_t *fs_opendir(const char *path);

/*
 * fs_readdir  — fetch the next entry. Skips "." and "..".
 *               Returns 1 and fills *out on success; 0 at end-of-dir.
 *               `out->name` is valid until the next call or fs_closedir.
 */
int fs_readdir(fs_dir_t *dir, fs_entry_t *out);

/*
 * fs_closedir — release all resources held by the iterator.
 */
void fs_closedir(fs_dir_t *dir);

/* ------------------------------------------------------------------ */
/*  Misc helpers                                                        */
/* ------------------------------------------------------------------ */

/*
 * fs_is_dir — returns 1 if `path` is a directory (follows symlinks).
 */
int fs_is_dir(const char *path);

/*
 * fs_join — write path+"/"+name into buf (size bytes).
 *           Always NUL-terminates; truncates silently if too long.
 */
void fs_join(char *buf, size_t size, const char *path, const char *name);

#endif /* FS_H */
