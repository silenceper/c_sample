/* Archive functions
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2012.4.12
 */
#include "archive.h"

/*
 * file to file
 */
int archive_extract_file( char *arch_file, const char *src, char *dest )
{
    int                     flags;     
    const char              *filename;
    struct archive          *arch_r = NULL, *arch_w = NULL;
    struct archive_entry    *entry;

    if( !arch_file || !src || !dest )
        return -1;

    arch_r = archive_read_new();
    archive_read_support_format_all( arch_r );
    archive_read_support_compression_all( arch_r );

    if( archive_read_open_filename( arch_r, arch_file, 10240 ) != ARCHIVE_OK )
        goto errout;

    while( archive_read_next_header( arch_r, &entry ) == ARCHIVE_OK ) 
    {
        filename = archive_entry_pathname( entry );
		if( fnmatch( src, filename, FNM_PATHNAME | FNM_PERIOD ) )
		{
			if( archive_read_data_skip( arch_r ) != ARCHIVE_OK )
            {
                goto errout;
            }
		}
        else
        {
#ifdef DEBUG
            printf("extract:%s\n", filename );
#endif

            flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS;
            arch_w = archive_write_disk_new();
            archive_write_disk_set_options( arch_w, flags );
            archive_write_disk_set_standard_lookup( arch_w );
            archive_entry_set_pathname( entry, dest );

            if( archive_read_extract2( arch_r, entry, arch_w ) != ARCHIVE_OK ) 
                goto errout;

            archive_write_finish( arch_w );
        }
    }

    archive_read_finish( arch_r );
    return 0;

errout:
#ifdef DEBUG
    fprintf( stderr, "%s\n", archive_error_string( arch_r ) );
#endif
    if( arch_r )
        archive_read_finish( arch_r );
    if( arch_w )
        archive_write_finish( arch_w );
    return -1;
}

/*
 * file to memory
 */
int archive_extract_file2( char *arch_file, const char *src, void **dest_buff, size_t *dest_len )
{
    const char              *filename;
    struct archive          *arch_r = NULL;
    struct archive_entry    *entry;

    if( !arch_file || !src )
        return -1;

    arch_r = archive_read_new();
    archive_read_support_format_all( arch_r );
    archive_read_support_compression_all( arch_r );

    if( archive_read_open_filename( arch_r, arch_file, 10240 ) != ARCHIVE_OK )
        goto errout;

    while( archive_read_next_header( arch_r, &entry ) == ARCHIVE_OK ) 
    {
        filename = archive_entry_pathname( entry );
		if( fnmatch( src, filename, FNM_PATHNAME | FNM_PERIOD ) )
		{
			if( archive_read_data_skip( arch_r ) != ARCHIVE_OK )
            {
                goto errout;
            }
		}
        else
        {
#ifdef DEBUG
            printf("extract:%s\n", filename );
#endif

            *dest_len = archive_entry_size( entry ); 
            if( *dest_len > 0 )
            {
                *dest_buff = malloc( *dest_len + 10 );
                memset( *dest_buff, 0, *dest_len + 10 );
            }

            if( archive_read_data( arch_r, *dest_buff, *dest_len) < 0 ) 
                goto errout;
        }
    }

    archive_read_finish( arch_r );
    return 0;

errout:
#ifdef DEBUG
    fprintf( stderr, "errno:%d, error:%s\n", archive_errno( arch_r ), archive_error_string( arch_r ) );
#endif
    if( arch_r )
        archive_read_finish( arch_r );
    return -1;
}

/*
 * memory to file
 */
