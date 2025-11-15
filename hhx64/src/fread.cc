/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "fread.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * read a file from the provided path.
 * returns empty fread_data if the file has no content/does not exist.
 *
 * caller is responsible for freeing fread_data.content
 */
fread_data fread_file(const char* path)
{
    fread_data result = { 0, 0 };

    /** open as read bytes */
    FILE* f = fopen(path, "rb");
    if (!f) return result;

    /** get the length of the file by reading to the end */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return result;
    }

    /** allocate file size in memory */
    result.content = (char*)malloc(file_size);
    if (!result.content) {
        fclose(f);
        return result;
    }

    long total = 0;
    while (total < file_size) {
        long to_read = file_size - total;
        /** read the file in chunks, this increases the read speed for large files */
        if (to_read > 65536) to_read = 65536;

        long n = fread(result.content + total, 1, to_read, f);
        if (n == 0) break;

        total += n;
    }

    /** total read bytes */
    result.size = total;
    fclose(f);
    return result;
}
