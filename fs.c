/*
 * fs.c — portable filesystem abstraction implementation
 *
 * POSIX path:   opendir / readdir / stat
 * Windows path: FindFirstFile / FindNextFile / GetFileAttributes
 *
 * The two implementations are selected at compile time via _WIN32.
 * Both expose exactly the same fs_dir_t / fs_entry_t interface.
 */

/*
 * _DEFAULT_SOURCE enables POSIX.1-2008 extensions on glibc (Linux),
 * which exposes lstat(), DT_* constants, and d_type in struct dirent.
 * Must be defined before any system header is included.
 */
#ifndef _WIN32
#  define _DEFAULT_SOURCE
#endif

#include "fs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==================================================================
 * POSIX implementation (Linux + macOS)
 * ================================================================== */
#ifndef _WIN32

#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

struct fs_dir {
    DIR  *dp;
    char  path[4096];   /* kept so we can stat entries */
};

fs_dir_t *fs_opendir(const char *path) {
    DIR *dp = opendir(path);
    if (!dp) return NULL;

    fs_dir_t *d = malloc(sizeof(fs_dir_t));
    if (!d) { closedir(dp); return NULL; }

    d->dp = dp;
    strncpy(d->path, path, sizeof(d->path) - 1);
    d->path[sizeof(d->path) - 1] = '\0';
    return d;
}

int fs_readdir(fs_dir_t *dir, fs_entry_t *out) {
    struct dirent *de;

    while ((de = readdir(dir->dp)) != NULL) {
        /* skip self and parent */
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        out->name = de->d_name;

        /*
         * Prefer d_type when available (avoids a stat call on most
         * Linux filesystems). DT_* constants are POSIX extensions;
         * guard with #ifdef so the code compiles even on platforms
         * that lack them (rare, but possible).
         * Fall through to lstat when d_type == DT_UNKNOWN (some NFS /
         * tmpfs mounts return this) or when DT_* is unavailable.
         */
#if defined(DT_DIR) && defined(DT_LNK) && defined(DT_REG) && defined(DT_UNKNOWN)
        if (de->d_type == DT_DIR) {
            out->type = FS_ENTRY_DIR;
            return 1;
        }
        if (de->d_type == DT_LNK) {
            out->type = FS_ENTRY_SYMLINK;
            return 1;
        }
        if (de->d_type == DT_REG) {
            out->type = FS_ENTRY_FILE;
            return 1;
        }
        if (de->d_type != DT_UNKNOWN) {
            out->type = FS_ENTRY_OTHER;
            return 1;
        }
#endif
        /* Fallback: use lstat (does NOT follow symlinks) */
        {
            char full[4096];
            struct stat st;
            fs_join(full, sizeof(full), dir->path, de->d_name);

            if (lstat(full, &st) != 0) {
                /* unreadable entry — report as OTHER, keep iterating */
                out->type = FS_ENTRY_OTHER;
                return 1;
            }

            if (S_ISDIR(st.st_mode))       out->type = FS_ENTRY_DIR;
            else if (S_ISLNK(st.st_mode))  out->type = FS_ENTRY_SYMLINK;
            else if (S_ISREG(st.st_mode))  out->type = FS_ENTRY_FILE;
            else                           out->type = FS_ENTRY_OTHER;
        }

        return 1;
    }

    return 0; /* end of directory */
}

void fs_closedir(fs_dir_t *dir) {
    if (dir) {
        closedir(dir->dp);
        free(dir);
    }
}

int fs_is_dir(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* ==================================================================
 * Windows implementation
 * ================================================================== */
#else /* _WIN32 */

#include <windows.h>

struct fs_dir {
    HANDLE           handle;
    WIN32_FIND_DATAA data;
    int              has_first; /* 1 = data holds the first entry  */
    int              done;      /* 1 = iteration exhausted          */
};

fs_dir_t *fs_opendir(const char *path) {
    char pattern[4096];
    /* FindFirstFile needs "path\*" */
    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    fs_dir_t *d = malloc(sizeof(fs_dir_t));
    if (!d) return NULL;

    d->handle = FindFirstFileA(pattern, &d->data);
    if (d->handle == INVALID_HANDLE_VALUE) {
        free(d);
        return NULL;
    }

    d->has_first = 1;
    d->done      = 0;
    return d;
}

int fs_readdir(fs_dir_t *dir, fs_entry_t *out) {
    if (dir->done) return 0;

    for (;;) {
        WIN32_FIND_DATAA *wd;

        if (dir->has_first) {
            wd = &dir->data;
            dir->has_first = 0;
        } else {
            if (!FindNextFileA(dir->handle, &dir->data)) {
                dir->done = 1;
                return 0;
            }
            wd = &dir->data;
        }

        /* skip . and .. */
        if (strcmp(wd->cFileName, ".") == 0 || strcmp(wd->cFileName, "..") == 0)
            continue;

        out->name = wd->cFileName;

        if (wd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            out->type = FS_ENTRY_SYMLINK;
        else if (wd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            out->type = FS_ENTRY_DIR;
        else
            out->type = FS_ENTRY_FILE;

        return 1;
    }
}

void fs_closedir(fs_dir_t *dir) {
    if (dir) {
        if (dir->handle != INVALID_HANDLE_VALUE)
            FindClose(dir->handle);
        free(dir);
    }
}

int fs_is_dir(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES &&
            (attr & FILE_ATTRIBUTE_DIRECTORY));
}

#endif /* _WIN32 */

/* ==================================================================
 * Shared helpers (platform-independent)
 * ================================================================== */

void fs_join(char *buf, size_t size, const char *path, const char *name) {
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif

    size_t plen = strlen(path);
    size_t nlen = strlen(name);

    /* strip trailing separator from path if present */
    while (plen > 1 && (path[plen-1] == '/' || path[plen-1] == '\\'))
        plen--;

    /* write: path + sep + name + NUL */
    if (plen + 1 + nlen + 1 > size) {
        /* truncate gracefully */
        strncpy(buf, path, size - 1);
        buf[size - 1] = '\0';
        return;
    }

    memcpy(buf, path, plen);
    buf[plen] = sep;
    memcpy(buf + plen + 1, name, nlen);
    buf[plen + 1 + nlen] = '\0';
}