int archive_extract_file3( void *arch_buff, size_t arch_size, const char *src, char *dest )
{
    int                     flags;     
    const char              *filename;
    struct archive          *arch_r = NULL, *arch_w = NULL;
    struct archive_entry    *entry;

    if( !src || !dest )
        return -1;

    arch_r = archive_read_new();
    archive_read_support_format_all( arch_r );
    archive_read_support_compression_all( arch_r );

    if( archive_read_open_memory( arch_r, arch_buff, arch_size ) != ARCHIVE_OK )
        goto errout;

    while( archive_read_next_header( arch_r, &entry ) == ARCHIVE_OK ) 
    {
        filename = archive_entry_pathname( entry );
		if( fnmatch( src, filename, FNM_PATHNAME | FNM_PERIOD ) )
		{
			if( archive_read_data_skip( arch_r ) != ARCHIVE_OK )
            {
                goto errout;
            }
		}
        else
        {
#ifdef DEBUG
            printf("extract:%s\n", filename );
#endif

            flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS;
            arch_w = archive_write_disk_new();
            archive_write_disk_set_options( arch_w, flags );
            archive_write_disk_set_standard_lookup( arch_w );
            archive_entry_set_pathname( entry, dest );

            if( archive_read_extract2( arch_r, entry, arch_w ) != ARCHIVE_OK ) 
                goto errout;

            archive_write_finish( arch_w );
        }
    }

    archive_read_finish( arch_r );
    return 0;

errout:
#ifdef DEBUG
    fprintf( stderr, "%s\n", archive_error_string( arch_r ) );
#endif
    if( arch_r )
        archive_read_finish( arch_r );
    if( arch_w )
        archive_write_finish( arch_w );
    return -1;
}

/*
 * memory to memory
 */
int archive_extract_file4( void *arch_buff, size_t arch_size, const char *src,  void **dest_buff, size_t *dest_len )
{    
    const char              *filename;
    struct archive          *arch_r = NULL;
    struct archive_entry    *entry;

    if( !src )
        return -1;

    arch_r = archive_read_new();
    archive_read_support_format_all( arch_r );
    archive_read_support_compression_all( arch_r );

    if( archive_read_open_memory( arch_r, arch_buff, arch_size ) != ARCHIVE_OK )
        goto errout;

    while( archive_read_next_header( arch_r, &entry ) == ARCHIVE_OK ) 
    {
        filename = archive_entry_pathname( entry );
		if( fnmatch( src, filename, FNM_PATHNAME | FNM_PERIOD ) )
		{
			if( archive_read_data_skip( arch_r ) != ARCHIVE_OK )
            {
                goto errout;
            }
		}
        else
        {
#ifdef DEBUG
            printf("extract:%s\n", filename );
#endif

            *dest_len = archive_entry_size( entry ); 
            if( *dest_len > 0 )
            {
                *dest_buff = malloc( *dest_len + 1 );
                memset( *dest_buff, 0, *dest_len + 1 );
            }

            if( archive_read_data( arch_r, *dest_buff, *dest_len) < 0 ) 
                goto errout;
        }
    }

    archive_read_finish( arch_r );
    return 0;

errout:
#ifdef DEBUG
    fprintf( stderr, "%s\n", archive_error_string( arch_r ) );
#endif
    if( arch_r )
        archive_read_finish( arch_r );
    return -1;

}

int archive_extract_all( char *arch_file, char *dest_dir, char *suffix )
{
    int                     ret, flags;     
    char                    *pwd = NULL, *filename = NULL, *filename_new = NULL, *hardlink = NULL;
    struct archive          *arch_r = NULL, *arch_w = NULL;
    struct archive_entry    *entry = NULL;


    if( !arch_file )
        return -1;

    arch_r = archive_read_new();
    archive_read_support_format_all( arch_r );
    archive_read_support_compression_all( arch_r );

    if( archive_read_open_filename( arch_r, arch_file, 10240 ) != ARCHIVE_OK )
        goto errout;

    if( dest_dir )
    {
        if( util_mkdir( dest_dir ) == -1 )
        {
            if( errno == EEXIST )
            {
                if( access( dest_dir, R_OK | W_OK | X_OK ) == -1 )
                {
                    goto errout;
                }

            }
            else
            {
                goto errout;
            }
        }
        pwd = getcwd( NULL, 0 );
        chdir( dest_dir );
    }

    flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_OWNER;
    arch_w = archive_write_disk_new();
    archive_write_disk_set_options( arch_w, flags );
    archive_write_disk_set_standard_lookup( arch_w );

    while( archive_read_next_header( arch_r, &entry ) == ARCHIVE_OK ) 
    {
        if( suffix )
            filename = (char *)archive_entry_pathname( entry );
        
#ifdef DEBUG
        if( !filename )
            filename = (char *)archive_entry_pathname( entry );

        printf("extract:%s\n", filename );
#endif

        if( suffix && archive_entry_filetype( entry ) != AE_IFDIR  )
        {
            filename_new = util_strcat( filename, suffix, NULL );
            archive_entry_set_pathname( entry, filename_new );
            free( filename_new );

            if( archive_entry_nlink( entry ) > 0 )
            {
                hardlink = (char *)archive_entry_hardlink( entry );
                if( hardlink )
                {
                    filename_new = util_strcat( hardlink, suffix, NULL );
                    archive_entry_set_hardlink( entry, filename_new );
                    free( filename_new );
                }
            }

        }

        ret = archive_read_extract2( arch_r, entry, arch_w );
        if( ret != ARCHIVE_OK && ret != ARCHIVE_WARN ) 
        {
            goto errout;
        }


#ifdef DEBUG
        if( ret != ARCHIVE_OK)
        {
            printf("ret:%d, file:%s, link:%d, size:%d, read_err:%s, write_err:%s\n", ret, filename, archive_entry_nlink(entry), archive_entry_size(entry), archive_error_string(arch_r), archive_error_string(arch_w));
        }
#endif
    }


    archive_read_finish( arch_r );
    archive_write_finish( arch_w );
    if( pwd )
    {
        chdir( pwd );
        free( pwd );
    }
    return 0;

errout:

    if( arch_r )
        archive_read_finish( arch_r );

    if( arch_w )
        archive_write_finish( arch_w );

    if( pwd )
    {
        chdir( pwd );
        free( pwd );
    }
    return -1;
}

