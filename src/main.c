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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <ofa1/ofa.h>
#include <sndfile.h>

#define ENV_NO_TEMP "AFPRINT_NO_TEMP"
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

static unsigned char *
convert_raw(const float *data, long frames, long samplerate, int channels)
{
	int pfd[2], pid, status;
	sf_count_t bufsize, ret;
	unsigned char *buf;
	SNDFILE *sf;
	SF_INFO info;

	info.format = SF_FORMAT_AU | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
	info.channels = channels;
	info.samplerate = samplerate;

	if (pipe(pfd) < 0) {
		lg("Failed to create pipe: %s", strerror(errno));
		return NULL;
	}

	bufsize = frames * channels * 2;
	if ((buf = calloc(bufsize, sizeof(unsigned char))) == NULL) {
		lg("Failed to allocate memory: %s", strerror(errno));
		close(pfd[0]);
		close(pfd[1]);
		return NULL;
	}

	if ((pid = fork()) < 0) {
		lg("Failed to fork: %s", strerror(errno));
		free(buf);
		close(pfd[0]);
		close(pfd[1]);
		return NULL;
	}
	else if (pid == 0) { /* child */
		free(buf);
		close(pfd[0]);

		if ((sf = sf_open_fd(pfd[1], SFM_WRITE, &info, 1)) == NULL) {
			lg("Failed to open pipe for writing: %s", sf_strerror(NULL));
			close(pfd[1]);
			_exit(EXIT_FAILURE);
		}
		ret = sf_writef_float(sf, data, frames);
		sf_close(sf);
		_exit(ret == frames ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	/* parent */
	close(pfd[1]);

	if ((sf = sf_open_fd(pfd[0], SFM_READ, &info, 1)) == NULL) {
		lg("Failed to open pipe for reading: %s", sf_strerror(NULL));
		close(pfd[0]);
		kill(pid, SIGKILL);
		return NULL;
	}

	ret = sf_read_raw(sf, buf, bufsize);
	sf_close(sf);
	wait(&status);
	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == EXIT_SUCCESS);
	assert(ret == bufsize);

	return buf;
}

static int
copy_stdin_temp(void)
{
	int fd, ret, written;
	FILE *fp;

	if (!(fp = tmpfile())) {
		lg("Failed to create temporary file: %s", strerror(errno));
		return -1;
	}
	fd = fileno(fp);

#ifdef HAVE_SPLICE
	int pfd[2];

	if (pipe(pfd) < 0) {
		lg("Failed to create pipe: %s", strerror(errno));
		return -1;
	}

	for (;;) {
		ret = splice(STDIN_FILENO, NULL, pfd[1], NULL, 4096, 0);
		if (!ret)
			break;
		if (ret < 0) {
			lg("Splicing data from standard input failed: %s", strerror(errno));
			close(pfd[0]);
			close(pfd[1]);
			goto fail;
		}

		do {
			written = splice(pfd[0], NULL, fd, NULL, ret, 0);
			if (!written) {
				lg("Splicing data to temporary file failed: %s", strerror(errno));
				close(pfd[0]);
				close(pfd[1]);
				goto fail;
			}
			if (written < 0) {
				lg("Splicing data to temporary file failed: %s", strerror(errno));
				close(pfd[0]);
				close(pfd[1]);
				goto fail;
			}
			ret -= written;
		} while (ret);
	}

	close(pfd[0]);
	close(pfd[1]);
#else
	char buf[4096], *p;

	for (;;) {
		ret = read(STDIN_FILENO, buf, 4096);
		if (!ret)
			break;
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			lg("Reading from standard input failed: %s", strerror(errno));
			goto fail;
		}

		p = buf;
		do {
			written = write(fd, p, ret);
			if (!written) {
				lg("Writing to temporary file failed: %s", strerror(errno));
				goto fail;
			}
			if (written < 0) {
				if (errno == EINTR)
					continue;
				lg("Writing to temporary file failed: %s", strerror(errno));
				goto fail;
			}
			p += written;
			ret -= written;
		} while (ret);
	}
#endif /* HAVE_SPLICE */

	if (lseek(fd, 0, SEEK_SET) < 0) {
		lg("Seeking in temporary file failed: %s", strerror(errno));
		goto fail;
	}

	return fd;
fail:
	close(fd);
	return -1;
}

static bool
dump_print(const char *path)
{
	bool isstdin;
	int ret, tmpfd;
	long duration, eframes;
	size_t data_size;
	float *data;
	unsigned char *buf;
	const char *fingerprint;
	SNDFILE *input;
	SF_INFO info;
	SF_FORMAT_INFO format_info;

	isstdin = (0 == strncmp(path, "-", 2));
	info.format = 0;

	if (isstdin) {
		if (!getenv(ENV_NO_TEMP)) {
			if ((tmpfd = copy_stdin_temp()) < 0)
				return false;
			input = sf_open_fd(tmpfd, SFM_READ, &info, SF_TRUE);
		}
		else
			input = sf_open_fd(STDIN_FILENO, SFM_READ, &info, SF_TRUE);
	}
	else
		input = sf_open(path, SFM_READ, &info);

	if (!input) {
		lg("Failed to open %s: %s", isstdin ? "stdin" : path, sf_strerror(NULL));
		return false;
	}

	duration = info.frames / (info.samplerate / 1000);
	format_info.format = info.format;
	sf_command(input, SFC_GET_FORMAT_INFO, &format_info, sizeof(format_info));
	lgv("Format: %s", format_info.name);
	lgv("Frames: %ld", (long)info.frames);
	lgv("Channels: %d", info.channels);
	lgv("Samplerate: %dHz", info.samplerate);
	lgv("Duration: %ldms", duration);

	eframes = ESSENTIAL_SECONDS * info.samplerate;
	if (eframes > info.frames) {
		lgv("essential frames: %ld > frames: %ld, adjusting", eframes, (long)info.frames);
		eframes = info.frames;
	}

	data_size = eframes * info.channels * sizeof(float);
	if (!(data = malloc(data_size))) {
		lg("Failed to allocate memory for data: %s", strerror(errno));
		return NULL;
	}
	memset(data, 0, data_size);

	ret = sf_readf_float(input, data, eframes);
	assert(ret == eframes);
	sf_close(input);

	buf = convert_raw(data, eframes, info.samplerate, info.channels);
	free(data);
	if (!buf)
		return false;

	fingerprint = ofa_create_print(buf, OFA_LITTLE_ENDIAN,
			eframes * info.channels, info.samplerate, 2 == info.channels);
	free(buf);
	if (!fingerprint) {
		lg("Failed to calculate fingerprint for %s", isstdin ? "stdin" : path);
		return false;
	}

	if (isstdin)
		printf("/dev/stdin.%s", format_info.extension);
	else
		printf("%s", path);
	fputc(print0 ? '\0' : ' ', stdout);

	printf("%ld", duration);
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

	return dump_print(argv[0]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
