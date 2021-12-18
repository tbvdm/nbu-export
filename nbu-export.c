/*
 * Copyright (c) 2021 Tim van der Molen <tim@kariliq.nl>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "nbu.h"

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s backup [directory]\n", getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	struct nbu_ctx *ctx;
	const char *backup, *dir;

	switch (argc) {
	case 2:
		backup = argv[1];
		dir = ".";
		break;
	case 3:
		backup = argv[1];
		dir = argv[2];
		if (mkdir(dir, 0777) == -1 && errno != EEXIST)
			err(1, "mkdir: %s", dir);
		break;
	default:
		usage();
	}

	if (unveil(backup, "r") == -1)
		err(1, "unveil: %s", backup);

	if (unveil(dir, "rwc") == -1)
		err(1, "unveil: %s", dir);

	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		err(1, "pledge");

	if (nbu_open(&ctx, backup) == -1) {
		nbu_close(ctx);
		return 1;
	}

	if (nbu_export(ctx, dir) == -1) {
		nbu_close(ctx);
		return 1;
	}

	nbu_close(ctx);
	return 0;
}
