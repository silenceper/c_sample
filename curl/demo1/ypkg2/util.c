/* Libypk utility functions
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2012.3.6
 */

#include "util.h"
#include "sha1.h"

char *util_get_config(char *config_file, char *keyword)
{
    int             match, len;
    FILE            *fp;
    char            *pos, *result;
    char            line[MAXCFGLINE], keybuf[MAXKWLEN], valbuf[MAXCFGLINE];

    if( ( fp = fopen( config_file, "r" ) ) == NULL )
        return NULL;
    match = 0;
    while( fgets( line, MAXCFGLINE - 1, fp ) != NULL ) 
    {
        if( line[0] == '#' )
            continue;

        pos = strchr( line, '=' );
        if( !pos )
            continue;

        memset( keybuf, 0, MAXKWLEN );
        len = (int)(pos - line);
        strncpy( keybuf, line, len < MAXKWLEN - 1 ? len : MAXKWLEN - 1 );
        util_rtrim( keybuf, 0 );

        memset( valbuf, 0, MAXCFGLINE );
        pos++;
        while( pos[0] == ' ' || pos[0] == '\'' || pos[0] == '\"' )
        {
            pos++;
        }
        strncpy( valbuf, pos, MAXCFGLINE );
        util_rtrim( valbuf, '\"' );
        util_rtrim( valbuf, '\'' );
        util_rtrim( valbuf, 0 );

        if( strcmp(keyword, keybuf) == 0 )
        {
            match = 1;
            break;
        }
    }
    fclose( fp );
    if( match != 0 )
    {
        len = strlen( valbuf );
        result = malloc( len + 1 );
        strncpy( result, valbuf, len );
        result[len] = 0;
        return result;
    }
    else 
        return NULL;
}


char *util_rtrim( char *str, char c )
{
    int i;

    for(i = strlen(str)-1;  i >= 0; i--)
    {
        if( c && str[i] == c )
        {
            str[i] = '\0';
        }
        else if(str[i] == '\r' || str[i] == '\n' || str[i] == ' ' || str[i] == '\t' || str[i] == '\x0B' )
        {
            str[i] = '\0';
        }
        else
        {
            break;
        }
    }
    return str;
}

int util_ends_with( char *str, char *suffix )
{
    size_t len_str, len_suffix;

    if( !str || !suffix )
        return 0;

    len_str = strlen( str );
    len_suffix = strlen( suffix );

    if( len_suffix > len_str )
        return 0;

    return strncmp( str + len_str - len_suffix, suffix, len_suffix ) == 0;
}

char *util_mem_gets( char *mem )
{
    int             len;
    char            *pos, *str;
    static int      cur_len;
    static char     *cur_pos;

    if( mem )
    {
        cur_pos = mem;
        cur_len = strlen( cur_pos );
        if( cur_len < 1 )
        {
            cur_pos = NULL;
            cur_len = 0;
            return NULL;
        }
    }
    else if( !cur_pos || cur_len < 1 )
    {
        return NULL;
    }

    pos = strchr( cur_pos, '\n' );
    if( !pos )
        len = cur_len;
    else
        len = pos - cur_pos;

    str = malloc( len + 1 );
    if( !str )
        return NULL;

    strncpy( str, cur_pos, len );
    str[len] = '\0';
    cur_pos = pos + 1;
    cur_len -= len + 1;

    return str;
}

char *util_chr_replace( char *str, char chr_s, char chr_d )
{
    char *p;

    if( !str )
        return NULL;

    while( ( p = strchr( str, chr_s ) ) )
    {
        *p = chr_d;
    }
    return str;
}

char *util_null2empty( char *str )
{
    return str ? str : "";
}

char *util_strcat( char *first, ... )
{
    va_list ap;
    char    *arg, *result;
    int     len;

    if( !first )
        return NULL;

    len = strlen( first );
    va_start( ap, first );
    while( ( arg = va_arg( ap, char * ) ) != NULL )
    {
        len += strlen( arg );
    }
    va_end(ap);

    result = malloc( len + 1 );
    if( !result )
        return NULL;
    memset( result, '\0', len + 1);
    strncpy( result, first, strlen( first ) );

    va_start( ap, first );
    while( ( arg = va_arg( ap, char * ) ) != NULL )
    {
        strncat( result, arg, strlen( arg ) );
    }
    va_end(ap);

    result[len] = '\0';

    return result;
}

char *util_strcat2( char *dest, int size, char *first, ...)
{
    va_list ap;
    char    *arg, *result;
    int     len;

    len = strlen( first );
    va_start( ap, first );
    while( ( arg = va_arg( ap, char * ) ) != NULL )
    {
        len += strlen( arg );
    }
    va_end(ap);

    if( len + 1 > size )
        return NULL;

    result = dest;
    memset( result, '\0', size );
    strncpy( result, first, strlen( first ) );

    va_start( ap, first );
    while( ( arg = va_arg( ap, char * ) ) != NULL )
    {
        strncat( result, arg, strlen( arg ) );
    }
    va_end(ap);

    result[len] = '\0';

    return result;
}

char *util_int_to_str( int i )
{
    char    *result;

    result = malloc( 11 );
    if( !result )
        return NULL;
    snprintf( result, 11, "%d", i );
    result[10] = '\0';

    return result;
}

int util_log( char *log, char *msg )
{
    FILE *fp;

    fp = fopen( log, "a" );
    if( !fp )
        return -1;

    fputs( msg,  fp );

    fclose( fp );

    return 0;
}


