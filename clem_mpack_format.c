#include "external/mpack.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

size_t strbinbuf_size = 1024;
char* strbinbuf;

static char g_bin_to_hex[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

#define INDENT_STRING "          "

void read_bytes_chunked(mpack_reader_t* reader, char* buf, size_t bufsize) {
    size_t bufcnt = 0;
    size_t chunk_size = 1024;
    while (bufcnt < bufsize) {
        if (bufcnt + chunk_size > bufsize) {
            chunk_size = bufsize - bufcnt;
        }
        mpack_read_bytes(reader, buf + bufcnt, chunk_size);
        bufcnt += chunk_size;
    }
}

void print_bytes(const char* strbinbuf, size_t bufsize, size_t bytes_per_line) {
    char* line = malloc(bytes_per_line * 3);
    size_t start = 0;
    size_t end;
    size_t i;
    uint8_t byte;
    while (start < bufsize) {
        end = start + bytes_per_line;
        if (end > bufsize) {
            end = bufsize;
        }
        for (i = start; i < end; ++i) {
            byte = strbinbuf[i];
            line[(i - start) * 3] = g_bin_to_hex[byte >> 4];
            line[(i - start) * 3 + 1] = g_bin_to_hex[byte & 0xf];
            if (i + 1 == end) {
                line[(i - start) * 3 + 2] = '\0';
            }
        }
        fprintf(stdout, "%s\n", line);
        start = end;
    }
    free(line);
}

void print_message(mpack_reader_t* reader, int level) {
    mpack_tag_t mpack_tag = mpack_read_tag(reader);
    unsigned index;

    switch (mpack_tag.type) {
        case mpack_type_missing:
            break;
        case mpack_type_nil:
        case mpack_type_bool:
            if (mpack_tag.v.b) {
                fprintf(stdout, "true");
            } else {
                fprintf(stdout, "false");
            }
            break;
        case mpack_type_int:
            fprintf(stdout, "%"PRId64, mpack_tag.v.i);
            break;
        case mpack_type_uint:        /**< A 64-bit unsigned integer. */
            fprintf(stdout, "%"PRIu64, mpack_tag.v.u);
            break;
        case mpack_type_float:       /**< A 32-bit IEEE 754 floating point number. */
            fprintf(stdout, "%f", mpack_tag.v.f);
            break;
        case mpack_type_double:      /**< A 64-bit IEEE 754 floating point number. */
            fprintf(stdout, "%lf", mpack_tag.v.d);
            break;
        case mpack_type_str:         /**< A string. */
            if (mpack_tag.v.l > strbinbuf_size - 1) {
                free(strbinbuf);
                strbinbuf_size = mpack_tag.v.l + 1;
                strbinbuf = (char *)malloc(strbinbuf_size);
            }
            read_bytes_chunked(reader, strbinbuf, mpack_tag.v.l);
            strbinbuf[mpack_tag.v.l] = '\0';
            fprintf(stdout, "\"%s\"", strbinbuf);
            mpack_done_str(reader);
            break;
        case mpack_type_bin:         /**< A chunk of binary data. */
            if (mpack_tag.v.l > strbinbuf_size) {
                free(strbinbuf);
                strbinbuf_size = mpack_tag.v.l;
                strbinbuf = (char *)malloc(strbinbuf_size);
            }
            read_bytes_chunked(reader, strbinbuf, mpack_tag.v.l);
            print_bytes(strbinbuf, mpack_tag.v.l, 16);
            mpack_done_bin(reader);
            break;
        case mpack_type_array:       /**< An array of MessagePack objects. */
            fprintf(stdout, "%.*s[\n", level, INDENT_STRING);
            for (index = 0; index < mpack_tag.v.l; ++index) {
                print_message(reader, level + 1);
                fprintf(stdout, ",\n");
            }
            fprintf(stdout, "%.*s]\n", level, INDENT_STRING);
            mpack_done_array(reader);
            break;
        case mpack_type_map:         /**< An ordered map of key/value pairs of MessagePack objects. */
            fprintf(stdout, "%.*s{\n", level, INDENT_STRING);
            for (index = 0; index < mpack_tag.v.l; ++index) {
                print_message(reader, level + 1);
                fprintf(stdout, " : ");
                print_message(reader, level + 1);
                fprintf(stdout, ",\n");
            }
            fprintf(stdout, "%.*s}\n", level, INDENT_STRING);
            mpack_done_map(reader);
            break;
    }
}

int main(int argc, char* argv[]) {
    mpack_reader_t reader;

    if (argc < 2) {
        fprintf(stdout, "usage: clem_mpack_format <msgpack_file>\n");
        return 0;
    }
    strbinbuf = (char*)malloc(strbinbuf_size);
    mpack_reader_init_filename(&reader, argv[1]);
    print_message(&reader, 0);
    mpack_reader_destroy(&reader);

    free(strbinbuf);

    return 0;
}
