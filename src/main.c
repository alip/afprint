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

static bool verbose;
static bool print0;

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
			"Usage: "PACKAGE" [-hVv0] <infile>\n"
			"\nOptions:\n"
			"\t-h, --help\tDisplay usage and exit\n"
			"\t-V, --version\tDisplay version and exit\n"
			"\t-v, --verbose\tBe verbose\n"
			"\t-0, --print0\tDelimit path and fingerprint by null character instead of space\n"
			"If <infile> is '-' "PACKAGE" reads from standard input.\n");
	exit(exitval);
}

static bool
dump_fingerprint(const char *path)
{
	bool isstdin;
	const char *fingerprint;
	short *data;
	int essential_frames, ret;
	SNDFILE *input;
	SF_INFO info;
	SF_FORMAT_INFO format_info;

	isstdin = (0 == strncmp(path, "-", 2));

	info.format = 0;
	if (isstdin)
		input = sf_open_fd(fileno(stdin), SFM_READ, &info, 0);
	else
		input = sf_open(path, SFM_READ, &info);

	if (input == NULL) {
		lg("Failed to open %s: %s", isstdin ? "stdin" : path, sf_strerror(NULL));
		return false;
	}

	format_info.format = info.format;
	sf_command(input, SFC_GET_FORMAT_INFO, &format_info, sizeof(format_info));
	lgv("Format: %s", format_info.name);
	lgv("Frames: %ld", info.frames);
	lgv("Channels: %d", info.channels);
	lgv("Samplerate: %dHz", info.samplerate);

	if ((data = malloc(info.frames * info.channels * sizeof(short))) == NULL) {
		lg("Failed to allocate memory for data: %s", strerror(errno));
		return NULL;
	}
	memset(data, 0, info.frames * info.channels * sizeof(short));

	essential_frames = ESSENTIAL_SECONDS * info.samplerate * info.channels;
	if (essential_frames > info.frames) {
		lgv("essential_frames: %d > frames: %ld, adjusting", essential_frames, info.frames);
		essential_frames = info.frames;
	}

	ret = sf_readf_short(input, data, essential_frames);
	assert(essential_frames == ret);
	sf_close(input);

	fingerprint = ofa_create_print((unsigned char *) data, ENDIAN_CPU,
			essential_frames, info.samplerate, 2 == info.channels);
	free(data);

	if (isstdin)
		printf("/dev/stdin.%s", format_info.extension);
	else
		printf("%s", path);

	fputc(print0 ? '\0' : ' ', stdout);

	printf("%s\n", fingerprint);
	fflush(stdout);

	return true;
}

int
main(int argc, char **argv)
{
	int optc;
	struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{"verbose", no_argument, NULL, 'v'},
		{"print0", no_argument, NULL, '0'},
		{0, 0, NULL, 0}
	};

	verbose = false;
	print0 = false;
	while ((optc = getopt_long(argc, argv, "hVv0", long_options, NULL)) != -1) {
		switch (optc) {
		case 'h':
			usage(stdout, 0);
		case 'V':
			about();
			return EXIT_SUCCESS;
		case 'v':
			verbose = true;
			break;
		case '0':
			print0 = true;
			break;
		case '?':
		default:
			usage(stderr, 1);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage(stderr, 1);

	return dump_fingerprint(argv[0]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
