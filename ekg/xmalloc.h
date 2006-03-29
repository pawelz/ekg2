/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *		       2004 Piotr Kupisiewicz <deli@rzepaknet.us>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __EKG_XMALLOC_H
#define __EKG_XMALLOC_H

#include <sys/types.h>
#include <stddef.h>
#include <stdarg.h>
#include "char.h"
#include "stuff.h"

void ekg_oom_handler();

void *xcalloc(size_t nmemb, size_t size);
void *xmalloc(size_t size);
void xfree(void *ptr);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
CHAR_T *xwcsdup(const CHAR_T *s);
size_t xstrnlen(const char *s, size_t n);
char *xstrndup(const char *s, size_t n);
void *xmemdup(void *ptr, size_t size);

int xstrcasecmp(const char *s1, const char *s2);
int xwcscasecmp(const CHAR_T *s1, const CHAR_T *s2);
char *xstrcat(char *dest, const char *src);
char *xstrchr(const char *s, int c);
int xstrcmp(const char *s1, const char *s2);
int xwcscmp(const CHAR_T *s1, const CHAR_T *s2);
int xstrcoll(const char *s1, const char *s2);
char *xstrcpy(char *dest, const char *src);
size_t xstrcspn(const char *s, const char *reject);
char *xstrfry(char *string);
size_t xstrlen(const char *s);
size_t xwcslen(const CHAR_T *s);
int xstrncasecmp_pl(const char *s1, const char *s2, size_t n);
char *xstrncat(char *dest, const char *src, size_t n);
int xstrncmp(const char *s1, const char *s2, size_t n);
char *xstrncpy(char *dest, const char *src, size_t n);
int xstrncasecmp(const char *s1, const char *s2, size_t n);
int xwcsncasecmp(const CHAR_T *s1, const CHAR_T *s2, size_t n);
char *xstrpbrk(const char *s, const char *accept);
char *xstrrchr(const char *s, int c);
/*
char *xstrsep(char **stringp, const char *delim);
*/
size_t xstrspn(const char *s, const char *accept);
char *xstrstr(const char *haystack, const char *needle);
char *xstrcasestr(const char *haystack, const char *needle);
char *xstrtok(char *s, const char *delim);
size_t xstrxfrm(char *dest, const char *src, size_t n);
char *xindex(const char *s, int c);
char *xrindex(const char *s, int c);

char *vsaprintf(const char *format, va_list ap);
CHAR_T *vwcssaprintf(const CHAR_T *format, va_list ap);

#endif /* __EKG_XMALLOC_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
