/* Libypk regular expression functions
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2012.3.6
 */

#ifndef PREG_H
#define PREG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>

#define OVECCOUNT 30    /* should be a multiple of 3 */
    
typedef struct {
    pcre *re;
    char *subject;
    int offset;
    int ovector[OVECCOUNT];
    int stringcount;
} PREGInfo;

int preg_match(PREGInfo *piptr, char *pattern, char *subject, int options, int first);

int preg_result(PREGInfo *piptr, int number, char *buf, int buf_size);

int preg_result2(PREGInfo *piptr, char *name, char *buf, int buf_size);

void preg_free(PREGInfo *piptr);

char *preg_replace(char *pattern, char *replace, char *subject, int options, int once);

#endif /* !PREG_H */