int archive_create( char *arch_file, int compress, int format, char *src_dir, char **exclude )
{
    int                     fd, len, ret, skip;
    char                    *pwd, **files;
    DIR                     *dir;
    struct dirent           *dir_entry;
    struct archive          *arch_w = NULL, *arch_r = NULL;
    struct archive_entry    *arch_entry = NULL;
    char                    buf[8192];

    if( !arch_file || !src_dir )
        return -1;


    dir = opendir( src_dir );
    if( !dir )
        return -1;


    arch_w = archive_write_new();

    switch( compress )
    {
        case 'j': //bz2
            //archive_write_add_filter_bzip2( arch_w ); //libarchive 3
            archive_write_set_compression_bzip2( arch_w );
            break;

        case 'J': //xz
            //archive_write_add_filter_xz( arch_w );
            archive_write_set_compression_xz( arch_w );
            break;

        case 'z': //gzip
            //archive_write_add_filter_gzip( arch_w );
            archive_write_set_compression_gzip( arch_w );
            break;
    }

    switch( format )
    {
        case 'c': //cpio
            archive_write_set_format_cpio( arch_w );
            break;

        case 't': //tar
            archive_write_set_format_ustar( arch_w );
            break;

        default: //tar
            archive_write_set_format_ustar( arch_w );
            break;
    }



    ret = archive_write_open_filename( arch_w, arch_file );
    if( ret != ARCHIVE_OK )
    {
        archive_write_finish( arch_w );
        return -1;
    }

    pwd = getcwd( NULL, 0 );
    chdir( src_dir );

    while( (dir_entry = readdir( dir )) )
    {
        if( !strcmp( dir_entry->d_name, "." ) || !strcmp( dir_entry->d_name, ".." ) )
        {
            continue;
        }

        if( exclude )
        {
            files = exclude;
            skip = 0;
            while( *files )
            {
                if( !strcmp( *files, dir_entry->d_name ) )
                {
                    skip = 1;
                    break;
                }

                files++;
            }

            if( skip )
                continue;
        }

        arch_r = archive_read_disk_new();
        ret = archive_read_disk_open( arch_r, dir_entry->d_name );
        if( ret != ARCHIVE_OK )
        {
#ifdef DEBUG
            printf( "%s\n", archive_error_string( arch_r ) );
#endif
            archive_write_finish( arch_w );
            return -1;
        }

        for( ; ; )
        {
            arch_entry = archive_entry_new();
            ret = archive_read_next_header2( arch_r, arch_entry );
            if( ret == ARCHIVE_EOF )
                break;
            
            if( ret != ARCHIVE_OK )
            {
#ifdef DEBUG
                printf( "%s\n", archive_error_string( arch_r ) );
#endif
                archive_entry_free( arch_entry );
                archive_write_finish( arch_r );
                archive_write_finish( arch_w );
                return -1;
            }

#ifdef DEBUG
            printf( "%s\n", archive_entry_pathname( arch_entry ) );
#endif

            archive_read_disk_descend( arch_r );

            ret = archive_write_header( arch_w, arch_entry );
            if( ret < ARCHIVE_OK )
            {
#ifdef DEBUG
                printf( "%s\n", archive_error_string( arch_w ) );
#endif
                archive_entry_free( arch_entry );
                archive_write_finish( arch_r );
                archive_write_finish( arch_w );
                return -1;
            }

            if( ( fd = open( archive_entry_sourcepath( arch_entry ), O_RDONLY ) ) != -1 )
            {
                len = read( fd, buf, 8192 );
                while( len > 0 )
                {
                    archive_write_data( arch_w, buf, len );
                    len = read( fd, buf, 8192 );
                }
                close( fd );
            }
            archive_entry_free( arch_entry );
        }
        /* libarchive 3
        archive_read_close( arch_r );
        archive_read_free( arch_r );
        */
        archive_read_finish( arch_r );

    }



    if( arch_w )
    {
        /* libarchive 3
        archive_write_close( arch_r );
        archive_write_free( arch_r );
        */
        archive_write_finish( arch_w );
    }

    if( pwd )
    {
        chdir( pwd );
        free( pwd );
    }
    return 0;
}

