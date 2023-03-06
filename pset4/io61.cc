#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// io61.c
//    YOUR CODE HERE!

const size_t BUFSIZE = 4096;

// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.
struct io61_file {
    int fd;                     // The file descriptor

    int mode;                   // The mode (premission) of the file

    char buf[BUFSIZE];          // The cache with size 4096

    size_t cache_curr_pos;      // The current working position in cache
                                // relative to the beginning of cache

    size_t cache_valid_length;  // The valid length of cache
                                // relative to the beginning of cache

    size_t file_tag;            // The physical position in the file
                                // where the beginning of cache is aligned to
};


// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    f->cache_curr_pos = 0;
    f->cache_valid_length = 0;
    f->file_tag = 0;
    return f;
}


// io61_close(f)
//    Close the io61_file `f` and release all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
    unsigned char cbuf[1];
    ssize_t status = io61_read(f, (char*) cbuf, 1);
    if (status == 1) {
        return cbuf[0];
    } else {
        return EOF;
    }
}


// fill_cache(f)
//     Read up to BUFSIZE bytes from an aligned block in 'f' into the cache.
//     Return the number of new bytes actually read, which might be
//     BUFSIZE or a short count.

ssize_t fill_cache(io61_file* f) {

    lseek(f->fd, f->file_tag, SEEK_SET);  // Make fp points to aligned block

    size_t nread = 0;  // Counts number of bytes read
    while (nread < BUFSIZE) {

        // Read from file into cache with needed size
        ssize_t status = read(f->fd, &f->buf[nread], BUFSIZE - nread);

        // If error occurred on read, return -1
        if (status == -1) return -1;

        // If reach EOF, return short count
        if (status == 0) {
            f->cache_valid_length = nread;  // Cache now filled with nread bytes
            lseek(f->fd, f->file_tag + f->cache_curr_pos, SEEK_SET);  // Reset fp
            return nread;
        }
        nread += status;
    }

    lseek(f->fd, f->file_tag + f->cache_curr_pos, SEEK_SET);  // Reset fp

    f->cache_valid_length = BUFSIZE;  // Cache now filled with sz bytes

    return BUFSIZE;
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read.

ssize_t io61_read(io61_file* f, char* buf, size_t sz) {

    // If cache is empty, fill cache
    if (f->cache_valid_length == 0) {
      fill_cache(f);
    }

    size_t cache_remaining_length = f->cache_valid_length - f->cache_curr_pos;

    // If cache's remaining data is enough, read from cache
    if (cache_remaining_length >= sz) {
        memcpy(buf, &f->buf[f->cache_curr_pos], sz);
        f->cache_curr_pos += sz;
        return sz;
    }

    // If cache's remaining data is not enough
    else {

        // Read the remaining data and deplete cache
        memcpy(buf, &f->buf[f->cache_curr_pos], cache_remaining_length);
        buf += cache_remaining_length;

        // Then read more
        size_t needed_to_read = sz - cache_remaining_length;

        // If cache can incorporate what is needed, fill cache and read from it
        if (needed_to_read <= BUFSIZE) {

            f->file_tag += BUFSIZE;  // Update physical position of cache
            int n = fill_cache(f);

            if (n == -1) return -1;

            else {
                // min = min(needed_to_read, n) !!
                ssize_t min = n < (int)needed_to_read ? n : needed_to_read;

                // Move needed data from cache into buf
                memcpy(buf, f->buf, min);

                f->cache_curr_pos = min;

                return cache_remaining_length + min;  // Return total size read
            }
        }

        // If cache cannot incorporate what is needed, read from file directly
        else {
            f->cache_valid_length = 0;
            f->cache_curr_pos = (sz - cache_remaining_length) % BUFSIZE;
            f->file_tag = (f->file_tag + BUFSIZE + sz - cache_remaining_length) / BUFSIZE * BUFSIZE;
            size_t nread = 0;

            while (nread < needed_to_read) {
                ssize_t status = read(f->fd, &buf[nread], needed_to_read - nread);

                // If error occurred on read, return -1
                if (status == -1) return -1;

                // If reach EOF
                if (status == 0) {
                    return cache_remaining_length + nread;
                }
                nread += status;
            }
            return sz;
        }
    }
}

// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    char buf[1];
    buf[0] = ch;
    ssize_t status = io61_write(f, buf, 1);
    if (status == 1) {
        return 0;
    } else {
        return -1;
    }
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {

    size_t cache_available_space = BUFSIZE - f->cache_curr_pos;

    // If cache's available space is enough, write into cache
    if (cache_available_space >= sz) {

        memcpy(&f->buf[f->cache_curr_pos], buf, sz);

        // max(cache_valid_length, cache_curr_pos + sz);
        size_t max = f->cache_valid_length > (f->cache_curr_pos + sz) ? f->cache_valid_length : (f->cache_curr_pos + sz);

        f->cache_valid_length = max;

        f->cache_curr_pos += sz;

        return sz;
    }

    // If cache's available space is not enough
    else {

        io61_flush(f);

        size_t nwritten = 0;  // Counts number of bytes written

        while (nwritten < sz) {

            ssize_t status = write(f->fd, &buf[nwritten], sz - nwritten);

            // If error occurred, return -1
            if (status == -1) return -1;

            nwritten += status;
        }

        f->file_tag = (f->file_tag + BUFSIZE + sz - cache_available_space) / BUFSIZE * BUFSIZE;

        return sz;
    }
}


// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {

    if (f->mode == O_RDONLY) {
        return 0;
    }

    size_t nwritten = 0;

    while (nwritten < f->cache_valid_length) {

        ssize_t status = write(f->fd, &f->buf[nwritten], f->cache_valid_length - nwritten);

        if (status == -1) return -1;

        nwritten += status;
    }

    // Zero the current position and valid length
    f->cache_curr_pos = 0;
    f->cache_valid_length = 0;

    return 0;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {

    // If new position is within the cache, seek to the position
    if ((size_t)pos >= f->file_tag
        && (size_t)pos < (f->file_tag + f->cache_valid_length)) {

        off_t r = lseek(f->fd, pos, SEEK_SET);

        if (r == -1) return -1;

        f->cache_curr_pos = pos - f->file_tag;

        return 0;
    }

    // If new position is outside the cache
    else {
        io61_flush(f);

        f->file_tag = off_t(pos / BUFSIZE * BUFSIZE);

        off_t r = lseek(f->fd, pos, SEEK_SET);
        if (r == -1) return -1;

        f->cache_curr_pos = pos % BUFSIZE;
        f->cache_valid_length = 0;

        return 0;
    }
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}
