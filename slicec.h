#pragma once

// libc includes (available in both C and C++)
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// A slice represents an immutable substring.
// Assumptions:
//      * the underlying string outlives the slice
//      * the underlying string is long enough
//      * the underlying string can be represented with single byte
//        characters (e.g. ASCII)
//
//      Slice representing "cde"
//          +---+---+
//          | o | 3 |
//          +-|-+---+
//            |
//            v
//       ...abcdefg...
//
// This class is intended as a light-weight wrapper around pointer and length
// and should be passed around by value.
//
typedef struct Slice {
    char const *start;      // where does the string start in memory?
                             // The 2 uses of 'const' express different ideas:
                             //    * the first says that we can't change the
                             //      character we point to
                             //    * the second says that we can't change the
                             //      place we point to
    size_t len;              // How many characters in the string
} Slice;

// constructor for slice using len
Slice sliceConstructorLen(char const *s, size_t const l) {
    Slice slice;
    slice.start = s;
    slice.len = l;
    return slice;
}

// constructor for slice using endpoint
Slice sliceConstructorEnd(char const *s, char const *end) {
    Slice slice;
    slice.start = s;
    slice.len = (size_t)(end - s);
    return slice;
}

bool sliceEqualString(Slice const slice, char const *p) {
    for (size_t i = 0; i < slice.len; i++) {
        if (p[i] != slice.start[i]) {
            return false;
        }
    }
    return p[slice.len] == 0;
}

bool sliceEqualSlice(Slice const slice, Slice const other) {
    if (slice.len != other.len) {
        return false;
    }
    for (size_t i = 0; i < slice.len; i++) {
        if (other.start[i] != slice.start[i]) {
            return false;
        }
    }
    return true;
}

bool isIdentifier(Slice slice) {
    if (slice.len == 0) {
        return 0;
    }
    if (!isalpha(slice.start[0])) {
        return false;
    }
    for (size_t i = 1; i < slice.len; i++) {
        if (!isalnum(slice.start[i])) {
            return false;
        }
    }
    return true;
}

void printSlice(Slice const slice) {
    for (size_t i = 0; i < slice.len; i++) {
        printf("%c", slice.start[i]);
    }
}

// find the hash of the string contained within the slice key
uint64_t hashSlice(Slice const key) {
    uint64_t out = 5381;
    for (size_t i = 0; i < key.len; i++) {
        char const c = key.start[i];
        out = out * 33 + (uint64_t)c;
    }
    return out;
}