int archive_create2( char *arch_file, int compress, int format, char **files )
{
    int                     fd, len, ret;
    struct archive          *arch_w = NULL, *arch_r;
    struct archive_entry    *arch_entry = NULL;
    char                    buf[8192];

    if( !arch_file || !files )
        return -1;

    arch_w = archive_write_new();

    switch( compress )
    {
        case 'j': //bz2
            //archive_write_add_filter_bzip2( arch_w );
            archive_write_set_compression_bzip2( arch_w );
            break;

        case 'J': //xz
            //archive_write_add_filter_xz( arch_w );
            archive_write_set_compression_xz( arch_w );
            break;

        case 'z': //gzip
            //archive_write_add_filter_gzip( arch_w );
            archive_write_set_compression_gzip( arch_w );
            break;
    }

    switch( format )
    {
        case 'c': //cpio
            archive_write_set_format_cpio( arch_w );
            break;

        case 't': //tar
            archive_write_set_format_ustar( arch_w );
            break;

        default: //tar
            archive_write_set_format_ustar( arch_w );
            break;
    }

    ret = archive_write_open_filename( arch_w, arch_file );
    if( ret != ARCHIVE_OK )
    {
        archive_write_finish( arch_w );
        return -1;
    }

    while( *files )
    {
        arch_r = archive_read_disk_new();
        ret = archive_read_disk_open( arch_r, *files );
        if( ret != ARCHIVE_OK )
        {
#ifdef DEBUG
            printf( "%s\n", archive_error_string( arch_r ) );
#endif
            archive_write_finish( arch_w );
            return -1;
        }

        for( ; ; )
        {
            arch_entry = archive_entry_new();
            ret = archive_read_next_header2( arch_r, arch_entry );
            if( ret == ARCHIVE_EOF )
                break;
            
            if( ret != ARCHIVE_OK )
            {
#ifdef DEBUG
                printf( "%s\n", archive_error_string( arch_r ) );
#endif
                archive_entry_free( arch_entry );
                archive_write_finish( arch_r );
                archive_write_finish( arch_w );
                return -1;
            }

            archive_read_disk_descend( arch_r );

            ret = archive_write_header( arch_w, arch_entry );
            if( ret < ARCHIVE_OK )
            {
#ifdef DEBUG
                printf( "%s\n", archive_error_string( arch_w ) );
#endif
                archive_entry_free( arch_entry );
                archive_write_finish( arch_r );
                archive_write_finish( arch_w );
                return -1;
            }

            if( ( fd = open( archive_entry_sourcepath( arch_entry ), O_RDONLY ) ) != -1 )
            {
                len = read( fd, buf, 8192 );
                while( len > 0 )
                {
                    archive_write_data( arch_w, buf, len );
                    len = read( fd, buf, 8192 );
                }
                close( fd );
            }
            archive_entry_free( arch_entry );
        }
        /* libarchive 3
        archive_read_close( arch_r );
        archive_read_free( arch_r );
        */
        archive_read_finish( arch_r );
    }

    if( arch_w )
        archive_write_finish( arch_w );

    return 0;
}
