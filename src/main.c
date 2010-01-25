/* vim: set cino= fo=croql sw=8 ts=8 sts=0 noet cin fdm=syntax : */

/*
 * Copyright (c) 2009, 2010 Ali Polatel <alip@exherbo.org>
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
#include <stdbool.h>
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

#define ESSENTIAL_SECONDS 135

static int verbose;

#define lg(...) __lg(__func__, __LINE__, __VA_ARGS__)
#define lgv(...)						\
	do {							\
		if (verbose)					\
			__lg(__func__, __LINE__, __VA_ARGS__);	\
	} while (0)

#if defined(__GNUC__) && (__GNUC__ >= 3)
__attribute__ ((format (printf, 3, 4)))
#endif
static void
__lg(const char *func, size_t len, const char *fmt, ...)
{
	va_list args;

	fprintf(stderr, "[%s.%zu] ", func, len);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fputc('\n', stderr);
}

static void
about(void)
{
	printf(PACKAGE"-"VERSION GIT_HEAD"\n");
}

#if defined(__GNUC__) && (__GNUC__ >= 3)
__attribute__ ((noreturn))
#endif
static void
usage(FILE *outf, int exitval)
{
	fprintf(outf, PACKAGE"-"VERSION GIT_HEAD" audio fingerprinting tool\n"
			"Usage: "PACKAGE" [-hVv] /path/to/libsndfile/supported/audio/file...\n"
			"\nOptions:\n"
			"\t-h, --help\tYou're looking at it :)\n"
			"\t-V, --version\tShow version information\n"
			"\t-v, --verbose\tBe verbose\n"
			"If filename is '-' "PACKAGE" reads from standard input.\n");
	exit(exitval);
}

static const char *
create_print(int fd, int close_desc)
{
	const char *fingerprint;
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

	if ((data = malloc(info.frames * info.channels * sizeof(short))) == NULL) {
		lg("Failed to allocate memory for data: %s", strerror(errno));
		return NULL;
	}
	memset(data, 0, info.frames * info.channels * sizeof(short));

	essential_frames = ESSENTIAL_SECONDS * info.samplerate * info.channels;
	lgv("Reading audio data, essential seconds: %d essential frames: %d", ESSENTIAL_SECONDS, essential_frames);
	if (essential_frames > info.frames) {
		lgv("Essential frames: %d > info.frames: %ld, adjusting", essential_frames, info.frames);
		essential_frames = info.frames;
	}
	ret = sf_readf_short(input, data, essential_frames);
	assert(essential_frames == ret);

	fingerprint = ofa_create_print((unsigned char *) data, ENDIAN_CPU,
			essential_frames, info.samplerate, 2 == info.channels);

	sf_close(input);
	free(data);
	return fingerprint;
}

int
main(int argc, char **argv)
{
	bool isstdin;
	const char *fingerprint;
	int optc, fd, ret;
	FILE *fp;
	struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{"verbose", no_argument, NULL, 'v'},
		{0, 0, NULL, 0}
	};

	verbose = 0;
	while ((optc = getopt_long(argc, argv, "hVv", long_options, NULL)) != -1) {
		switch (optc) {
		case 'h':
			usage(stdout, 0);
		case 'V':
			about();
			return EXIT_SUCCESS;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage(stderr, 1);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc == 0) {
		fprintf(stderr, "No file given!\n");
		return EXIT_FAILURE;
	}

	isstdin = false;
	ret = EXIT_SUCCESS;
	for (int i = 0; i < argc; i++) {
		if (strncmp(argv[i], "-", 2) == 0) {
			isstdin = true;
			fd = fileno(stdin);
		}
		else {
			if ((fp = fopen(argv[i], "r")) == NULL) {
				lg("Failed to open file `%s': %s", argv[i], strerror(errno));
				ret = EXIT_FAILURE;
				continue;
			}
			fd = fileno(fp);
		}
		if ((fingerprint = create_print(fd, 1)) == NULL) {
			ret = EXIT_FAILURE;
			continue;
		}
		printf("%s: %s\n", isstdin ? "/dev/stdin" : argv[i], fingerprint);
		isstdin = false;
	}
	return ret;
}
