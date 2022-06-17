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

#define NBU_DEBUG

#include <sys/queue.h>
#include <sys/stat.h>

#include <endian.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef NBU_DEBUG
#include <stdarg.h>
#endif

#include "utf.h"

#define NBU_CALENDAR_FILE	"calendar.ics"
#define NBU_CONTACTS_FILE	"contacts.vcf"
#define NBU_MEMOS_DIR		"memos"
#define NBU_MESSAGES_DIR	"messages"

#define NBU_GUID_LEN 16

#ifndef nitems
#define nitems(a) (sizeof (a) / sizeof (a)[0])
#endif

#ifdef NBU_DEBUG
#define NBU_DPRINTF(...) nbu_dprintf(__func__, __VA_ARGS__)
#else
#define NBU_DPRINTF(...)
#endif

struct nbu_item {
	long		 pos;
	uint32_t	 len;
	STAILQ_ENTRY(nbu_item) entries;
};

STAILQ_HEAD(nbu_item_list, nbu_item);

struct nbu_folder {
	uint16_t	*name;
	struct nbu_item_list *items;
	STAILQ_ENTRY(nbu_folder) entries;
};

STAILQ_HEAD(nbu_folder_list, nbu_folder);

struct nbu_ctx {
	FILE		*fp;

	uint64_t	 backup_time;
	uint16_t	*phone_imei;
	uint16_t	*phone_model;
	uint16_t	*phone_name;
	uint16_t	*phone_firmware;
	uint16_t	*phone_language;

	struct nbu_folder_list *bookmarks;
	struct nbu_folder_list *messages;
	struct nbu_folder_list *mmses;
	struct nbu_item_list *calendar;
	struct nbu_item_list *contacts;
	struct nbu_item_list *memos;
};

struct nbu_section {
	uint8_t		 guid[NBU_GUID_LEN];
	int		 (*read)(struct nbu_ctx *, uint64_t);
};

static int nbu_read_advanced_settings_section(struct nbu_ctx *, uint64_t);
static int nbu_read_bookmarks_section(struct nbu_ctx *, uint64_t);
static int nbu_read_calendar_section(struct nbu_ctx *, uint64_t);
static int nbu_read_groups_section(struct nbu_ctx *, uint64_t);
static int nbu_read_contacts_section(struct nbu_ctx *, uint64_t);
static int nbu_read_memos_section(struct nbu_ctx *, uint64_t);
static int nbu_read_messages_section(struct nbu_ctx *, uint64_t);
static int nbu_read_mms_section(struct nbu_ctx *, uint64_t);

static const struct nbu_section nbu_sections[] = {
	{
		{
			0x16, 0xcd, 0xf8, 0xe8, 0x23, 0x5e, 0x5a, 0x4e,
			0xb7, 0x35, 0xdd, 0xdf, 0xf1, 0x48, 0x12, 0x22
		},
		nbu_read_calendar_section
	},
	{
		{
			0x1f, 0x0e, 0x58, 0x65, 0xa1, 0x9f, 0x3c, 0x49,
			0x9e, 0x23, 0x0e, 0x25, 0xeb, 0x24, 0x0f, 0xe1
		},
		nbu_read_groups_section
	},
	{
		{
			0x2d, 0xf5, 0x68, 0x6b, 0x1f, 0x4b, 0x22, 0x4a,
			0x92, 0x83, 0x1b, 0x06, 0xc3, 0xc3, 0x9a, 0x35
		},
		nbu_read_advanced_settings_section
	},
	{
		{
			0x47, 0x1d, 0xd4, 0x65, 0xef, 0xe3, 0x32, 0x40,
			0x8c, 0x77, 0x64, 0xca, 0xa3, 0x83, 0xaa, 0x33
		},
		nbu_read_mms_section
	},
	{
		{
			0x5c, 0x62, 0x97, 0x3b, 0xdc, 0xa7, 0x54, 0x41,
			0xa1, 0xc3, 0x05, 0x9d, 0xe3, 0x24, 0x68, 0x08
		},
		nbu_read_memos_section
	},
	{
		{
			0x61, 0x7a, 0xef, 0xd1, 0xaa, 0xbe, 0xa1, 0x49,
			0x9d, 0x9d, 0x15, 0x5a, 0xbb, 0x4c, 0xeb, 0x8e
		},
		nbu_read_messages_section
	},
	{
		{
			0x7f, 0x77, 0x90, 0x56, 0x31, 0xf9, 0x57, 0x49,
			0x8d, 0x96, 0xee, 0x44, 0x5d, 0xbe, 0xbc, 0x5a
		},
		nbu_read_bookmarks_section
	},
	{
		{
			0xef, 0xd4, 0x2e, 0xd0, 0xa3, 0x51, 0x38, 0x47,
			0x9d, 0xd7, 0x30, 0x5c, 0x7a, 0xf0, 0x68, 0xd3
		},
		nbu_read_contacts_section
	},
};