int util_mkdir( char *dir )
{
    char *tmp, *parent_dir;

    if( !access( dir, F_OK ) )
    {
        return 0;
    }

    tmp = strdup( dir );
    if( !tmp )
        return -1;

    parent_dir = dirname( tmp );

    if( !access( parent_dir, F_OK ) )
    {
        if( !access( parent_dir, W_OK | X_OK ) )
        {
            if( mkdir( dir, 0755 ) )
            {
                free( tmp );
                return -1;
            }
        }
        else
        {
            free( tmp );
            return -1;
        }
    }
    else
    {
        if( !util_mkdir( parent_dir ) )
        {
            if( mkdir( dir, 0755 ) )
            {
                free( tmp );
                return -1;
            }
        }
        else
        {
            free( tmp );
            return -1;
        }
    }

    free( tmp );
    return 0;
}

int util_remove_dir( char *dir_path )
{
    int             ret;
    char            *file_path;
    DIR             *dir;
    struct dirent   *entry;
    struct stat     statbuf;


    dir = opendir( dir_path );
    if( !dir )
        return -1;

    while( ( entry = readdir( dir ) ) )
    {
        if( !strcmp( entry->d_name, "." ) || !strcmp( entry->d_name, ".." ) )
        {
            continue;
        }

        file_path = util_strcat( dir_path, "/", entry->d_name, NULL );
        if( !stat( file_path, &statbuf ) )
        {
            if( S_ISDIR( statbuf.st_mode ) )
            {
                ret = util_remove_dir( file_path );
            }
            else
            {
                ret = unlink( file_path );
            }
        }
        free( file_path );
        if( ret )
        {
            closedir( dir );
            return -1;
        }
    }
    closedir( dir );

    return rmdir( dir_path );
}


int util_remove_files( char *dir_path, char *suffix )
{
    int             ret;
    char            *file_path;
    DIR             *dir;
    struct dirent   *entry;
    struct stat     statbuf;


    dir = opendir( dir_path );
    if( !dir )
        return -1;

    while( (entry = readdir( dir )) )
    {
        if( !strcmp( entry->d_name, "." ) || !strcmp( entry->d_name, ".." ) )
        {
            continue;
        }

        file_path = util_strcat( dir_path, "/", entry->d_name, NULL );

        if( suffix && !util_ends_with( file_path, suffix ) )
        {
            free( file_path );
            continue;
        }

        if( !stat( file_path, &statbuf ) )
        {
            if( !S_ISDIR( statbuf.st_mode ) )
            {
                ret = unlink( file_path );
            }
        }
        free( file_path );
        if( ret )
        {
            closedir( dir );
            return -1;
        }
    }
    closedir( dir );

    return 0;
}

int util_copy_file( char *src, char *dest )
{
    size_t  cnt_r, cnt_w;
    FILE    *fp_r, *fp_w;
    char    buf[4096];

    fp_r = fopen( src, "r" );
    if( !fp_r )
        return -1;

    fp_w = fopen( dest, "w" );
    if( !fp_w )
    {
        fclose( fp_r );
        return -1;
    }

    while( (cnt_r = fread( buf, 1, 4096, fp_r )) )
    {
        if( cnt_r < 4096 )
        {
            if( ferror( fp_r ) )
            {
                fclose( fp_r );
                fclose( fp_w );
                return -1;
            }
        }

        cnt_w = fwrite( buf, 1, cnt_r, fp_w );
        if( cnt_w < cnt_r )
        {
            fclose( fp_r );
            fclose( fp_w );
            return -1;
        }
    }

    fclose( fp_w );
    fclose( fp_r );
    return 0;
}

int util_file_size( char *file )
{
    struct stat     statbuf;

    if( !file )
        return -1;

    if( !stat( file, &statbuf ) )
    {
        return statbuf.st_size;
    }
    return -1;
}


char *util_time_to_str( time_t time )
{
    char        *result;
    struct tm   *tmp;

    tmp = localtime( &time );
    if( !tmp )
        return NULL;

    result = malloc( 20 );
    if( !result )
        return NULL;
    memset( result, '\0', 20 );
    if( strftime( result, 20, "%Y-%m-%d,%H:%M:%S", tmp) == 0 ) 
    {
        free( result );
        return NULL;
    }

    return result;
}


char *util_sha1( char *file )
{
    unsigned char   c;
    char            *result;
    FILE            *fp;
    SHA1Context     sha;

    if ( !( fp = fopen( file,"r" ) ) )
    {
        return NULL;
    }


    SHA1Reset( &sha );

    c = (unsigned char)fgetc( fp );
    while( !feof( fp ) )
    {
        SHA1Input( &sha, &c, 1 );
        c = fgetc( fp );
    }

    fclose(fp);

    if( !SHA1Result( &sha ) )
    {
        return NULL;
    }
    else
    {
        result = malloc( 41 );
        if( !result )
            return NULL;

        snprintf( result, 41, "%08x%08x%08x%08x%08x", sha.Message_Digest[0], sha.Message_Digest[1], sha.Message_Digest[2], sha.Message_Digest[3], sha.Message_Digest[4] );
    }

    return result;
}

char *util_str_sha1( char *str )
{
    char            *result;
    SHA1Context     sha;

    if( !str )
    {
        return NULL;
    }

    SHA1Reset( &sha );

    SHA1Input( &sha, (unsigned char *)str, strlen( str ) );

    if( !SHA1Result( &sha ) )
    {
        return NULL;
    }
    else
    {
        result = malloc( 41 );
        if( !result )
            return NULL;

        snprintf( result, 41, "%08x%08x%08x%08x%08x", sha.Message_Digest[0], sha.Message_Digest[1], sha.Message_Digest[2], sha.Message_Digest[3], sha.Message_Digest[4] );
    }

    return result;
}
