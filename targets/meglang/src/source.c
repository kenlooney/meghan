// Copyright 2026 Kenneth Looney
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "meglang/source.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Copies a string of bytes and adds a null terminator. Returns NULL on failure.
// The caller is responsible for freeing the returned string.
// If length is SIZE_MAX, returns NULL to avoid overflow.
// This function is used to copy the text of a source file into a new buffer.
// The returned string is guaranteed to be null-terminated, even if the input text is not.
static char *copy_bytes(const char *text, size_t length)
{
    char *copy;
    if (length == SIZE_MAX)
    {
        return NULL;
    }
    copy = malloc(length + 1);
    if (!copy)
    {
        return NULL;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

// Initializes out with owned copies of path and the null-terminated source text.
// Returns false for invalid arguments or allocation failure, leaving out empty.
// The initialized source must be released with source_destroy.
bool source_from_string(Source *out, const char *path, const char *text)
{
    size_t length;
    if (!out)
        return false;

    *out = (Source){0};
    if (!path || !text)
        return false;

    length = strlen(text);
    out->path = copy_bytes(path, strlen(path));
    out->text = copy_bytes(text, length);
    if (!out->path || !out->text)
    {
        source_destroy(out);
        return false;
    }
    out->length = length;
    return true;
}

// Loads a file in binary mode and initializes out with owned copies of its path
// and null-terminated contents. Returns false for invalid arguments, file I/O
// errors, files too large to represent, or allocation failure, leaving out empty.
// The initialized source must be released with source_destroy.
bool source_load(Source *out, const char *path)
{
    FILE *file;
    long end;
    size_t length;
    char *text;
    char *path_copy;
    if (!out)
        return false;
    *out = (Source){0};
    if (!path)
        return false;
    file = fopen(path, "rb");
    if (!file)
        return false;
    if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    length = (size_t)end;
    if((long) length != end || length == SIZE_MAX)
    {
        fclose(file);
        return false;
    }
    text = malloc(length + 1);
    if (!text)
    {
        fclose(file);
        return false;
    }
    if (fread(text, 1, length, file) != length)
    {
        free(text);
        fclose(file);
        return false;
    }
    fclose(file);
    text[length] = '\0';
    path_copy = copy_bytes(path, strlen(path));
    if (!path_copy)
    {
        free(text);
        return false;
    }
    out->path = path_copy;
    out->text = text;
    out->length = length;
    return true;
}

// Releases the path and text owned by source, then resets it to an empty state.
// Does nothing when source is NULL.
void source_destroy(Source *source)
{
    if (!source)
        return;
    free(source->path);
    free(source->text);
    *source = (Source){0};
}

// Returns true when span references a source and its byte range lies entirely
// within that source. The subtraction-based length check avoids size_t overflow.
bool span_valid(SourceSpan span) {
    return span.source && span.start <= span.source->length &&
           span.length <= span.source->length - span.start;
}

// Returns true when span is valid and contains exactly the same bytes as the
// null-terminated text. Returns false when text is NULL.
bool span_equals(SourceSpan span, const char *text) {
    size_t length;
    if (!span_valid(span) || !text)
        return false;
    length = strlen(text);
    return length == span.length &&
        memcmp(span.source->text + span.start, text, length) == 0;
}

// Writes the bytes referenced by span to out. Returns true only when out is
// non-NULL, span is valid, and every byte is written successfully.
bool span_write(FILE *out, SourceSpan span)
{
    return out && span_valid(span) &&
           fwrite(span.source->text + span.start, 1, span.length, out) == span.length;
}