#ifdef NBU_DEBUG
__attribute__((format(printf, 2, 3))) static void
nbu_dprintf(const char *func, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (func != NULL)
		fprintf(stderr, "%s: ", func);
	if (fmt != NULL)
		vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif

static int
nbu_read(struct nbu_ctx *ctx, void *ptr, size_t size)
{
	if (fread(ptr, size, 1, ctx->fp) != 1) {
		if (ferror(ctx->fp))
			warn("fread");
		else
			warnx("Unexpected end of file");
		return -1;
	}

	return 0;
}

static int
nbu_read_uint8(struct nbu_ctx *ctx, uint8_t *u)
{
	if (nbu_read(ctx, u, sizeof *u) == -1)
		return -1;

	return 0;
}

/* Read a little-endian uint16_t */
static int
nbu_read_uint16(struct nbu_ctx *ctx, uint16_t *u)
{
	if (nbu_read(ctx, u, sizeof *u) == -1)
		return -1;

	*u = le16toh(*u);
	return 0;
}

/* Read a little-endian uint32_t */
static int
nbu_read_uint32(struct nbu_ctx *ctx, uint32_t *u)
{
	if (nbu_read(ctx, u, sizeof *u) == -1)
		return -1;

	*u = le32toh(*u);
	return 0;
}

/* Read a little-endian uint64_t */
static int
nbu_read_uint64(struct nbu_ctx *ctx, uint64_t *u)
{
	if (nbu_read(ctx, u, sizeof *u) == -1)
		return -1;

	*u = le64toh(*u);
	return 0;
}

/* Read a little-endian UTF-16 string of the specified length */
static uint16_t *
nbu_read_utf16_n(struct nbu_ctx *ctx, size_t len)
{
	uint16_t *utf16;
	size_t i;

	if (len == SIZE_MAX) {
		warnx("UTF-16 string too long");
		return NULL;
	}

	if ((utf16 = reallocarray(NULL, len + 1, sizeof *utf16)) == NULL) {
		warn(NULL);
		return NULL;
	}

	for (i = 0; i < len; i++) {
		if (nbu_read_uint16(ctx, &utf16[i]) == -1) {
			free(utf16);
			return NULL;
		}
	}

	utf16[i] = 0;
	return utf16;
}

/* Read a little-endian UTF-16 string */
static uint16_t *
nbu_read_utf16(struct nbu_ctx *ctx)
{
	uint16_t len;

	if (nbu_read_uint16(ctx, &len) == -1)
		return NULL;

	return nbu_read_utf16_n(ctx, len);
}

/*
 * Read a Windows file time.
 *
 * A Windows file time is a 64-bit integer that represents the number of
 * 100-nanosecond intervals that have elapsed since 0:00 UTC on 1 January 1601.
 *
 * Windows uses two 32-bit integers to store a file time: one contains the
 * high-order part, the other the low-order part.
 */
static int
nbu_read_file_time(struct nbu_ctx *ctx, uint64_t *u)
{
	uint32_t hi, lo;

	if (nbu_read_uint32(ctx, &hi) == -1 ||
	    nbu_read_uint32(ctx, &lo) == -1)
		return -1;

	*u = ((uint64_t)hi << 32) | lo;
	return 0;
}

static int
nbu_seek(struct nbu_ctx *ctx, long offset, int whence)
{
	if (fseek(ctx->fp, offset, whence) == -1) {
		warn("fseek");
		return -1;
	}
	return 0;
}

static int
nbu_tell(struct nbu_ctx *ctx, long *pos)
{
	if ((*pos = ftell(ctx->fp)) == -1) {
		warn("ftell");
		return -1;
	}
	return 0;
}

static int
nbu_write(int fd, const void *buf, size_t len)
{
	size_t off;
	ssize_t n;

	for (off = 0; off < len; off += n) {
		n = write(fd, (const char *)buf + off, len - off);
		if (n == -1) {
			warn("write");
			return -1;
		}
	}

	return 0;
}

static uint8_t *
nbu_convert_utf16_to_utf8(const uint16_t *utf16)
{
	uint8_t *utf8;
	size_t size;

	size = utf16_convert_string_to_utf8(NULL, 0, utf16);
	if (size == SIZE_MAX) {
		warnx("UTF-16 string too long");
		return NULL;
	}

	if ((utf8 = malloc(++size)) == NULL) {
		warn(NULL);
		return NULL;
	}

	utf16_convert_string_to_utf8(utf8, size, utf16);
	return utf8;
}

#ifdef NBU_DEBUG
static const char *
nbu_guid_to_string(const uint8_t guid[NBU_GUID_LEN])
{
	char *p;
	int i;
	static char buf[NBU_GUID_LEN * 2 + 1];

	p = buf;
	for (i = 0; i < NBU_GUID_LEN; i++) {
		snprintf(p, 3, "%02x", guid[i]);
		p += 2;
	}

	return buf;
}

static void
nbu_print_time(const char *name, uint64_t t)
{
	/* XXX */
	NBU_DPRINTF("%s: %" PRIu64 "\n", name, t);
}

static void
nbu_print_utf16(const char *name, const uint16_t *utf16)
{
	uint8_t *utf8;

	if ((utf8 = nbu_convert_utf16_to_utf8(utf16)) == NULL)
		return;

	NBU_DPRINTF("%s: %s\n", name, utf8);
	free(utf8);
}

static void
nbu_print_phone_info(struct nbu_ctx *ctx)
{
	nbu_print_time("backup time", ctx->backup_time);
	nbu_print_utf16("phone IMEI", ctx->phone_imei);
	nbu_print_utf16("phone model", ctx->phone_model);
	nbu_print_utf16("phone name", ctx->phone_name);
	nbu_print_utf16("phone firmware", ctx->phone_firmware);
	nbu_print_utf16("phone language", ctx->phone_language);
}
#endif

struct nbu_item_list *
nbu_new_item_list(void)
{
	struct nbu_item_list *list;

	if ((list = malloc(sizeof *list)) == NULL) {
		warn(NULL);
		return NULL;
	}

	STAILQ_INIT(list);
	return list;
}

static void
nbu_free_item_list(struct nbu_item_list *list)
{
	struct nbu_item *item;

	if (list != NULL) {
		while ((item = STAILQ_FIRST(list)) != NULL) {
			STAILQ_REMOVE_HEAD(list, entries);
			free(item);
		}
		free(list);
	}
}

static int
nbu_add_item(struct nbu_item_list *list, long pos, uint32_t len)
{
	struct nbu_item *item;

	if ((item = malloc(sizeof *item)) == NULL) {
		warn(NULL);
		return -1;
	}

	item->pos = pos;
	item->len = len;
	STAILQ_INSERT_TAIL(list, item, entries);
	return 0;
}

static struct nbu_folder *
nbu_new_folder(void)
{
	struct nbu_folder *folder;

	if ((folder = malloc(sizeof *folder)) == NULL) {
		warn(NULL);
		return NULL;
	}

	if ((folder->items = nbu_new_item_list()) == NULL) {
		free(folder);
		return NULL;
	}

	folder->name = NULL;
	return folder;
}

static void
nbu_free_folder(struct nbu_folder *folder)
{
	if (folder != NULL) {
		free(folder->name);
		nbu_free_item_list(folder->items);
		free(folder);
	}
}

static struct nbu_folder_list *
nbu_new_folder_list(void)
{
	struct nbu_folder_list *list;

	if ((list = malloc(sizeof *list)) == NULL) {
		warn(NULL);
		return NULL;
	}

	STAILQ_INIT(list);
	return list;
}

static void
nbu_free_folder_list(struct nbu_folder_list *list)
{
	struct nbu_folder *folder;

	if (list != NULL) {
		while ((folder = STAILQ_FIRST(list)) != NULL) {
			STAILQ_REMOVE_HEAD(list, entries);
			nbu_free_folder(folder);
		}
		free(list);
	}
}

static int
nbu_export_item_to_fd(struct nbu_ctx *ctx, struct nbu_item *item, int fd)
{
	char *buf;
	int ret;

	ret = -1;

	if ((buf = malloc(item->len)) == NULL)
		goto out;

	if (nbu_seek(ctx, item->pos, SEEK_SET) == -1)
		goto out;

	if (nbu_read(ctx, buf, item->len) == -1)
		goto out;

	if (nbu_write(fd, buf, item->len) == -1)
		goto out;

	ret = 0;

out:
	free(buf);
	return ret;
}

static int
nbu_export_utf16_item_to_fd(struct nbu_ctx *ctx, struct nbu_item *item, int fd)
{
	uint16_t *utf16;
	uint8_t *utf8;
	size_t len;

	/* Sanity check */
	if (item->len % 2 != 0) {
		warnx("Invalid item size");
		return -1;
	}

	/* Convert length from bytes to UTF-16 code units */
	len = item->len / 2;

	if (nbu_seek(ctx, item->pos, SEEK_SET) == -1)
		return -1;

	if ((utf16 = nbu_read_utf16_n(ctx, len)) == NULL)
		return -1;

	if ((utf8 = nbu_convert_utf16_to_utf8(utf16)) == NULL) {
		free(utf16);
		return -1;
	}

	free(utf16);
	len = strlen((char *)utf8);

	if (nbu_write(fd, utf8, len) == -1) {
		free(utf8);
		return -1;
	}

	free(utf8);
	return 0;
}

static int
nbu_export_utf16_item(struct nbu_ctx *ctx, struct nbu_item *item, int dfd,
    const char *path)
{
	int fd;

	fd = openat(dfd, path, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd == -1) {
		warn("openat: %s", path);
		return -1;
	}

	if (nbu_export_utf16_item_to_fd(ctx, item, fd) == -1) {
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int
nbu_export_item_list(struct nbu_ctx *ctx, struct nbu_item_list *list, int dfd,
    const char *path)
{
	struct nbu_item *item;
	int fd;

	if (list == NULL || STAILQ_EMPTY(list))
		return 0;

	fd = openat(dfd, path, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd == -1) {
		warn("openat: %s", path);
		return -1;
	}

	STAILQ_FOREACH(item, list, entries) {
		if (nbu_export_item_to_fd(ctx, item, fd) == -1) {
			close(fd);
			return -1;
		}
	}

	close(fd);
	return 0;
}

static int
nbu_export_message_folder(struct nbu_ctx *ctx, struct nbu_folder *folder,
    int dfd, const char *path)
{
	struct nbu_item *item;
	char *base, *name;
	int fd;

	if ((base = (char *)nbu_convert_utf16_to_utf8(folder->name)) == NULL)
		return -1;

	if (base[0] == '\0' || strchr(base, '/') != NULL ||
	    strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
		warnx("Invalid folder name");
		free(base);
		return -1;
	}

	if (asprintf(&name, "%s/%s.vmg", path, base) == -1) {
		warnx("asprintf() failed");
		free(base);
	}

	free(base);

	fd = openat(dfd, name, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd == -1) {
		warn("openat");
		free(name);
		return -1;
	}

	free(name);

	STAILQ_FOREACH(item, folder->items, entries) {
		if (nbu_export_utf16_item_to_fd(ctx, item, fd) == -1) {
			close(fd);
			return -1;
		}
	}

	close(fd);
	return 0;
}

static int
nbu_export_calendar(struct nbu_ctx *ctx, int dfd)
{
	return nbu_export_item_list(ctx, ctx->calendar, dfd,
	    NBU_CALENDAR_FILE);
}

static int
nbu_export_contacts(struct nbu_ctx *ctx, int dfd)
{
	return nbu_export_item_list(ctx, ctx->contacts, dfd,
	    NBU_CONTACTS_FILE);
}

static int
nbu_export_memos(struct nbu_ctx *ctx, int dfd)
{
	struct nbu_item *item;
	int i, ret;
	char name[32];

	if (ctx->memos == NULL || STAILQ_EMPTY(ctx->memos))
		return 0;

	if (mkdirat(dfd, NBU_MEMOS_DIR, 0777) == -1 && errno != EEXIST) {
		warn("mkdirat: %s", NBU_MEMOS_DIR);
		return -1;
	}

	ret = 0;
	i = 1;

	STAILQ_FOREACH(item, ctx->memos, entries) {
		snprintf(name, sizeof name, "%s/memo-%d.txt", NBU_MEMOS_DIR,
		    i++);
		if (nbu_export_utf16_item(ctx, item, dfd, name) == -1)
			ret = -1;
	}

	return ret;
}

static int
nbu_export_messages(struct nbu_ctx *ctx, int dfd)
{
	struct nbu_folder *folder;
	int ret;

	if (ctx->messages == NULL || STAILQ_EMPTY(ctx->messages))
		return 0;

	if (mkdirat(dfd, NBU_MESSAGES_DIR, 0777) == -1 && errno != EEXIST) {
		warn("mkdirat: %s", NBU_MESSAGES_DIR);
		return -1;
	}

	ret = 0;

	STAILQ_FOREACH(folder, ctx->messages, entries) {
		if (nbu_export_message_folder(ctx, folder, dfd,
		    NBU_MESSAGES_DIR) == -1)
			ret = -1;
	}

	return ret;
}

static int
nbu_read_vcards(struct nbu_ctx *ctx, struct nbu_item_list *list)
{
	long pos;
	uint32_t i, len, nitems, test;

	if (nbu_read_uint32(ctx, &nitems) == -1)
		return -1;

	NBU_DPRINTF("%" PRIu32 " items\n", nitems);

	for (i = 0; i < nitems; i++) {
		/* NbuExplorer does this. I don't know why. */
		if (nbu_read_uint32(ctx, &test) == -1)
			return -1;
		if (test != 0x10)
			NBU_DPRINTF("test 1: 0x%" PRIx32 " != 0x10\n", test);
		else {
			if (nbu_read_uint32(ctx, &test) == -1)
				return -1;
			if (test > 1)
				NBU_DPRINTF("test 2: 0x%" PRIx32 " > 1\n",
				    test);
		}

		if (nbu_read_uint32(ctx, &len) == -1)
			return -1;

		if (nbu_tell(ctx, &pos) == -1)
			return -1;

		if (nbu_add_item(list, pos, len) == -1)
			return -1;

		if (nbu_seek(ctx, len, SEEK_CUR) == -1)
			return -1;
	}

	return 0;
}

static struct nbu_folder *
nbu_read_vcard_folder(struct nbu_ctx *ctx, uint64_t folder_pos)
{
	struct nbu_folder *folder;

	if ((folder = nbu_new_folder()) == NULL)
		goto error;

	if (nbu_seek(ctx, folder_pos + 4, SEEK_SET) == -1)
		goto error;

	if ((folder->name = nbu_read_utf16(ctx)) == NULL)
		goto error;

#ifdef NBU_DEBUG
	uint8_t *utf8;

	if ((utf8 = nbu_convert_utf16_to_utf8(folder->name)) == NULL)
		goto error;

	NBU_DPRINTF("folder \"%s\"\n", utf8);
	free(utf8);
#endif

	if (nbu_read_vcards(ctx, folder->items) == -1)
		goto error;

	return folder;

error:
	nbu_free_folder(folder);
	return NULL;
}

static int
nbu_read_vcard_section(struct nbu_ctx *ctx, uint64_t section_pos,
    struct nbu_item_list **list)
{
	long pos;
	uint32_t nfolders, nitems;

	if ((*list = nbu_new_item_list()) == NULL)
		goto error;

	if (nbu_read_uint32(ctx, &nitems) == -1)
		goto error;

	NBU_DPRINTF("%" PRIu32 " items\n", nitems);

	if (nbu_read_uint32(ctx, &nfolders) == -1)
		goto error;

	if (nfolders != 0) {
		warnx("Section unexpectedly contains folders");
		goto error;
	}

	if (nbu_tell(ctx, &pos) == -1)
		goto error;

	if (nbu_seek(ctx, section_pos + 44, SEEK_SET) == -1)
		goto error;

	if (nbu_read_vcards(ctx, *list) == -1)
		goto error;

	if (nbu_seek(ctx, pos, SEEK_SET) == -1)
		goto error;

	return 0;

error:
	nbu_free_item_list(*list);
	*list = NULL;
	return -1;
}

static int
nbu_read_vcard_folder_section(struct nbu_ctx *ctx,
    struct nbu_folder_list **list)
{
	struct nbu_folder *folder;
	long pos;
	uint64_t folder_pos;
	uint32_t i, nfolders, nitems;

	if ((*list = nbu_new_folder_list()) == NULL)
		goto error;

	if (nbu_read_uint32(ctx, &nitems) == -1)
		goto error;

	if (nbu_read_uint32(ctx, &nfolders) == -1)
		goto error;

	NBU_DPRINTF("%" PRIu32 " items in %" PRIu32 " folders\n", nitems,
	    nfolders);

	for (i = 0; i < nfolders; i++) {
		/* Skip folder id */
		if (nbu_seek(ctx, 4, SEEK_CUR) == -1)
			goto error;

		if (nbu_read_uint64(ctx, &folder_pos) == -1)
			goto error;

		if (nbu_tell(ctx, &pos) == -1)
			goto error;

		if ((folder = nbu_read_vcard_folder(ctx, folder_pos)) == NULL)
			goto error;

		STAILQ_INSERT_TAIL(*list, folder, entries);

		if (nbu_seek(ctx, pos, SEEK_SET) == -1)
			goto error;
	}

	return 0;

error:
	nbu_free_folder_list(*list);
	*list = NULL;
	return -1;
}

static struct nbu_folder *
nbu_read_group_folder(struct nbu_ctx *ctx, uint64_t folder_pos)
{
	struct nbu_folder *folder;
	uint32_t nitems;

	if ((folder = nbu_new_folder()) == NULL)
		goto error;

	if (nbu_seek(ctx, folder_pos + 4, SEEK_SET) == -1)
		goto error;

	if ((folder->name = nbu_read_utf16(ctx)) == NULL)
		goto error;

	if (nbu_read_uint32(ctx, &nitems) == -1)
		goto error;

#ifdef NBU_DEBUG
	uint8_t *utf8;

	if ((utf8 = nbu_convert_utf16_to_utf8(folder->name)) == NULL)
		goto error;

	NBU_DPRINTF("folder \"%s\", %" PRIu32 " items\n", utf8, nitems);
	free(utf8);
#endif

	/* TODO */

	return folder;

error:
	nbu_free_folder(folder);
	return NULL;
}

static struct nbu_folder *
nbu_read_message_folder(struct nbu_ctx *ctx, uint64_t folder_pos)
{
	struct nbu_folder *folder;
	long pos;
	uint32_t i, len, nitems;

	if ((folder = nbu_new_folder()) == NULL)
		goto error;

	if (nbu_seek(ctx, folder_pos + 4, SEEK_SET) == -1)
		goto error;

	if ((folder->name = nbu_read_utf16(ctx)) == NULL)
		goto error;

	if (nbu_read_uint32(ctx, &nitems) == -1)
		goto error;

#ifdef NBU_DEBUG
	uint8_t *utf8;

	if ((utf8 = nbu_convert_utf16_to_utf8(folder->name)) == NULL)
		goto error;

	NBU_DPRINTF("folder \"%s\", %" PRIu32 " messages\n", utf8, nitems);
	free(utf8);
#endif

	for (i = 0; i < nitems; i++) {
		if (nbu_seek(ctx, 8, SEEK_CUR) == -1)
			goto error;

		if (nbu_read_uint32(ctx, &len) == -1)
			goto error;

		if (nbu_tell(ctx, &pos) == -1)
			goto error;

		if (nbu_add_item(folder->items, pos, len) == -1)
			goto error;

		if (nbu_seek(ctx, len, SEEK_CUR) == -1)
			goto error;
	}

	return folder;

error:
	nbu_free_folder(folder);
	return NULL;
}

static struct nbu_folder *
nbu_read_mms_folder(struct nbu_ctx *ctx, uint64_t folder_pos)
{
	struct nbu_folder *folder;
	uint16_t *utf16;
	long pos;
	uint32_t i, len, nitems;
	uint8_t j, n;

	if ((folder = nbu_new_folder()) == NULL)
		goto error;

	if (nbu_seek(ctx, folder_pos + 4, SEEK_SET) == -1)
		goto error;

	if ((folder->name = nbu_read_utf16(ctx)) == NULL)
		goto error;

	if (nbu_read_uint32(ctx, &nitems) == -1)
		goto error;

#ifdef NBU_DEBUG
	uint8_t *utf8;

	if ((utf8 = nbu_convert_utf16_to_utf8(folder->name)) == NULL)
		goto error;

	NBU_DPRINTF("folder \"%s\", %" PRIu32 " messages\n", utf8, nitems);
	free(utf8);
#endif

	for (i = 0; i < nitems; i++) {
		if (nbu_seek(ctx, 8, SEEK_CUR) == -1)
			goto error;

		if (nbu_read_uint8(ctx, &n) == -1)
			goto error;

		NBU_DPRINTF("unknown number: %" PRIu8 "\n", n);

		for (j = 0; j < n; j++) {
			if (nbu_seek(ctx, 8, SEEK_CUR) == -1)
				goto error;

			if ((utf16 = nbu_read_utf16(ctx)) == NULL)
				goto error;

#ifdef NBU_DEBUG
			if ((utf8 = nbu_convert_utf16_to_utf8(utf16)) ==
			    NULL) {
				free(utf16);
				goto error;
			}

			NBU_DPRINTF("unknown string %" PRIu8 ": \"%s\"\n",
			    j + 1, utf8);
			free(utf8);
#endif

			/* TODO */

			free(utf16);
		}

		if (nbu_seek(ctx, 20, SEEK_CUR) == -1)
			goto error;

		if (nbu_read_uint32(ctx, &len) == -1)
			goto error;

		if (nbu_tell(ctx, &pos) == -1)
			goto error;

		if (nbu_add_item(folder->items, pos, len) == -1)
			goto error;

		if (nbu_seek(ctx, len, SEEK_CUR) == -1)
			goto error;
	}

	return folder;

error:
	nbu_free_folder(folder);
	return NULL;
}

static int
nbu_read_advanced_settings_section(struct nbu_ctx *ctx,
    __unused uint64_t section_pos)
{
	long pos;
	uint64_t folder_pos;
	uint32_t i, n, nfolders;

	NBU_DPRINTF("reading section\n");

	if (nbu_read_uint32(ctx, &n) == -1)
		return -1;

	/* XXX Probably total number of items */
	NBU_DPRINTF("unknown number: %" PRIu32 "\n", n);

	if (nbu_read_uint32(ctx, &nfolders) == -1)
		return -1;

	NBU_DPRINTF("%" PRIu32 " folders\n", nfolders);

	for (i = 0; i < nfolders; i++) {
		if (nbu_seek(ctx, 4, SEEK_CUR) == -1)
			return -1;

		if (nbu_read_uint64(ctx, &folder_pos) == -1)
			return -1;

		if (nbu_tell(ctx, &pos) == -1)
			return -1;

		/* TODO */

		if (nbu_seek(ctx, pos, SEEK_SET) == -1)
			return -1;
	}

	return 0;
}

static int
nbu_read_bookmarks_section(struct nbu_ctx *ctx, __unused uint64_t section_pos)
{
	NBU_DPRINTF("reading section\n");
	return nbu_read_vcard_folder_section(ctx, &ctx->bookmarks);
}

static int
nbu_read_calendar_section(struct nbu_ctx *ctx, uint64_t section_pos)
{
	NBU_DPRINTF("reading section\n");
	return nbu_read_vcard_section(ctx, section_pos, &ctx->calendar);
}

static int
nbu_read_contacts_section(struct nbu_ctx *ctx, uint64_t section_pos)
{
	NBU_DPRINTF("reading section\n");
	return nbu_read_vcard_section(ctx, section_pos, &ctx->contacts);
}

static int
nbu_read_groups_section(struct nbu_ctx *ctx, __unused uint64_t section_pos)
{
	struct nbu_folder *folder;
	long pos;
	uint64_t folder_pos;
	uint32_t i, n, ngroups;

	NBU_DPRINTF("reading section\n");

	if (nbu_read_uint32(ctx, &n) == -1)
		return -1;

	/* XXX Probably total number of items */
	NBU_DPRINTF("unknown number: %" PRIu32 "\n", n);

	if (nbu_read_uint32(ctx, &ngroups) == -1)
		return -1;

	NBU_DPRINTF("%" PRIu32 " groups\n", ngroups);

	for (i = 0; i < ngroups; i++) {
		if (nbu_seek(ctx, 4, SEEK_CUR) == -1)
			return -1;

		if (nbu_read_uint64(ctx, &folder_pos) == -1)
			return -1;

		if (nbu_tell(ctx, &pos) == -1)
			return -1;

		if ((folder = nbu_read_group_folder(ctx, folder_pos)) == NULL)
			return -1;

		/* TODO */
		nbu_free_folder(folder);

		if (nbu_seek(ctx, pos, SEEK_SET) == -1)
			return -1;
	}

	return 0;
}

static int
nbu_read_memos_section(struct nbu_ctx *ctx, uint64_t section_pos)
{
	long memo_pos, pos;
	uint32_t i, nmemos;
	uint16_t len;

	NBU_DPRINTF("reading section\n");

	if ((ctx->memos = nbu_new_item_list()) == NULL)
		return -1;

	if (nbu_read_uint32(ctx, &nmemos) == -1)
		return -1;

	NBU_DPRINTF("%" PRIu32 " memos\n", nmemos);

	if (nbu_tell(ctx, &pos) == -1)
		return -1;

	pos += 4;

	if (nbu_seek(ctx, section_pos + 48, SEEK_SET) == -1)
		return -1;

	for (i = 0; i < nmemos; i++) {
		if (nbu_seek(ctx, 4, SEEK_CUR) == -1)
			return -1;

		/* Here, length is in UTF-16 code units, not bytes */
		if (nbu_read_uint16(ctx, &len) == -1)
			return -1;

		/* Convert length to bytes */
		if (len > UINT16_MAX / 2) {
			warnx("Memo too large");
			return -1;
		}
		len *= 2;

		if (nbu_tell(ctx, &memo_pos) == -1)
			return -1;

		if (nbu_add_item(ctx->memos, memo_pos, len) == -1)
			return -1;

		if (nbu_seek(ctx, len, SEEK_CUR) == -1)
			return -1;
	}

	if (nbu_seek(ctx, pos, SEEK_SET) == -1)
		return -1;

	return 0;
}

static int
nbu_read_messages_section(struct nbu_ctx *ctx, __unused uint64_t section_pos)
{
	struct nbu_folder *folder;
	long pos;
	uint64_t folder_pos;
	uint32_t i, nfolders, nmessages;

	NBU_DPRINTF("reading section\n");

	if ((ctx->messages = nbu_new_folder_list()) == NULL)
		return -1;

	if (nbu_read_uint32(ctx, &nmessages) == -1)
		return -1;

	if (nbu_read_uint32(ctx, &nfolders) == -1)
		return -1;

	NBU_DPRINTF("%" PRIu32 " messages in %" PRIu32 " folders\n", nmessages,
	    nfolders);

	for (i = 0; i < nfolders; i++) {
		if (nbu_seek(ctx, 4, SEEK_CUR) == -1)
			return -1;

		if (nbu_read_uint64(ctx, &folder_pos) == -1)
			return -1;

		if (nbu_tell(ctx, &pos) == -1)
			return -1;

		if ((folder = nbu_read_message_folder(ctx, folder_pos)) ==
		    NULL)
			return -1;

		STAILQ_INSERT_TAIL(ctx->messages, folder, entries);

		if (nbu_seek(ctx, pos, SEEK_SET) == -1)
			return -1;
	}

	return 0;
}

static int
nbu_read_mms_section(struct nbu_ctx *ctx, __unused uint64_t section_pos)
{
	struct nbu_folder *folder;
	long pos;
	uint64_t folder_pos;
	uint32_t i, nfolders, nmessages;

	NBU_DPRINTF("reading section\n");

	if ((ctx->mmses = nbu_new_folder_list()) == NULL)
		return -1;

	if (nbu_read_uint32(ctx, &nmessages) == -1)
		return -1;

	if (nbu_read_uint32(ctx, &nfolders) == -1)
		return -1;

	NBU_DPRINTF("%" PRIu32 " messages in %" PRIu32 " folders\n", nmessages,
	    nfolders);

	for (i = 0; i < nfolders; i++) {
		if (nbu_seek(ctx, 4, SEEK_CUR) == -1)
			return -1;

		if (nbu_read_uint64(ctx, &folder_pos) == -1)
			return -1;

		if (nbu_tell(ctx, &pos) == -1)
			return -1;

		if ((folder = nbu_read_mms_folder(ctx, folder_pos)) == NULL)
			return -1;

		STAILQ_INSERT_TAIL(ctx->mmses, folder, entries);

		if (nbu_seek(ctx, pos, SEEK_SET) == -1)
			return -1;
	}

	return 0;
}

static int
nbu_read_section(struct nbu_ctx *ctx, uint8_t guid[NBU_GUID_LEN], uint64_t pos)
{
	size_t i;

	for (i = 0; i < nitems(nbu_sections); i++) {
		if (memcmp(guid, nbu_sections[i].guid, NBU_GUID_LEN) == 0)
			return nbu_sections[i].read(ctx, pos);
	}

	warnx("Unsupported backup section");
	return -1;
}

static int
nbu_read_sections(struct nbu_ctx *ctx)
{
	uint64_t pos;
	uint32_t i, nsections;
	uint8_t guid[NBU_GUID_LEN];

	if (nbu_read_uint32(ctx, &nsections) == -1)
		return -1;

	NBU_DPRINTF("backup contains %" PRIu32 " sections\n", nsections);

	for (i = 0; i < nsections; i++) {
		if (nbu_read(ctx, guid, sizeof guid) == -1)
			return -1;

		if (nbu_read_uint64(ctx, &pos) == -1)
			return -1;

		/* Skip section length */
		if (nbu_seek(ctx, 8, SEEK_CUR) == -1)
			return -1;

		NBU_DPRINTF("section %" PRIu32 ": guid %s\n",
		    i + 1, nbu_guid_to_string(guid));

		if (nbu_read_section(ctx, guid, pos) == -1)
			return -1;
	}

	return 0;
}

int
nbu_open(struct nbu_ctx **ctxp, const char *path)
{
	struct nbu_ctx *ctx;
	uint64_t pos;
	int ret;

	ret = -1;

	if ((ctx = calloc(1, sizeof *ctx)) == NULL) {
		warn(NULL);
		goto out;
	}

	if ((ctx->fp = fopen(path, "r")) == NULL) {
		warn("fopen: %s", path);
		goto out;
	}

	if (nbu_seek(ctx, 20, SEEK_SET) == -1)
		goto out;

	if (nbu_read_uint64(ctx, &pos) == -1)
		goto out;

	if (nbu_seek(ctx, pos + 20, SEEK_SET) == -1)
		goto out;

	if (nbu_read_file_time(ctx, &ctx->backup_time) == -1)
		goto out;

	if ((ctx->phone_imei = nbu_read_utf16(ctx)) == NULL)
		goto out;

	if ((ctx->phone_model = nbu_read_utf16(ctx)) == NULL)
		goto out;

	if ((ctx->phone_name = nbu_read_utf16(ctx)) == NULL)
		goto out;

	if ((ctx->phone_firmware = nbu_read_utf16(ctx)) == NULL)
		goto out;

	if ((ctx->phone_language = nbu_read_utf16(ctx)) == NULL)
		goto out;

#ifdef NBU_DEBUG
	nbu_print_phone_info(ctx);
#endif

	if (nbu_seek(ctx, 20, SEEK_CUR) == -1)
		goto out;

	if (nbu_read_sections(ctx) == -1)
		goto out;

	ret = 0;

out:
	*ctxp = ctx;
	return ret;
}

void
nbu_close(struct nbu_ctx *ctx)
{
	if (ctx == NULL)
		return;

	if (ctx->fp != NULL)
		fclose(ctx->fp);

	free(ctx->phone_imei);
	free(ctx->phone_model);
	free(ctx->phone_name);
	free(ctx->phone_firmware);
	free(ctx->phone_language);
	nbu_free_folder_list(ctx->bookmarks);
	nbu_free_folder_list(ctx->messages);
	nbu_free_folder_list(ctx->mmses);
	nbu_free_item_list(ctx->calendar);
	nbu_free_item_list(ctx->contacts);
	nbu_free_item_list(ctx->memos);
	free(ctx);
}

int
nbu_export(struct nbu_ctx *ctx, const char *path)
{
	int dfd, ret;

	if (mkdir(path, 0777) == -1 && errno != EEXIST) {
		warn("mkdir: %s", path);
		return -1;
	}

	if ((dfd = open(path, O_RDONLY | O_DIRECTORY)) == -1) {
		warn("open: %s", path);
		return -1;
	}

	ret = 0;

	if (nbu_export_calendar(ctx, dfd) == -1)
		ret = -1;

	if (nbu_export_contacts(ctx, dfd) == -1)
		ret = -1;

	if (nbu_export_memos(ctx, dfd) == -1)
		ret = -1;

	if (nbu_export_messages(ctx, dfd) == -1)
		ret = -1;

	close(dfd);
	return ret;
}
