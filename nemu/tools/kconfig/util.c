// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002-2005 Roman Zippel <zippel@linux-m68k.org>
 * Copyright (C) 2002-2005 Sam Ravnborg <sam@ravnborg.org>
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "lkc.h"

/* file already present in list? If not add it */
struct file *file_lookup(const char *name)
{
	struct file *file;

	for (file = file_list; file; file = file->next) {
		if (!strcmp(name, file->name)) {
			return file;
		}
	}

	file = xmalloc(sizeof(*file));
	memset(file, 0, sizeof(*file));
	file->name = xstrdup(name);
	file->next = file_list;
	file_list = file;
	return file;
}

/* Allocate initial growable string */
struct gstr str_new(void)
{
	struct gstr gs;
	gs.s = xmalloc(sizeof(char) * 64);
	gs.len = 64;
	gs.max_width = 0;
	strcpy(gs.s, "\0");
	return gs;
}

/* Free storage for growable string */
void str_free(struct gstr *gs)
{
	if (gs->s)
		free(gs->s);
	gs->s = NULL;
	gs->len = 0;
}

/* Append to growable string */
void str_append(struct gstr *gs, const char *s)
{
	size_t l;
	if (s) {
		l = strlen(gs->s) + strlen(s) + 1;
		if (l > gs->len) {
			gs->s = xrealloc(gs->s, l);
			gs->len = l;
		}
		strcat(gs->s, s);
	}
}

/* Append printf formatted string to growable string */
void str_printf(struct gstr *gs, const char *fmt, ...)
{
	va_list ap;
	char s[10000]; /* big enough... */
	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	str_append(gs, s);
	va_end(ap);
}

/* Retrieve value of growable string */
const char *str_get(struct gstr *gs)
{
	return gs->s;
}

/**
 * Allocates memory of the specified size using `malloc`. If the allocation
 * fails, the function prints an error message to `stderr` and terminates the
 * program with an exit code of 1.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory. This function never returns NULL;
 *         if `malloc` fails, the program exits.
 */
void *xmalloc(size_t size)
{
	void *p = malloc(size);
	if (p)
		return p;
	fprintf(stderr, "Out of memory.\n");
	exit(1);
}

/**
 * Allocates memory for an array of `nmemb` elements of `size` bytes each and initializes all bytes to zero.
 * This function is a wrapper around the standard `calloc` function, with added error handling.
 * If the memory allocation fails, an error message is printed to `stderr` and the program exits with a status code of 1.
 *
 * @param nmemb The number of elements to allocate.
 * @param size The size in bytes of each element.
 * @return A pointer to the allocated memory, which is guaranteed to be zero-initialized.
 *         If the allocation fails, the function does not return and the program terminates.
 */
void *xcalloc(size_t nmemb, size_t size)
{
	void *p = calloc(nmemb, size);
	if (p)
		return p;
	fprintf(stderr, "Out of memory.\n");
	exit(1);
}

/**
 * Reallocates a block of memory and handles allocation failures.
 *
 * This function reallocates the memory block pointed to by `p` to a new size `size`.
 * If the reallocation is successful, the function returns a pointer to the newly
 * allocated memory block. If the reallocation fails, the function prints an error
 * message to `stderr` and terminates the program with an exit code of 1.
 *
 * @param p    Pointer to the previously allocated memory block, or NULL if a new
 *             block is to be allocated.
 * @param size The new size of the memory block, in bytes.
 *
 * @return A pointer to the reallocated memory block. If the reallocation fails,
 *         the function does not return and the program terminates.
 */
void *xrealloc(void *p, size_t size)
{
	p = realloc(p, size);
	if (p)
		return p;
	fprintf(stderr, "Out of memory.\n");
	exit(1);
}

/**
 * Duplicates a given string using dynamic memory allocation.
 *
 * This function creates a duplicate of the input string `s` by allocating memory
 * for the new string using `strdup`. If the memory allocation is successful, the
 * function returns a pointer to the newly allocated string. If the allocation fails,
 * the function prints an error message to `stderr` and terminates the program with
 * an exit code of 1.
 *
 * @param s The input string to be duplicated. Must be a null-terminated C string.
 * @return A pointer to the newly allocated duplicate string. The caller is responsible
 *         for freeing this memory using `free`.
 * @note The function will terminate the program if memory allocation fails.
 */
char *xstrdup(const char *s)
{
	char *p;

	p = strdup(s);
	if (p)
		return p;
	fprintf(stderr, "Out of memory.\n");
	exit(1);
}

/**
 * Duplicates a string up to a specified number of characters.
 * 
 * This function creates a duplicate of the string `s`, copying up to `n` characters.
 * If `n` is greater than the length of `s`, the entire string is duplicated.
 * The duplicated string is allocated using `strndup`, and the caller is responsible
 * for freeing the allocated memory.
 * 
 * If memory allocation fails, the function prints an error message to `stderr`
 * and terminates the program with an exit code of 1.
 * 
 * @param s The string to duplicate. Must not be NULL.
 * @param n The maximum number of characters to duplicate.
 * @return A pointer to the newly allocated duplicated string. This pointer must
 *         be freed by the caller. If memory allocation fails, the program exits.
 */
char *xstrndup(const char *s, size_t n)
{
	char *p;

	p = strndup(s, n);
	if (p)
		return p;
	fprintf(stderr, "Out of memory.\n");
	exit(1);
}
