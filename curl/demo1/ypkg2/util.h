/* Libypk utility functions
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2012.3.6
 */
#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>



#define MAXFMTLEN 16 
#define MAXKWLEN 32 
#define MAXCFGLINE 128



/*
 * config
 */
char *util_get_config(char *config_file, char *keyword);

/*
 * string
 */
char    *util_rtrim( char *str, char c );
char    *util_mem_gets( char *mem );
char    *util_chr_replace( char *str, char chr_s, char chr_d );
char    *util_null2empty( char *str );
char    *util_strcat(char *first, ...);
char    *util_strcat2( char *dest, int size, char *first, ...);
char    *util_int_to_str( int i );
char    *util_time_to_str( time_t time );
int     util_ends_with( char *str, char *suffix );


/*
 * log
 */
int util_log( char *log, char *msg );


/*
 * file & dir
 */
int util_mkdir( char *dir );
int util_remove_dir( char *dir_path );
int util_remove_files( char *dir_path, char *suffix );
int util_copy_file( char *src, char *dest );
int util_file_size( char *file );

/*
 * hash
 */
char *util_sha1( char *file );
char *util_str_sha1( char *str );

#endif /* !UTIL_H */
