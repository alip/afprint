/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Ali Polatel
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void about(void) {
    fprintf(stderr, PACKAGE"-"VERSION);
#ifdef GITHEAD
    if (0 != strlen(GITHEAD))
        fprintf(stderr, "-"GITHEAD);
#endif
    fputc('\n', stderr);
}

void usage(void) {
    fprintf(stderr, PACKAGE"-"VERSION);
#ifdef GITHEAD
    if (0 != strlen(GITHEAD))
        fprintf(stderr, "-"GITHEAD);
#endif
    fprintf(stderr, " audio fingerprinting tool\n");
    fprintf(stderr, "Usage: "PACKAGE" < file.wav\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "\t-h, --help\tYou're looking at it :)\n");
    fprintf(stderr, "\t-V, --version\tShow version information\n");
}

const char *create_print(int fd, int close_desc) {
    SNDFILE *input;
    SF_INFO info;

    input = sf_open_fd(fd, SFM_READ, &info, close_desc);
    if (NULL == input) {
        fprintf(stderr, PACKAGE": %s\n", sf_strerror(NULL));
        return NULL;
    }
    assert(info.format & SF_FORMAT_WAV);
    assert(info.format & SF_FORMAT_PCM_16);

    int *data = malloc(info.frames * info.channels * sizeof(int));
    if (NULL == data) {
        fprintf(stderr, PACKAGE": malloc failed: %s", strerror(errno));
        return NULL;
    }
    sf_readf_int(input, data, info.frames);
    sf_close(input);

    const char *p = ofa_create_print((unsigned char *) data, ENDIAN_CPU,
            info.frames, info.samplerate, info.channels == 2 ? 1 : 0);
    free(data);
    return p;
}

int main(int argc, char **argv) {
    int optc;
    static struct option long_options[] = {
        {"help",    no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {0, 0, NULL, 0}
    };

    while (-1 != (optc = getopt_long(argc, argv, "hV", long_options, NULL))) {
        switch (optc) {
            case 'h':
                usage();
                return EXIT_SUCCESS;
            case 'V':
                about();
                return EXIT_SUCCESS;
            case '?':
            default:
                fprintf(stderr, "try "PACKAGE" --help for more information\n");
                return EXIT_FAILURE;
        }
    }

    const char *p = create_print(fileno(stdin), 0);
    if (NULL == p)
        return EXIT_FAILURE;
    printf("%s\n", p);
    return EXIT_SUCCESS;
}
