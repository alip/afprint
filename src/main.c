/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Ali Polatel <polatel@gmail.com>
 *
 * This file is part of the afprint audio fingerprint tool. afprint is free
 * software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License version 2, as published by the Free Software
 * Foundation.
 *
 * afprint is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ofa1/ofa.h>
#include <sndfile.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WORDS_BIGENDIAN
#define ENDIAN_CPU OFA_BIG_ENDIAN
#else
#define ENDIAN_CPU OFA_LITTLE_ENDIAN
#endif

static int verbose;

// Prototypes
void __lg(const char *func, size_t len, const char *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));
#define lg(...)     __lg(__func__, __LINE__, __VA_ARGS__)
#define lgv(...)                                        \
        do {                                            \
            if (verbose)                                \
                __lg(__func__, __LINE__, __VA_ARGS__);  \
        } while (0)

void about(void)
{
    fprintf(stderr, PACKAGE"-"VERSION);
#ifdef GITHEAD
    if (0 != strlen(GITHEAD))
        fprintf(stderr, "-"GITHEAD);
#endif
    fputc('\n', stderr);
}

void usage(void)
{
    fprintf(stderr, PACKAGE"-"VERSION);
#ifdef GITHEAD
    if (0 != strlen(GITHEAD))
        fprintf(stderr, "-"GITHEAD);
#endif
    fprintf(stderr, " audio fingerprinting tool\n");
    fprintf(stderr, "Usage: "PACKAGE" < /path/to/libsndfile/supported/audio/file\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "\t-h, --help\tYou're looking at it :)\n");
    fprintf(stderr, "\t-V, --version\tShow version information\n");
    fprintf(stderr, "\t-v, --verbose\tBe verbose\n");
}

void __lg(const char *func, size_t len, const char *fmt, ...)
{
    va_list args;

    fprintf(stderr, PACKAGE"@%ld: [%s:%zu] ", time(NULL), func, len);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}

const char *create_print(int fd, int close_desc)
{
    const char *fp;
    short *data;
    int essential_frames, ret;
    SNDFILE *input;
    SF_INFO info;

    lgv("Opening fd %d for reading", fd);
    info.format = 0;
    input = sf_open_fd(fd, SFM_READ, &info, close_desc);
    if (NULL == input) {
        lg("Failed to open file: %s", sf_strerror(NULL));
        return NULL;
    }

    if (verbose) {
        SF_FORMAT_INFO format_info;
        format_info.format = info.format;
        sf_command(input, SFC_GET_FORMAT_INFO, &format_info, sizeof(format_info));
        lg("Format: %s", format_info.name);
        lg("Frames: %ld", info.frames);
        lg("Channels: %d", info.channels);
        lg("Samplerate: %dHz", info.samplerate);
    }

    data = malloc(info.frames * info.channels * sizeof(short));
    if (NULL == data) {
        lg("Failed to allocate memory for data: %s", strerror(errno));
        return NULL;
    }

    // Only get first 135 seconds
    essential_frames = 135 * info.samplerate;
    if (essential_frames > info.frames)
        essential_frames = info.frames;
    ret = sf_readf_short(input, data, essential_frames);
    assert(essential_frames == ret);

    fp = ofa_create_print((unsigned char *) data, ENDIAN_CPU,
            essential_frames, info.samplerate, 2 == info.channels);

    sf_close(input);
    free(data);
    return fp;
}

int main(int argc, char **argv)
{
    const char *fp;
    int optc;
    static struct option long_options[] = {
        {"help",    no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {"verbose", no_argument, NULL, 'v'},
        {0, 0, NULL, 0}
    };

    verbose = 0;
    while (-1 != (optc = getopt_long(argc, argv, "hVv", long_options, NULL))) {
        switch (optc) {
            case 'h':
                usage();
                return EXIT_SUCCESS;
            case 'V':
                about();
                return EXIT_SUCCESS;
            case 'v':
                verbose = 1;
                break;
            case '?':
            default:
                fprintf(stderr, "try "PACKAGE" --help for more information\n");
                return EXIT_FAILURE;
        }
    }

    fp = create_print(fileno(stdin), 0);
    if (NULL == fp)
        return EXIT_FAILURE;
    printf("%s\n", fp);
    return EXIT_SUCCESS;
}

