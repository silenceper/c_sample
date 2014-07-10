/* Libypk
 *
 * Copyright (c) 2013 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Date: 2013.3.11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sqlite3.h>

#include "download.h"
#include "util.h"
#include "db.h"
#include "data.h"
#include "archive.h"
#include "xml.h"
#include "preg.h"

#include "ypackage.h"

int libypk_errno;

YPackageManager *packages_manager_init()
{
    int                 len;
    char                *source_name, *source_uri, *accept_repo, *desc, *config_file, *token, *saveptr;
    DIR                 *dir;
    struct dirent       *entry;
    YPackageManager     *pm;

    pm = malloc( sizeof(YPackageManager) );

    if( !pm )
        return NULL;

    if( access( DB_NAME, R_OK ) )
    {
        free( pm );
        return NULL;
    }

    pm->db_name = strdup( DB_NAME );

    pm->source_list =  dlist_init();
    if( !pm->source_list )
    {
        free( pm->db_name );
        free( pm );
        return NULL;
    }

    pm->log = NULL;

    //yget.conf.d
    dir = opendir( CONFIG_DIR );
    if( dir )
    {
        while( ( entry = readdir( dir ) ) )
        {
            if( !strcmp( entry->d_name, "." ) || !strcmp( entry->d_name, ".." ) )
                continue;

            len = strlen( entry->d_name );
            if( len < 6 )
                continue;

            if( strncmp( entry->d_name + len - 5, ".conf", 4 ) )
                continue;

            config_file = util_strcat( CONFIG_DIR, "/", entry->d_name, NULL );

            source_uri = util_get_config( config_file, "YPPATH_URI" );
            if( source_uri )
            {
                source_name = util_str_sha1( source_uri );
                accept_repo = util_get_config( config_file, "ACCEPT_REPO" );
                desc = util_get_config( config_file, "YPPATH_PKGDEST" );

                token = strtok_r( accept_repo, " ", &saveptr );
                while( token )
                {
                    packages_manager_add_source( pm->source_list, source_name, source_uri, token, desc );
                    token = strtok_r( NULL, " ,", &saveptr );
                }

                free( source_name );
                free( accept_repo );
                free( desc );
            }
            free( source_uri );
            free( config_file );
            config_file = NULL;
        }

        closedir( dir );
    }
    //yget.conf
    if( !access( CONFIG_FILE, R_OK ) )
    {
        config_file = CONFIG_FILE;
        source_uri = util_get_config( config_file, "YPPATH_URI" );
        if( source_uri )
        {
            desc = util_get_config( config_file, "YPPATH_PKGDEST" );
            accept_repo = util_get_config( config_file, "ACCEPT_REPO" );
            source_name = util_str_sha1( source_uri );
            token = strtok_r( accept_repo, " ", &saveptr );
            while( token )
            {
                //packages_manager_add_source( pm->source_list, "universe", source_uri, token, desc );
                packages_manager_add_source( pm->source_list, source_name, source_uri, token, desc );
                token = strtok_r( NULL, " ,", &saveptr );
            }

            free( source_name );
            free( accept_repo );
            free( desc );
        }
        free( source_uri );

        pm->log = util_get_config( config_file, "LOG" );
    }

    //default config
    if( pm->source_list->cnt == 0 )
        packages_manager_add_source( pm->source_list, "default", DEFAULT_URI, NULL, NULL );
        //packages_manager_add_source( pm->source_list, "universe", DEFAULT_URI, NULL, NULL );

    if( !pm->log )
        pm->log = strdup( LOG_FILE );

    return pm;
}


void packages_manager_cleanup( YPackageManager *pm )
{
    if( !pm )
        return;

    if( pm->db_name )
        free( pm->db_name );

    if( pm->log )
        free( pm->log );

    if( pm->source_list )
        dlist_cleanup( pm->source_list, packages_manager_free_source );

    free( pm );
}

int packages_manager_add_source( YPackageSourceList *list, char *source_name, char *source_uri, char *accept_repo, char *package_dest )
{
    YPackageSource   *source;

    if( !list || !source_uri )
        return -1;

    source =  (YPackageSource *)malloc( sizeof( YPackageSource ) );
    if( !source )
        return -2;

    source->source_name = strdup( source_name );
    source->source_uri = strdup( source_uri );
    source->accept_repo = accept_repo ? strdup( accept_repo ) : strdup( DEFAULT_REPO );
    source->package_dest = package_dest ? strdup( package_dest ) : strdup( DEFAULT_PKGDEST );
    source->updated = 0;
    dlist_append( list, source );

    return 0;
}

void packages_manager_free_source( void *node )
{
    YPackageSource *source;

    if( !node )
        return;

    source = (YPackageSource *)node;

    if( source->source_uri )
    {
        free( source->source_uri );
        source->source_uri  = NULL;
    }

    if( source->accept_repo )
    {
        free( source->accept_repo );
        source->accept_repo  = NULL;
    }

    if( source->package_dest )
    {
        free( source->package_dest );
        source->package_dest  = NULL;
    }

    if( source->source_name )
    {
        free( source->source_name );
        source->source_name  = NULL;
    }

    free( source );
}

YPackageManager *packages_manager_init2( int type )
{
    int                 ret, len;
    char                *source_name, *source_uri, *accept_repo, *desc, *config_file, *token, *saveptr;
    DIR                 *dir;
    struct dirent       *entry;
    YPackageManager     *pm;


    if( type == 1 )
    {
        ret = packages_read_lock();
    }
    else if( type == 2 )
    {
        ret = packages_write_lock();
    }
    else
    {
        libypk_errno = ARGS_INCORRECT;
        return NULL;
    }

    if( ret < 0 )
    {
        libypk_errno = LOCK_ERROR;
        return NULL;
    }

    pm = malloc( sizeof(YPackageManager) );

    if( !pm )
    {
        libypk_errno = OTHER_FAILURES;
        return NULL;
    }

    pm->lock_fd = ret;


    if( access( DB_NAME, R_OK ) )
    {
        libypk_errno = MISSING_DB;
        free( pm );
        return NULL;
    }

    pm->db_name = strdup( DB_NAME );

    pm->source_list =  dlist_init();
    if( !pm->source_list )
    {
        free( pm->db_name );
        free( pm );
        return NULL;
    }

    pm->log = NULL;

    //yget.conf.d
    dir = opendir( CONFIG_DIR );
    if( dir )
    {
        while( ( entry = readdir( dir ) ) )
        {
            if( !strcmp( entry->d_name, "." ) || !strcmp( entry->d_name, ".." ) )
                continue;

            len = strlen( entry->d_name );
            if( len < 6 )
                continue;

            if( strncmp( entry->d_name + len - 5, ".conf", 4 ) )
                continue;

            config_file = util_strcat( CONFIG_DIR, "/", entry->d_name, NULL );

            source_uri = util_get_config( config_file, "YPPATH_URI" );
            if( source_uri )
            {
                source_name = util_str_sha1( source_uri );
                accept_repo = util_get_config( config_file, "ACCEPT_REPO" );
                desc = util_get_config( config_file, "YPPATH_PKGDEST" );

                token = strtok_r( accept_repo, " ", &saveptr );
                while( token )
                {
                    packages_manager_add_source( pm->source_list, source_name, source_uri, token, desc );
                    token = strtok_r( NULL, " ,", &saveptr );
                }

                free( source_name );
                free( accept_repo );
                free( desc );
            }
            free( source_uri );
            free( config_file );
            config_file = NULL;
        }

        closedir( dir );
    }

    //yget.conf
    if( !access( CONFIG_FILE, R_OK ) )
    {
        config_file = CONFIG_FILE;
        source_uri = util_get_config( config_file, "YPPATH_URI" );
        if( source_uri )
        {
            source_name = util_str_sha1( source_uri );
            desc = util_get_config( config_file, "YPPATH_PKGDEST" );
            accept_repo = util_get_config( config_file, "ACCEPT_REPO" );
            token = strtok_r( accept_repo, " ", &saveptr );
            while( token )
            {
                packages_manager_add_source( pm->source_list, source_name, source_uri, token, desc );
                token = strtok_r( NULL, " ,", &saveptr );
            }
            free( source_name );
            free( accept_repo );
            free( desc );
        }
        free( source_uri );

        pm->log = util_get_config( config_file, "LOG" );
    }

    //default config
    if( pm->source_list->cnt == 0 )
        packages_manager_add_source( pm->source_list, "default", DEFAULT_URI, NULL, NULL );

    if( !pm->log )
        pm->log = strdup( LOG_FILE );

    return pm;
}

void packages_manager_cleanup2( YPackageManager *pm )
{
    if( !pm )
        return;

    packages_unlock( pm->lock_fd );


    if( pm->db_name )
        free( pm->db_name );

    if( pm->log )
        free( pm->log );

    if( pm->source_list )
        dlist_cleanup( pm->source_list, packages_manager_free_source );

    free( pm );
}

int packages_upgrade_db( YPackageManager *pm )
{
    size_t              len, cur_version, new_version, begin;
    char                *line, *ver_str, tmp[11];
    FILE                *fp;
    DB                  db;

    if( !pm )
        return -1;

    if( access( DB_UPGRADE, R_OK ) )
        return -1;

    fp = fopen( DB_UPGRADE, "r" );
    if( !fp )
        return -1;

    cur_version = 0;
    new_version = 0;
    begin = 0;


    db_init( &db, pm->db_name, OPEN_WRITE );
    db_query( &db, "select db_version from config", NULL );
    if( db_fetch_assoc( &db ) )
    {
        ver_str = db_get_value_by_key( &db, "db_version" );
        cur_version = ver_str ? atoi( ver_str ) : 0;
        db_close( &db );
    }
    else
    {
        if( db_exec( &db, "alter table config add column db_version text default '0'", NULL ) != SQLITE_DONE )
        {
            goto exception_handler;
        }
        db_close( &db );
    }

    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "begin", NULL );  

    line = NULL;
    while( getline( &line, &len, fp ) != -1 )
    {
        if( !strncmp( line, "Version:", 8 ) )
        {
            strncpy( tmp, line+8, 10 );
            tmp[10] = 0;
            new_version = atoi( tmp );
            if( new_version > cur_version )
            {
                cur_version = new_version; 
                begin = 1;
            }
        }
        else if( !strncmp( line, "End", 3 ) )
        {
            if( begin )
            {
                if( db_exec( &db, "update config set db_version=?", tmp, NULL ) != SQLITE_DONE )
                {
                    goto exception_handler;
                }

                begin = 0;
            }
        }
        else
        {
            if( begin && strlen( line ) > 3 )
            {
                //printf( "debug: exec %s\n", line );
                if( db_exec( &db, line, NULL ) != SQLITE_DONE )
                {
                    //goto exception_handler;
                    ;
                }
            }
        }

    }

    db_exec( &db, "end", NULL );  

    if( line )
        free( line );

    fclose( fp );

    db_close( &db );
    return 0;

exception_handler:
    db_exec( &db, "rollback", NULL );  
    db_close( &db );
    return -1;
}

int packages_lock( int type )
{
    int             fd;
    struct flock    lock;

    umask( 0 );
    fd = open( LOCK_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

    if( fd < 0 )
    {
        return -1;
    }

    lock.l_type = type == 2 ? F_WRLCK : F_RDLCK; 
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    if( fcntl( fd, F_SETLK, &lock ) == -1 )
    {
        close( fd );
        return -2;
    }

    return fd;
}

int packages_unlock( int fd )
{
    struct flock    lock;

    if( fd < 1 )
    {
        return -1;
    }

    lock.l_type = F_UNLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    if( fcntl( fd, F_SETLK, &lock ) == -1 )
    {
        close( fd );
        return -2;
    }

    close( fd );
    return 0;
}


int packages_import_local_data( YPackageManager *pm )
{
    int                 xml_ret, i;
    size_t              list_len;
    char                *sql, *sql_testing, *sql_history, *sql_data, *sql_testing_data, *sql_history_data, *sql_filelist, *idx, *data_key,*data_name, *data_format, *data_size, *data_install_size, *data_depend, *data_bdepend, *data_recommended, *data_conflict, *file_path, *file_path_sub, *list_line;
    char                 *saveptr, *package_name, *is_desktop, *version, *repo, *install_time, *install_size, *file_type, *file_file, *file_size, *file_perms, *file_uid, *file_gid, *file_mtime, *file_extra, *last_update;
    char                *xml_attrs[] = {"name", "type", "lang", "id", NULL};
    struct stat         statbuf;
    struct dirent       *entry, *entry_sub;
    DIR                 *dir, *dir_sub;
    FILE                *fp;
    XMLReaderHandle   xml_handle;
    DB                  db;

    //init
    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "begin", NULL );  

    printf( "Start ...\n" );


    printf( "Import universe ...\n" );
    //import universe
    reader_open( LOCAL_UNIVERSE,  &xml_handle );
    sql = "replace into universe (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sql_testing = "replace into universe_testing (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sql_history = "replace into universe_history (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    while( ( xml_ret = reader_fetch_a_row( &xml_handle, 1, xml_attrs ) ) == 1 )
    {
        is_desktop = reader_get_value( &xml_handle, "genericname|desktop|keyword|en" );
        package_name = reader_get_value2( &xml_handle, "name" );
        version = reader_get_value2( &xml_handle, "version" );
        repo = reader_get_value( &xml_handle, "repo" );

        //universe
        if( repo && !strcmp( repo, "stable" ) )
        {
            db_exec( &db, sql,  
                    package_name, //name
                    is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                    is_desktop ? "1" : "0", //desktop
                    reader_get_value2( &xml_handle, "category" ), //category
                    reader_get_value2( &xml_handle, "arch" ), //arch
                    version, //version
                    reader_get_value2( &xml_handle, "priority" ), //priority
                    reader_get_value2( &xml_handle, "install" ), //install
                    reader_get_value2( &xml_handle, "license" ), //license
                    reader_get_value2( &xml_handle, "homepage" ), //homepage
                    reader_get_value2( &xml_handle, "repo" ), //repo
                    reader_get_value2( &xml_handle, "size" ), //size
                    reader_get_value2( &xml_handle, "sha" ), //sha
                    reader_get_value2( &xml_handle, "build_date" ), //build_date
                    reader_get_value2( &xml_handle, "packager" ), //packager
                    reader_get_value2( &xml_handle, "uri" ), //uri
                    is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                    reader_get_value2( &xml_handle, "data_count" ), //data_count
                    NULL);
        }

        db_exec( &db, sql_testing,  
                package_name, //name
                is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                is_desktop ? "1" : "0", //desktop
                reader_get_value2( &xml_handle, "category" ), //category
                reader_get_value2( &xml_handle, "arch" ), //arch
                version, //version
                reader_get_value2( &xml_handle, "priority" ), //priority
                reader_get_value2( &xml_handle, "install" ), //install
                reader_get_value2( &xml_handle, "license" ), //license
                reader_get_value2( &xml_handle, "homepage" ), //homepage
                reader_get_value2( &xml_handle, "repo" ), //repo
                reader_get_value2( &xml_handle, "size" ), //size
                reader_get_value2( &xml_handle, "sha" ), //sha
                reader_get_value2( &xml_handle, "build_date" ), //build_date
                reader_get_value2( &xml_handle, "packager" ), //packager
                reader_get_value2( &xml_handle, "uri" ), //uri
                is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                reader_get_value2( &xml_handle, "data_count" ), //data_count
                NULL);

        //universe_history
        db_exec( &db, sql_history,  
                package_name, //name
                is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                is_desktop ? "1" : "0", //desktop
                reader_get_value2( &xml_handle, "category" ), //category
                reader_get_value2( &xml_handle, "arch" ), //arch
                version, //version
                reader_get_value2( &xml_handle, "priority" ), //priority
                reader_get_value2( &xml_handle, "install" ), //install
                reader_get_value2( &xml_handle, "license" ), //license
                reader_get_value2( &xml_handle, "homepage" ), //homepage
                reader_get_value2( &xml_handle, "repo" ), //repo
                reader_get_value2( &xml_handle, "size" ), //size
                reader_get_value2( &xml_handle, "sha" ), //sha
                reader_get_value2( &xml_handle, "build_date" ), //build_date
                reader_get_value2( &xml_handle, "packager" ), //packager
                reader_get_value2( &xml_handle, "uri" ), //uri
                is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                reader_get_value2( &xml_handle, "data_count" ), //data_count
                NULL);
    
        //universe_data
        if( repo && !strcmp( repo, "stable" ) )
        {
            db_exec( &db, "delete from universe_data where name=?", package_name, NULL );  
        }

        db_exec( &db, "delete from universe_testing_data where name=?", package_name, NULL );  

        db_exec( &db, "delete from universe_history_data where name=? and version=?", package_name, version, NULL );  

        sql_data = "insert into universe_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sql_testing_data = "insert into universe_testing_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sql_history_data = "insert into universe_history_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        data_key = (char *)malloc( 32 );
        for( i = 0; ; i++ )
        {
            idx = util_int_to_str( i );
            if( !reader_get_value( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) ) )
            {
                free( idx );
                break;
            }

            data_name = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) );  
            data_format  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|format", NULL ) );   
            data_size = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|size", NULL ) );   
            data_install_size  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|install_size", NULL ) );
            data_depend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|depend", NULL ) );    
            data_bdepend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|bdepend", NULL ) );    
            data_recommended = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|recommended", NULL ) );    
            data_conflict  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|conflict", NULL ) );   

            if( repo && !strcmp( repo, "stable" ) )
            {
                db_exec( &db, sql_data,  
                        package_name, //name
                        version, //version
                        data_name, //data_name
                        data_format, //data_format
                        data_size, //data_size
                        data_install_size, //data_install_size
                        data_depend, //data_depend
                        data_bdepend, //data_bdepend
                        data_recommended, //data_recommended
                        data_conflict, //data_conflict
                        NULL);
            }

            db_exec( &db, sql_testing_data,  
                    package_name, //name
                    version, //version
                    data_name, //data_name
                    data_format, //data_format
                    data_size, //data_size
                    data_install_size, //data_install_size
                    data_depend, //data_depend
                    data_bdepend, //data_bdepend
                    data_recommended, //data_recommended
                    data_conflict, //data_conflict
                    NULL);

            db_exec( &db, sql_history_data,  
                    package_name, //name
                    version, //version
                    data_name, //data_name
                    data_format, //data_format
                    data_size, //data_size
                    data_install_size, //data_install_size
                    data_depend, //data_depend
                    data_bdepend, //data_bdepend
                    data_recommended, //data_recommended
                    data_conflict, //data_conflict
                    NULL);
            free( idx );
        }
        free( data_key );
    }
    reader_cleanup( &xml_handle );

    //world
    printf( "Import world ...\n" );
    reader_open( LOCAL_WORLD,  &xml_handle );
    sql = "replace into world (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    //char * sql_test;
    //sql_test = "replace into world (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s');";

    while( ( xml_ret = reader_fetch_a_row( &xml_handle, 1, xml_attrs ) ) == 1 )
    {
        is_desktop = reader_get_value( &xml_handle, "genericname|desktop|keyword|en" );
        package_name = reader_get_value2( &xml_handle, "name" );
        version = reader_get_value2( &xml_handle, "version" );

        //world
        db_exec( &db, sql,  
                package_name, //name
                is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                is_desktop ? "1" : "0", //desktop
                reader_get_value2( &xml_handle, "category" ), //category
                reader_get_value2( &xml_handle, "arch" ), //arch
                version, //version
                reader_get_value2( &xml_handle, "priority" ), //priority
                reader_get_value2( &xml_handle, "install" ), //install
                reader_get_value2( &xml_handle, "license" ), //license
                reader_get_value2( &xml_handle, "homepage" ), //homepage
                reader_get_value2( &xml_handle, "repo" ), //repo
                reader_get_value2( &xml_handle, "size" ), //size
                reader_get_value2( &xml_handle, "sha" ), //sha
                reader_get_value2( &xml_handle, "build_date" ), //build_date
                reader_get_value2( &xml_handle, "packager" ), //packager
                reader_get_value2( &xml_handle, "uri" ), //uri
                is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                reader_get_value2( &xml_handle, "data_count" ), //data_count
                NULL);

        //world_data
        db_exec( &db, "delete from world_data where name=?", package_name, NULL );  

        sql_data = "insert into world_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        data_key = (char *)malloc( 32 );
        for( i = 0; ; i++ )
        {
            idx = util_int_to_str( i );
            if( !reader_get_value( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) ) )
            {
                free( idx );
                break;
            }

            data_name = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) );  
            data_format  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|format", NULL ) );   
            data_size = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|size", NULL ) );   
            data_install_size  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|install_size", NULL ) );
            data_depend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|depend", NULL ) );    
            data_bdepend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|bdepend", NULL ) );    
            data_recommended = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|recommended", NULL ) );    
            data_conflict  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|conflict", NULL ) );   

            db_exec( &db, sql_data,  
                    package_name, //name
                    version, //version
                    data_name, //data_name
                    data_format, //data_format
                    data_size, //data_size
                    data_install_size, //data_install_size
                    data_depend, //data_depend
                    data_bdepend, //data_bdepend
                    data_recommended, //data_recommended
                    data_conflict, //data_conflict
                    NULL);
            free( idx );
        }
        free( data_key );

        //update universe
        db_exec( &db, "update universe set installed=1  where name=?", package_name, NULL );  
        db_exec( &db, "update universe_testing set installed=1  where name=?", package_name, NULL );  
    }
    reader_cleanup( &xml_handle );


    //world_file
    dir = opendir( PACKAGE_DB_DIR );
    if( !dir )
        return -1;

    while( (entry = readdir( dir )) )
    {
        if( !strcmp( entry->d_name, "." ) || !strcmp( entry->d_name, ".." ) )
        {
            continue;
        }

        package_name = entry->d_name;
        //printf( "%s\n", package_name );
        file_path = util_strcat( PACKAGE_DB_DIR, "/", entry->d_name, NULL );
        if( !stat( file_path, &statbuf ) && S_ISDIR( statbuf.st_mode ) )
        {
            //sub dir
            dir_sub = opendir( file_path );
            if( dir_sub )
            {
                while( (entry_sub = readdir( dir_sub )) )
                {
                    if( strstr(entry_sub->d_name, ".desc") )
                    {
                        file_path_sub = util_strcat( file_path, "/", entry_sub->d_name, NULL );
                        install_time = util_get_config( file_path_sub, "INSTALL_TIME" );
                        install_size = util_get_config( file_path_sub, "INSTALL_SIZE" );
                        if( install_time )
                        {
                            db_exec( &db, "update world set install_time=?, size=? where name=?", install_time, install_size ? install_size : "0", package_name, NULL );  
                        }
                        else
                        {
                            db_exec( &db, "update world set install_time=strftime('%s','now'), size=? where name=?", package_name,  install_size ? install_size : "0", NULL );  
                        }

                        if( install_time )
                            free( install_time );

                        if( install_size )
                            free( install_size );

                        free( file_path_sub );
                    }

                    if( strstr(entry_sub->d_name, ".list") )
                    {
                        file_path_sub = util_strcat( file_path, "/", entry_sub->d_name, NULL );

                        db_exec( &db, "delete from world_file where name=?", package_name, NULL );  
                        sql_filelist = "insert into world_file (name, type, file, size, perms, uid, gid, mtime, extra) values (?, ?, ?, ?, ?, ?, ?, ?, ?)"; 

                        fp = fopen( file_path_sub, "r" );
                        list_line = NULL;
                        while( getline( &list_line, &list_len, fp ) != -1 )
                        {
                            if( list_line[0] != 'I' )
                            {
                                file_type = strtok_r( list_line, " ,", &saveptr );
                                file_file = strtok_r( NULL, " ,", &saveptr );
                                file_size = strtok_r( NULL, " ,", &saveptr );
                                file_perms = strtok_r( NULL, " ,", &saveptr );
                                file_uid = strtok_r( NULL, " ,", &saveptr );
                                file_gid = strtok_r( NULL, " ,", &saveptr );
                                file_mtime = strtok_r( NULL, " ,", &saveptr );
                                file_extra = strtok_r( NULL, " ,", &saveptr );

                                db_exec( &db, sql_filelist, 
                                        package_name,
                                        file_type ? file_type : "",
                                        file_file ? file_file : "",
                                        file_size ? file_size : "",
                                        file_perms ? file_perms : "",
                                        file_uid ? file_uid : "",
                                        file_gid ? file_gid : "",
                                        file_mtime ? file_mtime : "",
                                        file_extra ? file_extra : "",
                                        NULL );
                            }

                        }
                        if( list_line )
                            free( list_line );

                        fclose( fp );
                        free( file_path_sub );
                    }
                }
                closedir( dir_sub );
            }
        }
        free( file_path );
    }
    closedir( dir );

    //update config
    db_query( &db, "select max(build_date) from universe_testing", NULL );
    db_fetch_num( &db );
    last_update = db_get_value_by_index( &db, 0 );
    db_cleanup( &db );
    db_exec( &db, "update config set last_update=?, last_check=?", last_update, last_update, NULL );  

    db_exec( &db, "end", NULL );  
    printf( "Done!\n" );
    //clean up
    db_close( &db );
    return 0;
}

int packages_add_source( YPackageManager *pm, char *source_name, char *repo )
{
    size_t              len;
    char                *line = NULL, *tmp = NULL, *sql = NULL;
    FILE                *fp;
    DB                  db;


    if( !pm || !source_name || !repo )
        return -1;

    if( access( DB_SOURCE_TEMPLATE, R_OK ) )
        return -1;

    if( !strcmp( source_name, "universe" ) || !strcmp( source_name, "world" ) )
        return -1;

    fp = fopen( DB_SOURCE_TEMPLATE, "r" );
    if( !fp )
        return -1;

    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "begin", NULL );  

    while( getline( &line, &len, fp ) != -1 )
    {
        if( strlen( line ) > 3 )
        {
            tmp = preg_replace( "xxx", source_name, line, PCRE_CASELESS, 0 );
            sql = preg_replace( "yyy", repo, tmp, PCRE_CASELESS, 0 );
            //printf( "debug: exec %s\n", sql );
            if( db_exec( &db, sql, NULL ) != SQLITE_DONE )
            {
                goto exception_handler;
            }
            free( tmp );
            tmp = NULL;
            free( sql );
            sql = NULL;
        }
    }

    db_exec( &db, "end", NULL );  

    if( line )
        free( line );

    if( tmp )
        free( tmp );

    if( sql )
        free( sql );

    fclose( fp );

    db_close( &db );
    return 0;

exception_handler:
    db_exec( &db, "rollback", NULL );  
    db_close( &db );

    if( line )
        free( line );

    if( tmp )
        free( tmp );

    if( sql )
        free( sql );

    fclose( fp );
    return -1;
}

int packages_check_update( YPackageManager *pm )
{
    return 1;
}


int packages_update( YPackageManager *pm, ypk_progress_callback cb, void *cb_arg )
{
    int             timestamp, len, cnt;
    char            *sql, *source_select, *tmp, *package_name, *version_installed, *version, *target_url, *list_line, *last_checksum, update_file[32], sum[48], buf[256];
    char            tmp_sql[] = "/tmp/tmp_sql.XXXXXX";
    FILE            *fp;
    DownloadContent content;
    YPackageSource  *source;
    DB              db;

    cnt = 0;


    if( cb )
    {
        cb( cb_arg, "", 0, 1, "Synchronizing new packages list ...\n" );
    }

    //init cache table
    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "create table if not exists universe_cache as select * from universe limit 0", NULL );
    db_exec( &db, "create table if not exists universe_data_cache as select * from universe_data limit 0", NULL );
    db_exec( &db, "create unique index if not exists cache_id on universe_cache(name,version,repo,source)", NULL );
    db_exec( &db, "create unique index if not exists cache_data_id on universe_data_cache(name,version,repo,source)", NULL );
    db_close( &db );

    //update cache table
    source = dlist_head_data( pm->source_list );
    while( source )
    {
        last_checksum = packages_get_source_checksum( pm, source->source_name, source->accept_repo );
        if( !last_checksum )
        {
            if( packages_add_source( pm, source->source_name, source->accept_repo ) == -1 )
            {
                source = dlist_next_data( pm->source_list );
                continue;
            }
        }

        target_url = util_strcat( source->source_uri, "/", UPDATE_DIR "/" LIST_FILE, NULL );
        content.text = malloc(256);
        content.size = 0;
        if( get_content( target_url, &content ) != 0 )
        {
            free( content.text );
            free( target_url );
            target_url = NULL;
            source = dlist_next_data( pm->source_list );
            continue;
        }

        list_line = util_mem_gets( content.text );
        while( list_line )
        {
            util_rtrim( list_line, 0 );
            memset( update_file, '\0', 32 );
            memset( sum, '\0', 48 );
            if( sscanf( list_line, "%s %d %s", update_file, &timestamp, sum ) == 3 )
            {
                len = strlen( source->accept_repo );
                if( !strncmp( source->accept_repo, update_file, len ) )
                {
                    if( (!last_checksum  || strcmp( sum, last_checksum ) ) )
                    {
                        if( !packages_update_single_xml( pm, source, update_file, sum, cb, cb_arg  ) )
                        {
                            packages_set_source_checksum( pm, source->source_name, source->accept_repo, sum );
                            source->updated++;
                            cnt++;
                        }
                    }
                    else
                    {
                        source->updated++;
                    }
                }
            }
            free( list_line );
            list_line = util_mem_gets( NULL );
        }

        if( last_checksum )
            free( last_checksum );
        free( content.text );
        free( target_url );
        target_url = NULL;

        source = dlist_next_data( pm->source_list );
    }

    tmp = NULL;
    source_select = NULL;
    db_init( &db, pm->db_name, OPEN_WRITE );

    //update universe
    db_create_collation( &db, "vercmp", packages_compare_version_collate, NULL );
    db_create_function( &db, "max_ver", 1,  NULL, &packages_max_version_step, &packages_max_version_final );
    source = dlist_head_data( pm->source_list );
    while( source )
    {
        if( source->updated )
        {
            if( source_select )
            {
                tmp = source_select;
                source_select = util_strcat( tmp, " or (source='", source->source_name, "' and repo='", source->accept_repo, "')", NULL );
            }
            else
            {
                source_select = util_strcat( "(source='", source->source_name, "' and repo='", source->accept_repo, "')", NULL );
            }

            if( tmp )
                free( tmp );
            tmp = NULL;
        }

        source = dlist_next_data( pm->source_list );
    }

    db_exec( &db, "begin", NULL );

    db_exec( &db, "delete from universe", NULL );
    sql = util_strcat( "insert into universe select * from universe_cache where (", source_select, ") group by name having version=max_ver(version)", NULL );
    db_exec( &db, sql, NULL );
    free( sql );
    sql = NULL;

    db_exec( &db, "delete from universe_data", NULL );
    sql = util_strcat( "insert into universe_data select * from universe_data_cache where (", source_select, ") group by name having version=max_ver(version)", NULL );
    free( source_select );
    source_select = NULL;
    db_exec( &db, sql, NULL );
    free( sql );
    sql = NULL;

    db_exec( &db, "end", NULL );  
    db_cleanup( &db );

    //update world
    sql = util_strcat( "select a.name, a.version, b.version from world as a left join universe as b using (name) where a.version <> b.version  order by b.name, b.version collate vercmp", NULL );
    free( source_select );
    source_select = NULL;
    db_query( &db, sql, NULL);
    free( sql );
    sql = NULL;

    mkstemp( tmp_sql );
    fp = fopen( tmp_sql, "w+" );

    while( db_fetch_num( &db ) )
    {
        package_name = db_get_value_by_index( &db, 0 );
        version_installed = db_get_value_by_index( &db, 1 );
        version = db_get_value_by_index( &db, 2 );
        if( packages_compare_version( version, version_installed ) == 1 )
        {
            fprintf( fp, "update world set can_update=1, version_available='%s' where name='%s'\n", version, package_name );
        }
    }

    rewind( fp );
    memset( buf, 0, 256 );
    db_cleanup( &db );
    db_exec( &db, "begin", NULL );  
    db_exec( &db, "update world set can_update=0", NULL );
    while( fgets( buf, 255, fp ) )
    {
        util_rtrim( buf, '\n' );
        db_exec( &db, buf, NULL );
    }

    db_exec( &db, "end", NULL );  
    fclose( fp );
    remove( tmp_sql );
    db_close( &db );

    if( cb )
    {
        cb( cb_arg, "", 9, 1, NULL );
    }

    return cnt;
}


/*
 * packages_get_upgrade_list
 */
YPackageChangeList *packages_get_upgrade_list( YPackageManager *pm )
{    
    int             pkg_count, i, wildcards[] = { 2, 0 };
    char            *package_name, *version, *can_upgrade;
    char            *keys[] = { "can_update", NULL }, *keywords[] = { "%1", NULL };
    YPackageList    *pkg_list;
    YPackageChangeList     *list;

    list = NULL;

    pkg_count = packages_get_count( pm,  keys, keywords, wildcards, 1 );

    if( pkg_count > 0 )
    {
        pkg_list = packages_get_list( pm, pkg_count, 0, keys, keywords, wildcards, 1 );
        if( pkg_list )
        {
            list = dlist_init();
            for( i = 0; i < pkg_list->cnt; i++ )
            {
                package_name = packages_get_list_attr( pkg_list, i, "name" );
                version = packages_get_list_attr( pkg_list, i, "version" );
                can_upgrade = packages_get_list_attr( pkg_list, i, "can_update" );

                if( can_upgrade && can_upgrade[0] )
                {
                    packages_clist_append( list, package_name, version, 0, can_upgrade[0] == '1' ? 4 : 5 );
                }
                else
                {
                    packages_clist_append( list, package_name, version, 0, 4 );
                }

            }
            packages_free_list( pkg_list );
        }
    }

    return list;

}

int packages_upgrade_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg   )
{
    YPackageChangePackage    *cur_pkg;

    if( !list )
        return -1;

    cur_pkg = dlist_head_data( list );
    while( cur_pkg )
    {
        //printf("upgrading %s ...\n", cur_pkg->name );
        packages_install_package( pm, cur_pkg->name, cur_pkg->version, cb, cb_arg  );
        cur_pkg = dlist_next_data( list );
    }
    return 0;
}

void packages_free_upgrade_list( YPackageChangeList *list )
{
    packages_free_change_list( list );
}


int packages_update_single_xml( YPackageManager *pm, YPackageSource *source, char *update_file, char *sum, ypk_progress_callback cb, void *cb_arg )
{
    int                 i, xml_ret, db_ret;
    char                *xml_sha, *target_url, *msg, *sql, *sql_data, *package_name, *version, *is_desktop, *repo, *idx, *data_key,*data_name, *data_format, *data_size, *data_install_size, *data_depend, *data_bdepend, *data_recommended, *data_conflict, *data_replace;
    char                tmp_bz2[] = "/tmp/tmp_bz2.XXXXXX";
    char                tmp_xml[] = "/tmp/tmp_xml.XXXXXX";
    char                *xml_attrs[] = {"name", "type", "lang", "id", NULL};
    DownloadFile        file;
    XMLReaderHandle     xml_handle;
    DB                  db;

                    
    if( !pm || !update_file || !sum )
        return -1;

    //donload xml
    target_url = util_strcat( source->source_uri, "/", UPDATE_DIR, "/", update_file, NULL );

    if( cb )
    {
        msg = util_strcat( "Get: ", target_url, NULL );

        cb( cb_arg, "*", 2, -1, msg ? msg : NULL );

        if( msg )
        {
            free( msg );
            msg = NULL;
        }
    }

    mkstemp(tmp_bz2);
    file.file = tmp_bz2;
    file.stream = NULL;
    file.cb = NULL;
    file.cb_arg = NULL;
    if( download_file( target_url, &file ) != 0 )
    {
        if( cb )
        {
            cb( cb_arg, "Download failed.\n", 2, 1, NULL );
        }
        remove(tmp_bz2);
        return -1; 
    }
    fclose(file.stream);
    free(target_url);
    target_url = NULL;


    //compare sum
    xml_sha = util_sha1( tmp_bz2 );
    if( xml_sha )
    {
        if( strncmp( xml_sha, sum, 41 ) != 0 )
        {
            if( cb )
            {
                cb( cb_arg, "Checksum mismatched.\n", 2, 1, NULL );
            }
            free( xml_sha );
            return -1;
        }
        free( xml_sha );
    }

    if( cb )
    {
        cb( cb_arg, "*", 2, 1, NULL );
    }

    //unzip
    if( cb )
    {
        msg = strdup( "Extracting information ..." );

        cb( cb_arg, "*", 4, -1, msg ? msg : NULL );

        if( msg )
        {
            free( msg );
            msg = NULL;
        }
    }

    mkstemp(tmp_xml);
    if( archive_extract_file( file.file, "update.xml", tmp_xml ) == -1 )
    {
        remove(tmp_bz2);
        remove(tmp_xml);
        return -1;
    }

    if( cb )
    {
        cb( cb_arg, "*", 4, 1, NULL );
    }


    if( cb )
    {
        msg = strdup( "Updating database ..." );

        cb( cb_arg, "*", 8, -1, msg ? msg : NULL );

        if( msg )
        {
            free( msg );
            msg = NULL;
        }
    }

    reader_open( tmp_xml,  &xml_handle );
    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "begin", NULL );  

    //clean up 
    sql = util_strcat( "delete from universe_cache where source='", source->source_name, "' and repo='", source->accept_repo, "'", NULL );
    db_exec( &db, sql, NULL );  
    free( sql );

    sql = util_strcat( "delete from universe_data_cache where source='", source->source_name, "' and repo='", source->accept_repo, "'", NULL );
    db_exec( &db, sql, NULL );  
    free( sql );

    //import records
    sql = NULL;
    sql = strdup( "replace into universe_cache (name, exec, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, source, repo, size, sha, build_date, packager, uri, description, data_count, can_update, installed ) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);" );

    sql_data = strdup( "replace into universe_data_cache (name, version, source, repo, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict, data_replace) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);" );

    while( ( xml_ret = reader_fetch_a_row( &xml_handle, 1, xml_attrs ) ) == 1 )
    {
        package_name = reader_get_value2( &xml_handle, "name" );
        version = reader_get_value2( &xml_handle, "version" );
        is_desktop = reader_get_value( &xml_handle, "genericname|desktop|keyword|en" );
        repo = reader_get_value2( &xml_handle, "repo" );

        db_ret = db_exec( &db, sql,  
                package_name, //name
                reader_get_value2( &xml_handle, "exec" ), //exec
                is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                is_desktop ? "1" : "0", //desktop
                reader_get_value2( &xml_handle, "category" ), //category
                reader_get_value2( &xml_handle, "arch" ), //arch
                version, //version
                reader_get_value2( &xml_handle, "priority" ), //priority
                reader_get_value2( &xml_handle, "install" ), //install
                reader_get_value2( &xml_handle, "license" ), //license
                reader_get_value2( &xml_handle, "homepage" ), //homepage
                source->source_name,
                repo, //repo
                reader_get_value2( &xml_handle, "size" ), //size
                reader_get_value2( &xml_handle, "sha" ), //sha
                reader_get_value2( &xml_handle, "build_date" ), //build_date
                reader_get_value2( &xml_handle, "packager" ), //packager
                reader_get_value2( &xml_handle, "uri" ), //uri
                is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                reader_get_value2( &xml_handle, "data_count" ), //data_count
                "0",
                "0",
                NULL);


        data_key = (char *)malloc( 32 );
        for( i = 0; ; i++ )
        {
            idx = util_int_to_str( i );
            if( !reader_get_value( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) ) )
            {
                free( idx );
                break;
            }

            data_name = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) );  
            data_format  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|format", NULL ) );   
            data_size = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|size", NULL ) );   
            data_install_size  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|install_size", NULL ) );
            data_depend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|depend", NULL ) );    
            data_bdepend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|bdepend", NULL ) );    
            data_recommended = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|recommended", NULL ) );    
            data_conflict  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|conflict", NULL ) );   
            data_replace  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|replace", NULL ) );   

            //if( repo && !strcmp( repo, "stable" ) )
            //{
                db_exec( &db, sql_data,  
                        package_name, //name
                        version, //version
                        source->source_name,
                        repo,
                        data_name, //data_name
                        data_format, //data_format
                        data_size, //data_size
                        data_install_size, //data_install_size
                        data_depend, //data_depend
                        data_bdepend, //data_bdepend
                        data_recommended, //data_recommended
                        data_conflict, //data_conflict
                        data_replace, //data_replace
                        NULL);
            //}
            free( idx );
        }//for
        free( data_key );
    }

    db_ret = db_exec( &db, "commit", NULL );  
    if( db_ret == SQLITE_BUSY )
    {
        db_ret = db_exec( &db, "commit", NULL );  
        //printf( "db_exec commit:%d\n", db_ret);
    }

    if( db_ret == SQLITE_BUSY )
    {
        db_ret = db_exec( &db, "rollback", NULL );  
        //printf( "rollback, db_ret:%d\n", db_ret );
    }
    //clean up
    db_close( &db );
    reader_cleanup( &xml_handle );

    if( cb )
    {
        cb( cb_arg, "*", 8, 1, NULL );
    }

    free( sql );
    sql = NULL;
    free( sql_data );
    sql_data = NULL;
    remove(tmp_bz2);
    remove(tmp_xml);
    return 0;
}

int packages_get_last_check_timestamp( YPackageManager *pm, char *source_name, char *repo )
{
    int     last_check, has_new;
    DB      db;

    if( !pm )
        return -1;

    if( !source_name )
        source_name = "universe";

    if( !repo )
        repo = "stable";

    db_init( &db, pm->db_name, OPEN_READ );
    //db_query( &db, "select has_new, last_check from config limit 1", NULL );
    db_query( &db, "select has_new, last_check from source where name=? and repo=?", source_name, repo, NULL );
    if( db_fetch_row( &db, RESULT_NUM ) == 0 )
    {
        //packages_add_source( pm, source_name, repo );
        db_close( &db );
        return -1;
    }

    has_new = atoi( db.crow[0] );
    if( has_new )
    {
        db_close( &db );
        return 1;
    }

    last_check = atoi( db.crow[1] );
    db_close( &db );
    return last_check;
}


int packages_set_last_check_timestamp( YPackageManager *pm, char *source_name, char *repo, int last_check )
{
    int     ret = -1;
    char    *timestamp;
    DB      db;

    if( !pm || !source_name || !repo || last_check < 0 )
        return -1;

    db_init( &db, pm->db_name, OPEN_WRITE );
    timestamp = util_int_to_str( last_check );
    //if( db_exec( &db, "update config set has_new = 1, last_check = ?", timestamp, NULL ) )
    if( db_exec( &db, "update source set has_new = 1, last_check = ? where name=? and repo=?", timestamp, source_name, repo, NULL ) )
    {
        ret = 0;
    }

    free( timestamp );
    db_close( &db );
    return ret;
}


int packages_get_last_update_timestamp( YPackageManager *pm, char *source_name, char *repo )
{
    int     last_update;
    DB      db;

    if( !pm )
        return -1;

    if( !source_name )
        source_name = "universe";

    if( !repo )
        repo = "stable";

    db_init( &db, pm->db_name, OPEN_READ );
    //db_query( &db, "select has_new, last_update from config limit 1", NULL );
    db_query( &db, "select last_update from source where name=? and repo=?", source_name, repo, NULL );
    if( db_fetch_row( &db, RESULT_NUM ) == 0 )
    {
        //packages_add_source( pm, source_name, repo );
        db_close( &db );
        return -1;
    }

    /*
    has_new = atoi( db.crow[0] );
    if( has_new )
    {
        db_close( &db );
        return 1;
    }
    */

    last_update = atoi( db.crow[0] );
    db_close( &db );
    return last_update;
}


int packages_set_last_update_timestamp( YPackageManager *pm, char *source_name, char *repo, int last_update )
{
    int     ret = -1;
    char    *timestamp;
    DB      db;

    if( !pm || !source_name || !repo || last_update < 0 )
        return -1;

    db_init( &db, pm->db_name, OPEN_WRITE );
    timestamp = util_int_to_str( last_update );
    //if( db_exec( &db, "update config set has_new = 1, last_update = ?", timestamp, NULL ) )
    if( db_exec( &db, "update source set last_update = ? where name=? and repo=?", timestamp, source_name, repo, NULL ) )
    {
        ret = 0;
    }

    free( timestamp );
    db_close( &db );
    return ret;
}

char *packages_get_source_checksum( YPackageManager *pm, char *source_name, char *repo )
{
    char    *checksum, *tmp;
    DB      db;

    if( !pm )
        return NULL;

    if( !source_name )
        source_name = "universe";

    if( !repo )
        repo = "stable";

    db_init( &db, pm->db_name, OPEN_READ );
    db_query( &db, "select checksum from source where name=? and repo=?", source_name, repo, NULL );
    if( db_fetch_num( &db ) == 0 )
    {
        db_close( &db );
        return NULL;
    }

    tmp =  db_get_value_by_index( &db, 0 );
    checksum = tmp ? strdup( tmp ) : NULL;

    db_close( &db );
    return checksum;
}

int packages_set_source_checksum( YPackageManager *pm, char *source_name, char *repo, char *checksum )
{
    int     ret = -1;
    DB      db;

    if( !pm || !source_name || !repo || !checksum )
        return -1;

    db_init( &db, pm->db_name, OPEN_WRITE );
    if( db_exec( &db, "update source set checksum = ? where name=? and repo=?", checksum, source_name, repo, NULL ) )
    {
        ret = 0;
    }

    db_close( &db );
    return ret;
}

int packages_get_installed_count( YPackageManager *pm, char *keys[], char *keywords[], int wildcards[] )
{
    int     count;
    char    *sql, *where_str, *tmp;
    DB      db;

    if( !pm )
        return -1;

    db_init( &db, pm->db_name, OPEN_READ );
    if( !keys || !keywords || !wildcards || !(*keys) || !(*keywords) || !(*wildcards) )
    {
        sql = util_strcat( "select count(*) from world", NULL );
        db_query( &db, sql, NULL);
        free( sql );
    }
    else
    {
        where_str = NULL;

        while( *keys && *keywords && *wildcards )
        {
            tmp = NULL;
            if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 2) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(world.name like '%", *keywords, "%' or world.generic_name like  '%", *keywords, "%' or world.description like  '%", *keywords, "%' or keywords.kw_name like  '%", *keywords, "%' or keywords.kw_generic_name like  '%", *keywords, "%' or keywords.kw_fullname like  '%", *keywords, "%' or keywords.kw_comment like '%", *keywords, "%')", NULL );
                if( tmp )
                    free( tmp );
                tmp = NULL;
            }
            else if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 1) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(world.name = '", *keywords, "' or world.generic_name = '", *keywords, "' or keywords.kw_name = '", *keywords, "' or keywords.kw_generic_name = '", *keywords, "' or keywords.kw_fullname = '", *keywords, "')", NULL );
                if( tmp )
                    free( tmp );
                tmp = NULL;
            }
            else if( *keywords && *keys && *wildcards )
            {
                if( where_str )
                    tmp = where_str;

                if( strncmp( *keys, "kw_", 3 ) && strcmp( *keys, "language" ) )
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (" : "(world.", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }
                else
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (keywords." : "(keywords.", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }

                if( tmp )
                    free( tmp );
            }
            else
            {
                break;
            }
            keys++;
            keywords++;
            wildcards++;
        }

        if( where_str )
        {
            sql = util_strcat( "select count(distinct world.name) from world left join keywords using (name) where ", where_str, NULL );
            db_query( &db, sql, NULL );
            //printf( "%s\n", sql);

            free( where_str );
            free( sql );
        }
    }

    db_fetch_num( &db );
    tmp =  db_get_value_by_index( &db, 0 );
    count = tmp ? atoi( tmp ) : 0;
    db_close( &db );

    return count;
}

int packages_get_count( YPackageManager *pm, char *keys[], char *keywords[], int wildcards[], int installed  )
{
    int     count;
    char    *sql, *where_str, *tmp;
    DB      db;

    if( !pm )
        return -1;

    if( installed )
        return packages_get_installed_count( pm, keys, keywords, wildcards );

    where_str = NULL;

    db_init( &db, pm->db_name, OPEN_READ );

    tmp = NULL;

    if( !keys || !keywords || !wildcards || !(*keys) || !(*keywords) || !(*wildcards) )
    {
        sql = "select count(distinct name) from universe";
        db_query( &db, sql, NULL);
        sql = NULL;
    }
    else
    {
        where_str = NULL;

        while( *keys && *keywords && *wildcards )
        {
            tmp = NULL;
            if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 2) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(name like '%", *keywords, "%' or generic_name like  '%", *keywords, "%' or description like  '%", *keywords, "%' or keywords.kw_name like  '%", *keywords, "%' or keywords.kw_generic_name like  '%", *keywords, "%' or keywords.kw_fullname like  '%", *keywords, "%' or keywords.kw_comment like '%", *keywords, "%')", NULL );
                if( tmp )
                    free( tmp );
            }
            else if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 1) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(name = '", *keywords, "' or generic_name = '", *keywords, "' or keywords.kw_name = '", *keywords, "' or keywords.kw_generic_name = '", *keywords, "' or keywords.kw_fullname = '", *keywords, "')", NULL );
                if( tmp )
                    free( tmp );
            }
            else if( *keywords && *keys && *wildcards )
            {
                if( where_str )
                    tmp = where_str;

                if( strncmp( *keys, "kw_", 3 ) && strcmp( *keys, "language" ) )
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (" : "(", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }
                else
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (keywords." : "(keywords.", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }

                if( tmp )
                    free( tmp );
            }
            else
            {
                break;
            }
            keys++;
            keywords++;
            wildcards++;
        }

        if( where_str )
        {
            sql = util_strcat( "select count(distinct name) from universe left join keywords using (name)  where ", where_str, NULL );
            db_query( &db, sql, NULL );
            //puts( sql);

            free( where_str );
            where_str = NULL;
            free( sql );
            sql = NULL;
        }
    }

    db_fetch_num( &db );
    tmp =  db_get_value_by_index( &db, 0 );
    count = tmp ? atoi( tmp ) : 0;
    db_close( &db );

    return count;
}

int packages_has_installed( YPackageManager *pm, char *name, char *version )
{
    int     count, ret;
    char    *version_installed, *tmp;
    DB      db;

    if( !pm || !name )
        return -1;

    db_init( &db, pm->db_name, OPEN_READ );

    if( version )
    {
        version_installed = NULL;

        db_query( &db, "select version from world where name=?", name, NULL);

        if( db_fetch_num( &db ) )
        {
            version_installed = db_get_value_by_index( &db, 0 );
            if( version[0] == '>' || version[0] == '=' || version[0] == '!' || version[0] == '<')
            {
                if( version[1] == '=' )
                {
                    ret = packages_compare_version( version_installed, version + 2 );
                    switch( version[0] )
                    {
                        case '>':
                            ret = ret != -1;
                            break;

                        case '!':
                            ret = ret != 0;
                            break;

                        case '<':
                            ret = ret != 1;
                            break;

                        default:
                            ret = ret == 0;
                    }
                }
                else
                {
                    ret = packages_compare_version( version_installed, version + 1 );
                    switch( version[0] )
                    {
                        case '=':
                            ret = ret == 0;
                            break;

                        case '>':
                            ret = ret == 1;
                            break;

                        case '<':
                            ret = ret == -1;
                            break;

                        default:
                            ret = ret == 0;
                    }
                }
            }
            else
            {
                ret = packages_compare_version( version, version_installed ) == 0;
            }
        }
        else
        {
            ret = 0;
        }

    }
    else
    {
        db_query( &db, "select count(*) from world where name=?", name, NULL);
        db_fetch_num( &db );
        tmp =  db_get_value_by_index( &db, 0 );
        count = tmp ? atoi( tmp ) : 0;
        ret = count > 0;
    }


    db_close( &db );

    return ret;
}

int packages_exists( YPackageManager *pm, char *name, char *version )
{
    int     count, ret;
    char    *sql, *version_installed, *tmp;
    DB      db;

    if( !pm || !name )
        return -1;

    db_init( &db, pm->db_name, OPEN_READ );

    tmp = NULL;

    if( version )
    {
        version_installed = NULL;

        sql = "select version from universe where name=? limit 1";
        db_query( &db, sql, name, NULL );
        sql = NULL;

        ret = 0;
        while( db_fetch_num( &db ) )
        {
            version_installed = db_get_value_by_index( &db, 0 );
            if( version[0] == '>' || version[0] == '=' || version[0] == '!' || version[0] == '<' )
            {
                if( version[1] == '=' )
                {
                    ret = packages_compare_version( version_installed, version + 2 );
                    switch( version[0] )
                    {
                        case '>':
                            ret = ret != -1;
                            break;

                        case '!':
                            ret = ret != 0;
                            break;

                        case '<':
                            ret = ret != 1;
                            break;

                        default:
                            ret = ret == 0;
                    }
                }
                else
                {
                    ret = packages_compare_version( version_installed, version + 1 );
                    switch( version[0] )
                    {
                        case '=':
                            ret = ret == 0;
                            break;

                        case '>':
                            ret = ret == 1;
                            break;

                        case '<':
                            ret = ret == -1;
                            break;

                        default:
                            ret = ret == 0;
                    }
                }
            }
            else
            {
                ret = packages_compare_version( version, version_installed ) == 0;
            }

            if( ret > 0 )
                break;
        }
    }
    else
    {
        sql = "select count(*) from universe where name=?";
        db_query( &db, sql, name, NULL );
        sql = NULL;

        db_fetch_num( &db );
        tmp =  db_get_value_by_index( &db, 0 );
        count = tmp ? atoi( tmp ) : 0;
        ret = count > 0;
    }


    db_close( &db );

    return ret;
}

YPackage *packages_get_repo_package( YPackageManager *pm, char *name, int installed, char *repo )
{
    char                    *sql, *cur_key, *cur_value, **attr_keys_offset;
    char                    *attr_keys[] = { "name", "generic_name", "category", "priority", "version", "license", "homepage", "description", "uri", "sha", "size", "repo", "arch", "install", "exec", "data_count", "installed", "install_time", "can_update", "build_date", "packager", "source", NULL  }; 
    DB                      db;
    YPackage                *pkg = NULL;
    YPackageSource          *source;

    if( !pm || !name )
    {
        return NULL;
    }

    db_init( &db, pm->db_name, OPEN_READ );

    if( installed )
    {
        sql = "select * from world where name=? limit 1";
        db_query( &db, sql, name, NULL);
        sql = NULL;
    }
    else
    {
        sql = "select * from universe where name=? limit 1";
        db_query( &db, sql, name, NULL);
        sql = NULL;
    }

    if( db_fetch_assoc( &db ) )
    {
        pkg = (YPackage *)malloc( sizeof( YPackage ) );
        pkg->ht = hash_table_init( );

        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_add_data( pkg->ht, cur_key, cur_value );

            if( !strcmp( cur_key, "source" ) && cur_value )
            {
                source = dlist_head_data( pm->source_list );
                while( source )
                {

                    if( !strcmp( cur_value, source->source_name ) )
                    {
                        hash_table_add_data( pkg->ht, "package_dest", source->package_dest );
                        hash_table_add_data( pkg->ht, "source_uri", source->source_uri );
                        break;
                    }

                    source = dlist_next_data( pm->source_list );
                }
         
            }
        }
    }

    db_close( &db );

    return pkg;
}

YPackage *packages_get_package( YPackageManager *pm, char *name, int installed )
{
    return packages_get_repo_package( pm, name, installed, NULL );
}

/*
 * packages_get_package_from_ypk
 */
int packages_get_package_from_ypk( char *ypk_path, YPackage **package, YPackageData **package_data )
{
    return packages_get_info_from_ypk( ypk_path, package, package_data, NULL, NULL, NULL, NULL );
}


/*
 * packages_get_info_from_ypk
 */
int packages_get_info_from_ypk( char *ypk_path, YPackage **package, YPackageData **package_data, YPackageFile **package_file, char *install_script, char *desktop_file, char *icon )
{
    int                 i, ret, return_code = 0, cur_data_index, data_count;
    void                *pkginfo = NULL, *control = NULL, *filelist = NULL;
    size_t              pkginfo_len = 0, control_len = 0, filelist_len = 0;
    char                *cur_key, *cur_value, *cur_xpath, **attr_keys_offset, **attr_xpath_offset, *idx, *data_key, *package_name, *is_desktop, *tmp;
    char                *attr_keys[] = { "name", "exec", "generic_name", "category", "arch", "priority", "version", "install", "license", "homepage", "repo", "description", "sha", "size", "build_date", "packager", "uri", "data_count", "is_desktop", NULL  }; 
    char                *attr_xpath[] = { "//Package/@name", "//exec", "//genericname/keyword", "//category", "//arch", "//priority", "//version", "//install", "//license", "//homepage", "//repo", "//description/keyword", "//sha", "//size", "//build_date", "//packager", "//uri", "//data_count", "//genericname[@type='desktop']", NULL  }; 
    char                *data_attr_keys[] = { "data_name", "data_format", "data_size", "data_install_size", "data_depend", "data_bdepend", "data_recommended", "data_conflict", "data_replace", NULL  }; 
    char                *data_attr_xpath[] = { "name", "format", "size", "install_size", "depend", "bdepend", "recommended", "conflict", "replace", NULL  }; 
    xmlDocPtr           xmldoc = NULL;
    YPackage            *pkg = NULL;
    YPackageData        *pkg_data = NULL;
    YPackageFile        *pkg_file = NULL;

    if( access( ypk_path, R_OK ) )
        return -1;

    //unzip pkginfo
    ret = archive_extract_file2( ypk_path, "pkginfo", &pkginfo, &pkginfo_len );
    if( ret == -1 || pkginfo_len == 0 )
    {
        return_code = -1;
        goto exception_handler;
    }

    //get control info
    ret = archive_extract_file4( pkginfo, pkginfo_len, "control.xml", &control, &control_len );
    if( ret == -1 ||  control_len == 0 )
    {
        return_code = -2;
        goto exception_handler;
    }

	if( ( xmldoc = xpath_open2( control, control_len ) ) == (xmlDocPtr)-1 )
    {
        return_code = -3;
        goto exception_handler;
    }

    if( control )
    {
        free( control );
        control = NULL;
    }

    if( package || install_script || desktop_file || icon )
    {
        pkg = (YPackage *)malloc( sizeof( YPackage ) );
        pkg->ht = hash_table_init( );

        attr_keys_offset = attr_keys;
        attr_xpath_offset = attr_xpath;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_xpath = *attr_xpath_offset++;
            cur_value =  xpath_get_node( xmldoc, (xmlChar *)cur_xpath );

            if( strcmp( cur_key, "is_desktop" ) == 0 )
            {
                hash_table_add_data( pkg->ht, cur_key, cur_value ? "1" : "0" );
            }
            else
            {
                hash_table_add_data( pkg->ht, cur_key, cur_value );
            }

            if( cur_value )
                free( cur_value );
        }
        if( package )
            *package = pkg;
    }

    if( package_data )
    {
        if( package && packages_get_package_attr( (*package), "data_count") )
        {
            cur_value = packages_get_package_attr( (*package), "data_count");
            data_count =  cur_value ? atoi( cur_value ) : 1;
        }
        else
        {
            cur_value = xpath_get_node( xmldoc, (xmlChar *)"//data_count" );
            data_count =  cur_value ? atoi( cur_value ) : 1;
            free( cur_value );
        }
        data_count = data_count ? data_count : 1;

        pkg_data = (YPackageData *)malloc( sizeof( YPackageData ) );
        pkg_data->htl = hash_table_list_init( data_count );
        
        cur_data_index = 0;
        pkg_data->cnt = 0;

        data_key = (char *)malloc( 32 );
        for( i = 0; ; i++ )
        {
            idx = util_int_to_str( i );
            cur_value = xpath_get_node( xmldoc, (xmlChar *)util_strcat2( data_key, 32, "//data[@id='", idx, "']", NULL ) );
            if( !cur_value )
            {
                free( idx );
                break;
            }
            free( cur_value );

            attr_keys_offset = data_attr_keys;
            attr_xpath_offset = data_attr_xpath;
            while( (cur_key = *attr_keys_offset++) )
            {
                cur_xpath = *attr_xpath_offset++;
                cur_value = xpath_get_node( xmldoc, (xmlChar *)util_strcat2( data_key, 32, "//data[@id='", idx, "']/", cur_xpath, NULL ) );
                hash_table_list_add_data( pkg_data->htl, cur_data_index, cur_key, cur_value );
                if( cur_value  )
                    free( cur_value );
            }
            free( idx );
            cur_data_index++;
        }
        free( data_key );
        pkg_data->cnt = cur_data_index;

        if( pkg_data->cnt == 0 )
        {
            hash_table_list_cleanup( pkg_data->htl );
            free( pkg_data );
            *package_data =  NULL;
        }
        else
        {
            *package_data = pkg_data;
        }
    }

    //get filelist
    if( package_file )
    {
        ret = archive_extract_file4( pkginfo, pkginfo_len, "filelist", &filelist, &filelist_len );
        if( ret == -1 ||  filelist_len == 0)
        {
            if( filelist )
                free( filelist );

            return_code = -4;
            goto exception_handler;
        }

        if( filelist )
        {
            pkg_file = packages_get_package_file_from_str( filelist );
            free( filelist );
        }

        *package_file = pkg_file;
    }

    //extract install script
    if( install_script )
    {
        tmp = packages_get_package_attr( (*package), "install" );
        if( tmp )
        {
            ret = archive_extract_file3( pkginfo, pkginfo_len, tmp, install_script );
            tmp = NULL;
            if( ret == -1 )
            {
                return_code = -5;
                goto exception_handler;
            }
        }
    }

    //extract desktop file
    if( desktop_file )
    {
        is_desktop = packages_get_package_attr( (*package), "is_desktop" );
        if( is_desktop && is_desktop[0] == '1' )
        {
            package_name = packages_get_package_attr( (*package), "name");
            tmp = util_strcat( package_name, ".desktop", NULL );
            ret = archive_extract_file3( pkginfo, pkginfo_len, tmp, desktop_file );
            free( tmp );
            tmp = NULL;
            if( ret == -1 )
            {
                return_code = -6;
                goto exception_handler;
            }
        }
    }

    //extract icon
    if( icon )
    {
        package_name = packages_get_package_attr( (*package), "name");

        tmp = util_strcat( package_name, ".png", NULL );
        ret = archive_extract_file3( pkginfo, pkginfo_len, tmp, icon );
        free( tmp );
        tmp = NULL;
        if( ret == -1 )
        {
            tmp = util_strcat( package_name, ".svg", NULL );
            ret = archive_extract_file3( pkginfo, pkginfo_len, tmp, icon );
            free( tmp );
            tmp = NULL;
            if( ret == -1 )
            {
                tmp = util_strcat( package_name, ".jpg", NULL );
                ret = archive_extract_file3( pkginfo, pkginfo_len, tmp, icon );
                free( tmp );
                tmp = NULL;
                if( ret == -1 )
                {
                    tmp = util_strcat( package_name, ".xpm", NULL );
                    ret = archive_extract_file3( pkginfo, pkginfo_len, tmp, icon );
                    free( tmp );
                    tmp = NULL;
                }
            }
        }
    }


    //clean up
    if( pkginfo )
    {
        free( pkginfo );
        pkginfo = NULL;
    }

    if( xmldoc )
        xmlFreeDoc( xmldoc );

	xmlCleanupParser();

    if( !package && pkg )
    {
        packages_free_package( pkg );
        pkg = NULL;
    }

    return 0;

exception_handler:
    if( pkginfo )
        free( pkginfo );

    if( control )
        free( control );

    if( xmldoc )
        xmlFreeDoc( xmldoc );

	xmlCleanupParser();

    if( pkg )
    {
        packages_free_package( pkg );
        pkg = NULL;
    }

    if( pkg_data )
    {
        packages_free_package_data( pkg_data );
        pkg_data = NULL;
    }


    if( pkg_file )
    {
        packages_free_package_file( pkg_file );
        pkg_file = NULL;
    }

    if( package )
        *package = NULL;

    if( package_data )
        *package_data = NULL;

    if( package_file )
        *package_file = NULL;

    return return_code;
}

/*
 * packages_get_package_attr
 */
char *packages_get_package_attr( YPackage *pkg, char *key )
{
    if( !pkg || !key )
        return NULL;

    return hash_table_get_data( pkg->ht, key );
}

char *packages_get_package_attr2( YPackage *pkg, char *key )
{
    char *result;

    if( !pkg || !key )
        return NULL;

    result = hash_table_get_data( pkg->ht, key );
    return result ? result : "";
}

/*
 * packages_free_package
 */
void packages_free_package( YPackage *pkg )
{
    if( !pkg )
        return;

    hash_table_cleanup( pkg->ht );
    free( pkg );
}

/*
 * packages_get_package_data
 */
YPackageData *packages_get_package_data( YPackageManager *pm, char *name, int installed )
{
    int                     data_count, cur_data_index;
    char                    *sql, *tmp, *cur_key, *cur_value, **attr_keys_offset, *max_version, *ypk_source, *repo;
    char                    *attr_keys[] = { "name", "data_name", "data_format", "data_size", "data_install_size", "data_depend", "data_bdepend", "data_recommended", "data_conflict", "data_replace", NULL  }; 
    DB                      db;
    YPackageData            *pkg_data = NULL;

    if( !pm || !name )
    {
        return NULL;
    }

    db_init( &db, pm->db_name, OPEN_READ );

    if( installed )
    {
        sql = "select count(*) from world_data where name=?";
        db_query( &db, sql, name, NULL);
        sql = NULL;
        db_fetch_num( &db );
        tmp =  db_get_value_by_index( &db, 0 );
        data_count = tmp ? atoi( tmp ) : 0;
        db_cleanup( &db );

        sql = "select * from world_data where name=?";
        db_query( &db, sql, name, NULL);
        sql = NULL;
    }
    else
    {
        tmp = NULL;

        sql = "select version, source, repo from universe where name=? limit 1";
        db_query( &db, sql, name, NULL);
        sql = NULL;

        max_version = NULL;
        ypk_source =  NULL;
        repo =  NULL;
        if( db_fetch_num( &db ) )
        {
            max_version =  strdup( db_get_value_by_index( &db, 0 ) );
            ypk_source =  strdup( db_get_value_by_index( &db, 1 ) );
            repo =  strdup( db_get_value_by_index( &db, 2 ) );
        }
        db_cleanup( &db );

        sql =  "select count(*) from universe_data where name=? and version=? and source=? and repo=?";
        db_query( &db, sql, name, max_version, ypk_source, repo, NULL);
        sql = NULL;

        db_fetch_num( &db );
        tmp =  db_get_value_by_index( &db, 0 );
        data_count = tmp ? atoi( tmp ) : 0;
        db_cleanup( &db );

        sql = "select * from universe_data where name=? and version=? and source=? and repo=?";
        db_query( &db, sql, name, max_version, ypk_source, repo, NULL);
        sql = NULL;

        if( max_version )
        {
            free( max_version );
            max_version = NULL;
        }

        if( ypk_source )
        {
            free( ypk_source );
            ypk_source = NULL;
        }

        if( repo )
        {
            free( repo );
            repo = NULL;
        }
    }

    pkg_data = (YPackageData *)malloc( sizeof( YPackageData ) );
    pkg_data->htl = hash_table_list_init( data_count );

    cur_data_index = 0;
    pkg_data->cnt = 0;

    while( db_fetch_assoc( &db ) )
    {

        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_data->htl, cur_data_index, cur_key, cur_value );
        }
        cur_data_index++;
    }
    pkg_data->cnt = cur_data_index;

    db_close( &db );
    if( pkg_data->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_data->htl );
        free( pkg_data );
        return NULL;
    }

    return pkg_data;
}

/*
 * packages_get_package_data_attr
 */
char *packages_get_package_data_attr( YPackageData *pkg_data, int index, char *key )
{
    if( !pkg_data || !key )
        return NULL;

    return hash_table_list_get_data( pkg_data->htl, index, key );
}

char *packages_get_package_data_attr2( YPackageData *pkg_data, int index, char *key )
{
    char *result;

    if( !pkg_data || !key )
        return NULL;

    result = hash_table_list_get_data( pkg_data->htl, index, key );
    return result ? result : "";
}
/*
 * packages_free_package_data
 */
void packages_free_package_data( YPackageData *pkg_data )
{
    if( !pkg_data )
        return;

    hash_table_list_cleanup( pkg_data->htl );
    free( pkg_data );
}

/*
 * packages_get_package_file
 */
YPackageFile *packages_get_package_file( YPackageManager *pm, char *name )
{
    int                     file_count, file_type, cur_file_index;
    char                    *cur_key, *cur_value, **attr_keys_offset, *tmp;
    char                    *attr_keys[] = { "name", "file", "type", "size", "perms", "uid", "gid", "mtime", "replace", "replace_with", NULL  }; 
    DB                      db;
    YPackageFile            *pkg_file = NULL;

    if( !pm || !name )
    {
        return NULL;
    }


    db_init( &db, pm->db_name, OPEN_READ );

    //get file count
    db_query( &db, "select count(*) from world_file where name=?", name, NULL);
    db_fetch_num( &db );
    tmp =  db_get_value_by_index( &db, 0 );
    file_count = tmp ? atoi( tmp ) : 0;
    db_cleanup( &db );

    //get file info
    db_query( &db, "select * from world_file where name=?", name, NULL);

    pkg_file = (YPackageFile *)malloc( sizeof( YPackageFile ) );
    pkg_file->htl = hash_table_list_init( file_count );

    cur_file_index = 0;
    pkg_file->cnt = 0;
    pkg_file->cnt_file = 0;
    pkg_file->cnt_dir = 0;
    pkg_file->cnt_link = 0;
    pkg_file->size = 0;

    while( db_fetch_assoc( &db ) )
    {
        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_file->htl, cur_file_index, cur_key, cur_value );

            file_type = 1;
            //counter
            if( cur_key[0] == 't' && cur_value ) //type
            {
                switch( cur_value[0] )
                {
                    case 'F':
                        pkg_file->cnt_file++;
                        file_type = 1;
                        break;
                    case 'D':
                        pkg_file->cnt_dir++;
                        file_type = 2;
                        break;
                    case 'S':
                        pkg_file->cnt_link++;
                        file_type = 3;
                        break;
                }
            }
            else if( cur_key[0] == 's' && file_type == 1 ) //size
            {
                pkg_file->size += atoi( cur_value );
            }
        }
        cur_file_index++;
    }
    pkg_file->cnt = cur_file_index;

    db_close( &db );
    if( pkg_file->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_file->htl );
        free( pkg_file );
        return NULL;
    }

    pkg_file->size = pkg_file->size / 1024;
    return pkg_file;
}

/*
 * packages_get_package_file_from_str
 */
YPackageFile *packages_get_package_file_from_str( char *filelist )
{
    int                 file_count, cur_file_index;
    char                *saveptr;
    char                *pos, *list_line, *cur_key, *cur_value, **attr_keys_offset;
    char                *attr_keys[] = { "type", "file", "size", "perms", "uid", "gid", "mtime", "extra", NULL  }; 
    YPackageFile        *pkg_file = NULL;

    if( !filelist )
        return NULL;

    pos = strrchr( filelist, '\n' );
    if( !pos )
        return NULL;

    if( !pos[1] )
    {
        *pos = 0;
        pos = strrchr( filelist, '\n' );
        if( !pos )
            return NULL;
    }

    if( pos[1] != 'I' )
    {
        return NULL;
    }

    pkg_file = (YPackageFile *)malloc( sizeof( YPackageFile ) );

    strtok_r( pos + 1, ",", &saveptr );
    pkg_file->cnt_file = atoi( strtok_r( NULL, ",", &saveptr ) );
    pkg_file->cnt_dir = atoi( strtok_r( NULL, ",", &saveptr ) );
    pkg_file->cnt_link = atoi( strtok_r( NULL, ",", &saveptr ) );
    strtok_r( NULL, ",", &saveptr );
    pkg_file->size = atoi( strtok_r( NULL, ",", &saveptr ) );

    file_count = pkg_file->cnt_file + pkg_file->cnt_dir + pkg_file->cnt_link;
    pkg_file->htl = hash_table_list_init( file_count );

    cur_file_index = 0;
    pkg_file->cnt = 0;

    list_line = util_mem_gets( filelist );
    while( list_line )
    {
        if( cur_file_index + 1 > file_count  || list_line[0] == 'I' )
        {
            free( list_line );
            break;
        }

        if( list_line[0] == 'F' ||  list_line[0] == 'D' ||  list_line[0] == 'S'  )
        {
            attr_keys_offset = attr_keys;
            cur_value = strtok_r( list_line, ",", &saveptr );
            while( (cur_key = *attr_keys_offset++) )
            {
                hash_table_list_add_data( pkg_file->htl, cur_file_index, cur_key, cur_value );
                cur_value =  strtok_r( NULL, ",", &saveptr );
            }
            cur_file_index++;
        }

        free( list_line );
        list_line = util_mem_gets( NULL );
    }
    pkg_file->cnt = cur_file_index;

    if( !pkg_file )
        return NULL;

    if( pkg_file->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_file->htl );
        free( pkg_file );
        return NULL;
    }

    return pkg_file;
}

/*
 * packages_get_package_file_from_ypk
 */
YPackageFile *packages_get_package_file_from_ypk( char *ypk_path )
{
    int                 ret;
    size_t              pkginfo_len = 0, filelist_len = 0;
    void                *pkginfo = NULL, *filelist = NULL;
    YPackageFile        *pkg_file = NULL;

    if( access( ypk_path, R_OK ) )
        return NULL;


    //unzip info
    ret = archive_extract_file2( ypk_path, "pkginfo", &pkginfo, &pkginfo_len );
    if( ret == -1 || pkginfo_len == 0 )
    {
        goto exception_handler;
    }

    ret = archive_extract_file4( pkginfo, pkginfo_len, "filelist", &filelist, &filelist_len );
    if( ret == -1 ||  filelist_len == 0)
    {
        goto exception_handler;
    }

    pkg_file = packages_get_package_file_from_str( filelist );

exception_handler:
    if( pkginfo )
        free( pkginfo );

    if( filelist )
        free( filelist );

    return pkg_file;
}

/*
 * packages_get_package_file_attr
 */
char *packages_get_package_file_attr( YPackageFile *pkg_file, int index, char *key )
{
    return hash_table_list_get_data( pkg_file->htl, index, key );
}

char *packages_get_package_file_attr2( YPackageFile *pkg_file, int index, char *key )
{
    char *result;

    result = hash_table_list_get_data( pkg_file->htl, index, key );
    return result ? result : "";
}

/*
 * packages_free_package_file
 */
void packages_free_package_file( YPackageFile *pkg_file )
{
    if( !pkg_file )
        return;

    hash_table_list_cleanup( pkg_file->htl );
    free( pkg_file );
}


YPackageReplaceFileList *packages_get_replace_file_list( YPackageManager *pm, YPackageData *pkg_data, YPackageFile *pkg_file )
{
    int                 i, j, k;
    char                *replace, *tmp, *token, *saveptr, *file, *type, *replace_with;
    YPackageFile        *pkg_file2;
    YPackageReplaceFileList *list;

    if( !pkg_data || !pkg_file )
        return NULL;

    list = NULL;

    for( i = 0; i < pkg_data->cnt; i++ )
    {
        tmp = packages_get_package_data_attr( pkg_data, i, "data_replace");
        if( tmp )
        {
            replace = strdup( tmp );
            tmp = NULL;
            token = strtok_r( replace, " ,", &saveptr );
            while( token )
            {
                if( packages_has_installed( pm, token, NULL ) )
                {
                    pkg_file2 = packages_get_package_file( pm, token );

                    for( j = 0; j < pkg_file2->cnt; j++ )
                    {
                        replace_with = packages_get_package_file_attr( pkg_file2, j, "replace_with");
                        if( replace_with && strlen( replace_with ) > 0 )
                            continue;

                        file = packages_get_package_file_attr( pkg_file2, j, "file");
                        type = packages_get_package_file_attr( pkg_file2, j, "type");
                        if( file && type && ( type[0] == 'F' || type[0] == 'S' ) )
                        {
                            for( k = 0; k < pkg_file->cnt; k++ )
                            {
                                if( !strcmp( file, packages_get_package_file_attr( pkg_file, k, "file" ) ) )
                                {

                                    if( !list )
                                    {
                                        list = (YPackageReplaceFileList *)malloc( sizeof( YPackageReplaceFileList ) );
                                        list->max_cnt = pkg_file->cnt;
                                        list->cur_cnt = 0;
                                        list->htl = hash_table_list_init( pkg_file->cnt );
                                    }

                                    if( list->cur_cnt >= list->max_cnt )
                                        break;

                                    hash_table_list_add_data( list->htl, list->cur_cnt, "replace", token );
                                    hash_table_list_add_data( list->htl, list->cur_cnt, "file", file );
                                    list->cur_cnt++;
                                    break;
                                }
                            } //for k
                        }
                    } //for j
                    packages_free_package_file( pkg_file2 );
                    pkg_file2 = NULL;
                }
                token = strtok_r( NULL, " ,", &saveptr );
            }
            free( replace );
        }
    }

    return list;
}

void packages_free_replace_file_list( YPackageReplaceFileList *list )
{
    if( !list )
        return;

    hash_table_list_cleanup( list->htl );
    free( list );
}


YPackageList *packages_get_installed_list( YPackageManager *pm, int limit, int offset, char *keys[], char *keywords[], int wildcards[] )
{
    int                     cur_pkg_index;
    char                    *sql, *where_str, *offset_str, *limit_str, *cur_key, *cur_value, **attr_keys_offset, *tmp;
    char                    *attr_keys[] = { "name", "generic_name", "category", "priority", "version", "license", "description", "size", "repo", "exec", "install_time", "installed", "can_update", "homepage", "build_date", "packager", "language", "kw_name", "kw_generic_name", "kw_fullname", "kw_comment", NULL  }; 
    DB                      db;
    YPackageList            *pkg_list;


    if( offset < 0 )
        offset = 0;

    if( limit < 1 )
        limit = 5;

    where_str = NULL;

    offset_str = util_int_to_str( offset );
    limit_str = util_int_to_str( limit );

    db_init( &db, pm->db_name, OPEN_READ );
    if( !keys || !keywords || !wildcards || !(*keys) || !(*keywords) || !(*wildcards) )
    {
        sql = util_strcat( "select * from world order by install_time desc limit ? offset ?", NULL );
        db_query( &db, sql, limit_str, offset_str, NULL);
        free( sql );
    }
    else
    {
        while( *keys && *keywords && *wildcards )
        {
            tmp = NULL;

            if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 2) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "( world.name like '%", *keywords, "%' or world.generic_name like  '%", *keywords, "%' or world.description like  '%", *keywords, "%' or keywords.kw_name like  '%", *keywords, "%' or keywords.kw_generic_name like  '%", *keywords, "%' or keywords.kw_fullname like  '%", *keywords, "%' or keywords.kw_comment like '%", *keywords, "%')", NULL );
                if( tmp )
                    free( tmp );
                tmp = NULL;
            }
            else if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 1) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(world.name = '", *keywords, "' or world.generic_name = '", *keywords, "' or keywords.kw_name = '", *keywords, "' or keywords.kw_generic_name = '", *keywords, "' or keywords.kw_fullname = '", *keywords, "')", NULL );
                if( tmp )
                    free( tmp );
                tmp = NULL;
            }
            else if( *keywords && *keys && *wildcards )
            {
                if( where_str )
                    tmp = where_str;

                if( strncmp( *keys, "kw_", 3 ) && strcmp( *keys, "language" ) )
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (" : "(world.", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }
                else
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (keywords." : "(keywords.", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }

                if( tmp )
                    free( tmp );
                tmp = NULL;
            }
            else
            {
                break;
            }

            keys++;
            keywords++;
            wildcards++;
        }

        if( where_str )
        {
            sql = util_strcat( "select * from world left join keywords using (name) where ", where_str, " group by name order by install_time desc limit ? offset ?", NULL );
            //printf( "%s\n", sql );
            db_query( &db, sql, limit_str, offset_str, NULL );

            free( where_str );
            free( sql );
        }
    }
    free( limit_str );
    free( offset_str );

    pkg_list = (YPackageList *)malloc( sizeof( YPackageList ) );
    pkg_list->htl = hash_table_list_init( limit );

    cur_pkg_index = 0;
    pkg_list->cnt = 0;

    while( db_fetch_assoc( &db ) )
    {

        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_list->htl, cur_pkg_index, cur_key, cur_value );
        }
        cur_pkg_index++;
    }
    pkg_list->cnt = cur_pkg_index;

    db_close( &db );

    if( pkg_list->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_list->htl );
        free( pkg_list );
        return NULL;
    }

    return pkg_list;
}

/*
 * packages_get_list
 */
YPackageList *packages_get_list( YPackageManager *pm, int limit, int offset, char *keys[], char *keywords[], int wildcards[], int installed )
{
    int                     cur_pkg_index;
    char                    *sql, *where_str, *offset_str, *limit_str, *cur_key, *cur_value, **attr_keys_offset, *tmp;
    char                    *attr_keys[] = { "name", "generic_name", "category", "priority", "version", "license", "description", "size", "repo", "exec", "install_time", "installed", "can_update", "homepage", "build_date", "packager", "language", "kw_name", "kw_generic_name", "kw_fullname", "kw_comment", NULL  }; 
    DB                      db;
    YPackageList            *pkg_list;


    if( installed )
        return packages_get_installed_list( pm, limit, offset, keys, keywords, wildcards );

    if( offset < 0 )
        offset = 0;

    if( limit < 1 )
        limit = 5;


    where_str = NULL;

    offset_str = util_int_to_str( offset );
    limit_str = util_int_to_str( limit );

    db_init( &db, pm->db_name, OPEN_READ );

    tmp = NULL;

    if( !keys || !keywords || !wildcards || !(*keys) || !(*keywords) || !(*wildcards) )
    {
        sql = util_strcat( " select * from universe left join keywords using (name) group by name limit ? offset ?", NULL );

        db_query( &db, sql, limit_str, offset_str, NULL);
        free( sql );
        sql = NULL;
    }
    else
    {
        while( *keys && *keywords && *wildcards )
        {
            tmp = NULL;

            if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 2) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(name like '%", *keywords, "%' or generic_name like  '%", *keywords, "%' or description like  '%", *keywords, "%' or keywords.kw_name like  '%", *keywords, "%' or keywords.kw_generic_name like  '%", *keywords, "%' or keywords.kw_fullname like  '%", *keywords, "%' or keywords.kw_comment like '%", *keywords, "%')", NULL );
                if( tmp )
                    free( tmp );
                tmp = NULL;
            }
            else if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 1) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(name = '", *keywords, "' or generic_name = '", *keywords, "' or keywords.kw_name = '", *keywords, "' or keywords.kw_generic_name = '", *keywords, "' or keywords.kw_fullname = '", *keywords, "')", NULL );
                if( tmp )
                    free( tmp );
                tmp = NULL;
            }
            else if( *keywords && *keys && *wildcards )
            {
                if( where_str )
                    tmp = where_str;

                if( strncmp( *keys, "kw_", 3 ) && strcmp( *keys, "language" ) )
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (" : "(", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }
                else
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (keywords." : "(keywords.", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }

                if( tmp )
                    free( tmp );
                tmp = NULL;
            }
            else
            {
                break;
            }

            keys++;
            keywords++;
            wildcards++;
        }

        if( where_str )
        {
            sql = util_strcat( "select * from universe left join keywords using (name) where ", where_str, " group by name limit ? offset ?", NULL );
            free( where_str );
            where_str = NULL;
            //puts(sql);
        }
        else
        {
            sql = util_strcat( " select * from universe left join keywords using (name) group by name limit ? offset ?", NULL );
        }
        db_query( &db, sql, limit_str, offset_str, NULL );
        free( sql );
        sql = NULL;
    }
    free( limit_str );
    free( offset_str );

    pkg_list = (YPackageList *)malloc( sizeof( YPackageList ) );
    pkg_list->htl = hash_table_list_init( limit );

    cur_pkg_index = 0;
    pkg_list->cnt = 0;

    while( db_fetch_assoc( &db ) )
    {
        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_list->htl, cur_pkg_index, cur_key, cur_value );
        }
        cur_pkg_index++;
    }
    pkg_list->cnt = cur_pkg_index;

    db_close( &db );

    if( pkg_list->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_list->htl );
        free( pkg_list );
        return NULL;
    }

    return pkg_list;
}

/*
 * packages_get_list2
 */
YPackageList *packages_get_list2( YPackageManager *pm, int page_size, int page_no, char *keys[], char *keywords[], int wildcards[], int installed )
{
    int                     offset;
    YPackageList            *pkg_list;


    if( page_size < 1 )
        page_size = 5;

    if( page_no < 1 )
        page_no = 1;

    offset = ( page_no - 1 ) * page_size;

    pkg_list = packages_get_list( pm, page_size, offset, keys, keywords, wildcards, installed );

    return pkg_list;
}

YPackageList *packages_search_world_data( YPackageManager *pm, int limit, int offset, char *key, char *keyword )
{    
    int                     cur_pkg_index;
    char                    *sql, *offset_str, *limit_str, *cur_key, *cur_value, **attr_keys_offset;
    char                    *attr_keys[] = { "name", "version", NULL  }; 
    DB                      db;
    YPackageList            *pkg_list;

    if( !key || !keyword )
        return NULL;

    if( offset < 0 )
        offset = 0;

    if( limit < 1 )
        limit = 5;

    offset_str = util_int_to_str( offset );
    limit_str = util_int_to_str( limit );

    db_init( &db, pm->db_name, OPEN_READ );

    sql = util_strcat( "select name, version from world_data where ", key, " = '", keyword, "' union select name, version from world_data where ", key, " like '", keyword, ",%' union select name, version from world_data where ", key, " like '%,", keyword, "' union select name, version from world_data where ", key, " like '%,", keyword, ",%' union select name, version from world_data where ", key, " like '", keyword, "(%' union select name, version from world_data where ", key, " like '%,", keyword, "(%' limit ? offset ?", NULL );
    db_query( &db, sql, limit_str, offset_str, NULL);
    free( sql );
    free( limit_str );
    free( offset_str );

    pkg_list = (YPackageList *)malloc( sizeof( YPackageList ) );
    pkg_list->htl = hash_table_list_init( limit );

    cur_pkg_index = 0;
    pkg_list->cnt = 0;

    while( db_fetch_assoc( &db ) )
    {
        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_list->htl, cur_pkg_index, cur_key, cur_value );
        }
        cur_pkg_index++;
    }
    pkg_list->cnt = cur_pkg_index;

    db_close( &db );

    if( pkg_list->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_list->htl );
        free( pkg_list );
        return NULL;
    }

    return pkg_list;

}

/*
 * packages_get_list_by_recommended
 */
YPackageList *packages_get_list_by_recommended( YPackageManager *pm, int limit, int offset, char *recommended, int installed )
{
    return packages_search_world_data( pm, limit, offset, "data_recommended", recommended );
}

/*
 * packages_get_list_by_conflict
 */
YPackageList *packages_get_list_by_conflict( YPackageManager *pm, int limit, int offset, char *conflict, int installed )
{
    return packages_search_world_data( pm, limit, offset, "data_conflict", conflict );
}

/*
 * packages_get_list_by_depend
 */
YPackageList *packages_get_list_by_depend( YPackageManager *pm, int limit, int offset, char *depend, int installed )
{
    return packages_search_world_data( pm, limit, offset, "data_depend", depend );
}

/*
 * packages_get_list_by_bdepend
 */
YPackageList *packages_get_list_by_bdepend( YPackageManager *pm, int limit, int offset, char *bdepend, int installed )
{
    return packages_search_world_data( pm, limit, offset, "data_bdepend", bdepend );
}

/*
 * packages_get_list_by_file
 */
YPackageList *packages_get_list_by_file( YPackageManager *pm, int limit, int offset, char *file, int absolute )
{    
    int                     cur_pkg_index;
    char                    *sql, *offset_str, *limit_str, *cur_key, *cur_value, **attr_keys_offset;
    char                    *attr_keys[] = { "name", "generic_name", "category", "priority", "version", "license", "description", "size", "file", "type", "extra", NULL  }; 
    DB                      db;
    YPackageList            *pkg_list;

    if( !file )
        return NULL;

    if( offset < 0 )
        offset = 0;

    if( limit < 1 )
        limit = 5;

    offset_str = util_int_to_str( offset );
    limit_str = util_int_to_str( limit );

    db_init( &db, pm->db_name, OPEN_READ );
    if( absolute )
    {
        db_query( &db, "select distinct * from world,world_file where world.name=world_file.name  and file=? limit ? offset ?", file, limit_str, offset_str, NULL);
    }
    else
    {
        sql = util_strcat( "select distinct * from world,world_file where world.name=world_file.name  and file like ", file[0] == '/' ? "'%'" : "'%/'", "||? limit ? offset ?", NULL );
        db_query( &db, sql, file, limit_str, offset_str, NULL);
        free( sql );
        sql = NULL;
    }
    free( limit_str );
    free( offset_str );

    pkg_list = (YPackageList *)malloc( sizeof( YPackageList ) );
    pkg_list->htl = hash_table_list_init( limit );

    cur_pkg_index = 0;
    pkg_list->cnt = 0;
    while( db_fetch_assoc( &db ) )
    {
        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_list->htl, cur_pkg_index, cur_key, cur_value );
        }
        cur_pkg_index++;
    }
    pkg_list->cnt = cur_pkg_index;

    db_close( &db );

    if( pkg_list->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_list->htl );
        free( pkg_list );
        return NULL;
    }

    return pkg_list;
}

/*
 * packages_free_list
 */
void packages_free_list( YPackageList *pkg_list )
{
    if( !pkg_list )
        return;

    hash_table_list_cleanup( pkg_list->htl );
    free( pkg_list );
}

/*
 * packages_get_list_attr
 */
char *packages_get_list_attr( YPackageList *pkg_list, int index, char *key )
{
    return hash_table_list_get_data( pkg_list->htl, index, key );
}

char *packages_get_list_attr2( YPackageList *pkg_list, int index, char *key )
{
    char *result;

    result = hash_table_list_get_data( pkg_list->htl, index, key );
    return result ? result : "";
}


int packages_download_progress_callback( void *arg,double dltotal, double dlnow, double ultotal, double ulnow )
{
    YPackageDCB         *cb;

    if( !arg )
        return 0;

    cb = (YPackageDCB *)arg;
    if( cb->dcb )
        return cb->dcb( cb->dcb_arg, cb->package_name, dltotal, dlnow );
    else if( cb->pcb )
        return cb->pcb( cb->pcb_arg, cb->package_name, 2, dlnow / dltotal, NULL );
    else
        return 0;

    //return cb->cb( dcb->arg, dltotal, dlnow );
}

/*
 * packages_download_package
 */
int packages_download_package( YPackageManager *pm, char *package_name, char *url, char *dest, int force, ypk_download_callback dcb, void *dcb_arg, ypk_progress_callback pcb, void *pcb_arg  )
{
    int                 return_code;
    //char                *target_url, *package_url, *package_path;
    char                *target_url, *package_path;
    DownloadFile        file;
    YPackage            *pkg;
    YPackageDCB         *cb = NULL;

    if( (!url || !dest) && (!pm || !package_name) )
        return -1;

    return_code = 0;
    target_url = NULL;
    package_path = NULL;
    if( url && dest )
    {
        target_url = strdup( url );
        package_path = strdup( dest );
        pkg = NULL;
    }
    else
    {
        return -1;
    }
    /*
    else
    {
        pkg = packages_get_package( pm, package_name, 0 );
        if( !pkg )
        {
            return -2;
        }


        package_url = packages_get_package_attr( pkg, "uri" );
        if( !package_url )
        {
            return_code = -3;
            goto exception_handler;
        }

        package_path = util_strcat( pm->package_dest, "/", basename( package_url ), NULL );
        target_url = util_strcat( pm->source_uri, "/", package_url, NULL );
    }
    */
        //printf( "Path: %s\n", package_path );

    if( force || access( package_path, R_OK ) )
    {
        file.file = package_path;
        file.stream = NULL;
        cb = NULL;
        if( dcb || pcb )
        {
            cb = (YPackageDCB *)malloc( sizeof( YPackageDCB ) );
            if( cb )
            {
                memset( cb, 0, sizeof( YPackageDCB ) );

                cb->package_name = package_name;
                if( dcb )
                {
                    cb->dcb = dcb;
                    cb->dcb_arg = dcb_arg;
                }

                if( pcb )
                {
                    cb->pcb = pcb;
                    cb->pcb_arg = pcb_arg;
                }


                file.cb = packages_download_progress_callback;
                file.cb_arg = (void *)cb;
            }
            else
            {
                file.cb = NULL;
                file.cb_arg = NULL;
            }
        }
        else
        {
            file.cb = NULL;
            file.cb_arg = NULL;
        }

        if( download_file( target_url, &file ) != 0 )
        {
            if( file.stream )
                fclose( file.stream );
            return_code = -4;
            goto exception_handler;
        }
        fclose( file.stream );
        if( cb )
        {
            free( cb );
            cb = NULL;
        }
    }

exception_handler:
    if( target_url )
        free( target_url );

    if( package_path )
        free( package_path );

    if( pkg )
        packages_free_package( pkg );

    if( cb )
        free( cb );
    return return_code;
}


/*
 * packages_install_package
 */
int packages_install_package( YPackageManager *pm, char *package_name, char *version, ypk_progress_callback cb, void *cb_arg  )
{
    int                 return_code;
    char                *source_uri = NULL, *package_dest = NULL, *target_url = NULL, *package_url = NULL, *package_path = NULL, *pkg_sha = NULL, *ypk_sha = NULL;
    YPackage            *pkg;

    if( !package_name )
        return -1;

    if( cb )
    {
        cb( cb_arg, package_name, 0, 2, "Install\n" );
        cb( cb_arg, package_name, 1, -1, "Initializing\n" );
    }

    return_code = 0;
    if( !packages_exists( pm, package_name, version ) )
    {
        return -2;
    }

    pkg = packages_get_package( pm, package_name, 0 );
    if( !pkg )
    {
        return -2;
    }

    package_url = packages_get_package_attr( pkg, "uri" );
    if( !package_url )
    {
        return_code = -3;
        goto exception_handler;
    }

    package_dest = packages_get_package_attr( pkg, "package_dest" );
    if( !package_dest )
    {
        return_code = -3;
        goto exception_handler;
    }

    source_uri = packages_get_package_attr( pkg, "source_uri" );
    if( !source_uri )
    {
        return_code = -3;
        goto exception_handler;
    }

    if( cb )
    {
        cb( cb_arg, package_name, 1, 1, NULL );
    }

    package_path = util_strcat( package_dest, "/", basename( package_url ), NULL );
    target_url = util_strcat( source_uri, "/", package_url, NULL );
    pkg_sha = packages_get_package_attr( pkg, "sha" );

    if( !access( package_path, F_OK ) )
    {
        ypk_sha = util_sha1( package_path );

        if( ypk_sha && strncmp( pkg_sha, ypk_sha, 41 ) )
        {
            if( packages_download_package( NULL, package_name, target_url, package_path, 1, NULL, NULL, cb, cb_arg  ) < 0 )
            {
                return -4;
                goto exception_handler;
            }

            free( ypk_sha );

            ypk_sha = util_sha1( package_path );
            if( ypk_sha && strncmp( pkg_sha, ypk_sha, 41 ) )
            {
                return -4;
                goto exception_handler;
            }
        }
    }
    else
    {
        if( packages_download_package( NULL, package_name, target_url, package_path, 0, NULL, NULL, cb, cb_arg  ) < 0 )
        {
            return -4;
            goto exception_handler;
        }

        ypk_sha = util_sha1( package_path );
        if( ypk_sha && strncmp( pkg_sha, ypk_sha, 41 ) )
        {
            return -4;
            goto exception_handler;
        }
    }

    if( packages_install_local_package( pm, package_path, "/", 0, cb, cb_arg ) )
    {
        return_code = -5;
    }

exception_handler:
    if( package_path )
        free( package_path );

    if( target_url )
        free( target_url );

    if( ypk_sha )
        free( ypk_sha );

    packages_free_package( pkg );
    return return_code;
}


int packages_get_depend_list_recursively( YPackageManager *pm, YPackageChangeList **depend_list_p, YPackageChangeList **missing_list_p,  char *package_name, char *version, char *skip, int self_type )
//YPackageChangeList *packages_get_depend_list_recursively( YPackageManager *pm, char *package_name, char *version, char *skip, int self_type )
{
    int             i, err;
    char            *token, *saveptr, *depend, *recommended, *version2, *tmp, *tmp2;
    YPackageData    *pkg_data;
    YPackageChangeList    *list, *sub_list, *missing_list;

    if( !depend_list_p || !package_name )
        return -1;

    err = 0;

    list = NULL;
    sub_list = NULL;
    missing_list = NULL;

    if( (pkg_data = packages_get_package_data( pm, package_name, 0 )) )
    {
        for( i = 0; i < pkg_data->cnt; i++ )
        {
            depend = packages_get_package_data_attr( pkg_data, i, "data_depend");
            if( depend )
            {
                version2 = NULL;
                token = strtok_r( depend, " ,", &saveptr );
                while( token )
                {
                    tmp = util_strcat( token, NULL );
                    if( (version2 = strchr( tmp, '(' )) )
                    {
                        *version2++ = 0;

                        while( (*version2 == ' ') )
                            version2++;

                        if( (tmp2 = strchr( version2, ')' )) )
                            *tmp2 = 0;
                    }

                    if( skip && tmp && !strcmp( skip, tmp ) )
                    {
                        ;
                    }
                    else if( !packages_exists( pm, tmp, version2 ) )
                    {
                        err = -2;
                        if( !missing_list )
                        {
                            missing_list = dlist_init();
                        }
                        packages_clist_append( missing_list, tmp, version2, 0, 2 );
                    }
                    else if( !packages_has_installed( pm, tmp, version2 ) )
                    {
                        if( !list )
                        {
                            list = dlist_init();
                            packages_clist_append( list, util_strcat( "{", package_name, NULL ), NULL, 0, 7 );
                        }

                        err = packages_get_depend_list_recursively( pm, &sub_list, &missing_list, tmp, version2, package_name, 2 );
                        if( !err )
                        {
                            dlist_cat( list, sub_list );
                            dlist_cleanup( sub_list, packages_free_change_package );
                            sub_list = NULL;
                        }
                    }

                    free( tmp );
                    version2 = NULL;
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }

            //depend --> self --> recommended
            if( self_type )
            {
                if( !list )
                {
                    list = dlist_init();
                    packages_clist_append( list, util_strcat( "{", package_name, NULL ), NULL, 0, 7 );
                }
                else if( self_type == 2 )
                {
                    self_type = 6;
                }

                packages_clist_append( list, package_name, version, 0, self_type );
            }

            recommended = packages_get_package_data_attr( pkg_data, i, "data_recommended");
            if( recommended )
            {

                version2 = NULL;
                token = strtok_r( recommended, " ,", &saveptr );
                while( token )
                {
                    tmp = util_strcat( token, NULL );
                    if( (version2 = strchr( tmp, '(' )) )
                    {
                        *version2++ = 0;

                        while( *version2 == ' ' )
                            version2++;

                        if( (tmp2 = strchr( version2, ')' )) )
                            *tmp2 = 0;
                    }

                    if( skip && tmp && !strcmp( skip, tmp ) )
                    {
                        ;
                    }
                    else if( !packages_exists( pm, tmp, version2 ) )
                    {
                        /*
                        err = -2;
                        if( !missing_list )
                        {
                            missing_list = dlist_init();
                        }
                        packages_clist_append( missing_list, tmp, version2, 0, 2 );
                        */
                        ;
                    }
                    else if( !packages_has_installed( pm, tmp, version2 ) )
                    {
                        if( !list )
                            list = dlist_init();


                        //err = packages_get_depend_list_recursively( pm, &sub_list, &missing_list, tmp, version2, package_name, 3 );
                        if( !packages_get_depend_list_recursively( pm, &sub_list, NULL, tmp, version2, package_name, 3 ) )
                        {
                            dlist_cat( list, sub_list );
                            dlist_cleanup( sub_list, packages_free_change_package );
                            sub_list = NULL;
                        }
                    }
                    free( tmp );
                    version2 = NULL;
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }

        }
        packages_clist_append( list, util_strcat( package_name, "}", NULL ), NULL, 0, 8 );
        packages_free_package_data( pkg_data );
    }
    else
    {
        return -1;
    }

    if( list )
    {
        packages_clist_remove_duplicate_item( list );
    }
    *depend_list_p = list;

    if( missing_list )
    {
        if( missing_list_p )
        {
            if( !*missing_list_p )
                *missing_list_p = dlist_init();

            dlist_cat( *missing_list_p, missing_list );
        }

        dlist_cleanup( missing_list, packages_free_change_package );
        missing_list = NULL;
    }

    return err;
}

YPackageChangeList *packages_get_depend_list( YPackageManager *pm, char *package_name, char *version, char *skip )
{
    YPackageChangeList *list;
    packages_get_depend_list_recursively( pm, &list, NULL, package_name, version, skip, 0 );
    return list;
}

YPackageChangeList *packages_get_recommended_list( YPackageManager *pm, char *package_name, char *version )
{
    int             i;
    char            *token, *saveptr, *recommended, *version2, *tmp, *tmp2;
    YPackageData    *pkg_data;
    YPackageChangeList    *list, *sub_list;

    /*
    if( packages_has_installed( pm, package_name, NULL ) )
    {
        return NULL;
    }
    */
    
    list = NULL;

    if( (pkg_data = packages_get_package_data( pm, package_name, 0 )) )
    {
        for( i = 0; i < pkg_data->cnt; i++ )
        {
            recommended = packages_get_package_data_attr( pkg_data, i, "data_recommended");
            if( recommended )
            {

                version2 = NULL;
                token = strtok_r( recommended, " ,", &saveptr );
                while( token )
                {
                    tmp = util_strcat( token, NULL );
                    if( (version2 = strchr( tmp, '(' )) )
                    {
                        *version2++ = 0;

                        while( *version2 == ' ' )
                            version2++;

                        if( (tmp2 = strchr( version2, ')' )) )
                            *tmp2 = 0;
                    }

                    if( !packages_has_installed( pm, tmp, version2 ) )
                    {
                        if( !list )
                            list = dlist_init();

                        packages_clist_append( list, tmp, version2, 0, 3 );

                        sub_list = packages_get_recommended_list( pm, tmp, version2 );
                        if( sub_list )
                        {
                            dlist_cat( sub_list, list );
                            dlist_cleanup( list, packages_free_change_package );
                            list = sub_list;
                        }
                    }
                    free( tmp );
                    version2 = NULL;
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }
        }
        packages_free_package_data( pkg_data );
    }
    packages_clist_remove_duplicate_item( list );
    return list;
}

int packages_clist_package_cmp( void *pkg1, void *pkg2 )
{
     return strcmp( ((YPackageChangePackage *)pkg1)->name, ((YPackageChangePackage *)pkg2)->name );
}

int packages_clist_append( YPackageChangeList *list, char *name, char *version, int size, int type )
{
    YPackageChangePackage   *pkg;

    if( !list || !name )
        return -1;

    pkg =  (YPackageChangePackage *)malloc( sizeof( YPackageChangePackage ) );
    if( !pkg )
        return -2;

    pkg->name = strdup( name );
    if( !pkg->name )
    {
        free( pkg );
        return -3;
    }

    if( version )
    {
        pkg->version = strdup( version );
        if( !pkg->version )
        {
            free( pkg->name );
            free( pkg );
            return -4;
        }
    }
    else
    {
        pkg->version = NULL;
    }

    pkg->size = size;
    pkg->type = type;

    dlist_append( list, pkg );

    return 0;
}

YPackageChangeList *packages_clist_remove_duplicate_item( YPackageChangeList *list )
{
    dlist_strip_duplicate( list, packages_clist_package_cmp, packages_free_change_package );

    return list;
}


YPackageChangeList *packages_get_bdepend_list( YPackageManager *pm, char *package_name, char *version )
{
    int                     i;
    char                    *token, *version2, *saveptr, *depend, *dev, *bdepend, *tmp, *tmp2;
    YPackageData            *pkg_data;
    YPackageChangeList      *list, *sub_list;

    list = NULL;
    sub_list = NULL;

    if( (pkg_data = packages_get_package_data( pm, package_name, 0 )) )
    {
        for( i = 0; i < pkg_data->cnt; i++ )
        {
            depend = packages_get_package_data_attr( pkg_data, i, "data_depend");
            if( depend )
            {
                version2 = NULL;
                token = strtok_r( depend, " ,", &saveptr );
                while( token )
                {
                    tmp = strdup( token );
                    if( (version2 = strchr( tmp, '(' )) )
                    {
                        *version2++ = 0;

                        while( (*version2 == ' ') )
                            version2++;

                        if( (tmp2 = strchr( version2, ')' )) )
                            *tmp2 = 0;
                    }

                    dev = util_strcat( tmp, "-dev", NULL );
                    free( tmp );
                    if( !packages_exists( pm, dev, NULL ) )
                    {
                        free( dev );
                        token = strtok_r( NULL, " ,", &saveptr );
                        continue;
                    }

                    if( !packages_has_installed( pm, dev, version2 ) )
                    {
                        if( !list )
                            list = dlist_init();

                        packages_clist_append( list, dev, version2, 0, 2 );

                        sub_list = packages_get_depend_list( pm, dev, version2, NULL );
                        if( sub_list )
                        {
                            dlist_cat( sub_list, list );
                            dlist_cleanup( list, packages_free_change_package );
                            list = sub_list;
                        }

                    }
                    free( dev );
                    version2 = NULL;
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }

            bdepend = packages_get_package_data_attr( pkg_data, i, "data_bdepend");
            if( bdepend )
            {
                version2 = NULL;
                token = strtok_r( bdepend, " ,", &saveptr );
                while( token )
                {
                    tmp = strdup( token );
                    if( (version2 = strchr( tmp, '(' )) )
                    {
                        *version2++ = 0;

                        while( (*version2 == ' ') )
                            version2++;

                        if( (tmp2 = strchr( version2, ')' )) )
                            *tmp2 = 0;
                    }

                    if( !packages_has_installed( pm, tmp, version2 ) )
                    {
                        if( !list )
                            list = dlist_init();

                        packages_clist_append( list, token, version2, 0, 2 );

                        sub_list = packages_get_depend_list( pm, tmp, version2, NULL );
                        if( sub_list )
                        {
                            dlist_cat( sub_list, list );
                            dlist_cleanup( list, packages_free_change_package );
                            list = sub_list;
                        }
                    }
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }

        }
        packages_free_package_data( pkg_data );
    }

    //build-essential
    if( !packages_has_installed( pm, "build-essential", NULL ) )
    {
        if( !list )
            list = dlist_init();

        packages_clist_append( list, "build-essential", NULL, 0, 2 );

        sub_list = packages_get_depend_list( pm, "build-essential", NULL, NULL );
        if( sub_list )
        {
            dlist_cat( sub_list, list );
            dlist_cleanup( list, packages_free_change_package );
            list = sub_list;
        }
    }

    packages_clist_remove_duplicate_item( list );
    return list;
}

void packages_free_change_package( void *node )
{
    YPackageChangePackage *pkg;

    if( !node )
        return;

    pkg = (YPackageChangePackage *)node;

    free( pkg->name );

    if( pkg->version )
        free( pkg->version );

    free( pkg );
}

void packages_free_change_list( YPackageChangeList *list )
{
    if( !list )
        return;

    dlist_cleanup( list, packages_free_change_package );
}

/*
 * packages_get_install_list
 */
YPackageChangeList *packages_get_install_list( YPackageManager *pm, char *package_name, char *version )
{
    YPackageChangeList    *list, *depend;

    if( packages_has_installed( pm, package_name, version ) )
    {
        return NULL;
    }
    
    list = NULL;

    depend = packages_get_depend_list( pm, package_name, version, NULL );
    if( depend )
    {
        list = depend;
    }

    packages_clist_append( list, package_name, version, 0, 1 );

    depend = packages_get_recommended_list( pm, package_name, version );
    if( depend )
    {
        dlist_cat( list, depend );
        dlist_cleanup( depend, packages_free_change_package );
    }

    return list;
}

void packages_free_install_list( YPackageChangeList *list )
{
    packages_free_change_list( list );
}

/*
 * packages_get_dev_list
 */
YPackageChangeList *packages_get_dev_list( YPackageManager *pm, char *package_name, char *version )
{
    return packages_get_bdepend_list( pm, package_name, version );
}

void packages_free_dev_list( YPackageChangeList *list )
{
    packages_free_change_list( list );
}


/*
 * packages_install_list
 */
int packages_install_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg )
{
    int                     ret;
    YPackageChangePackage   *cur_pkg;

    if( !list )
        return 0;


    cur_pkg = dlist_head_data( list );
    while( cur_pkg )
    {
        ret = packages_install_package( pm, cur_pkg->name,cur_pkg->version, cb, cb_arg );
        if( ret )
        {
            return -1;
        }
        cur_pkg = dlist_next_data( list );
    }

    return 0;
}


/*
 * package_get_remove_list
 */
YPackageChangeList *packages_get_remove_list( YPackageManager *pm, char *package_name, int depth )
{
    int                     i, len;
    char                    *name, *size;
    YPackageList            *pkg_list;
    YPackageChangePackage   *cur_pkg;
    YPackageChangeList      *list, *sub_list;


    if( depth > 4 )
        return NULL;

    if( !packages_has_installed( pm, package_name, NULL ) )
    {
        return NULL;
    }


    /*
    cur_pkg =  (YPackageChangePackage *)malloc( sizeof( YPackageChangePackage ) );
    len = strlen( package_name );
    cur_pkg->name = (char *)malloc( len + 1 );
    strncpy( cur_pkg->name, package_name, len );
    cur_pkg->name[len] = 0;
    cur_pkg->version = NULL;
    cur_pkg->type = depth ? 2 : 1;
    dlist_append( list, cur_pkg );
    */

    list = dlist_init();

    if(  depth == 0 )
        packages_clist_append( list, package_name, NULL, 0, depth ? 2 : 1 );

    pkg_list = packages_get_list_by_depend( pm, 2000, 0, package_name, 1 );
    if( pkg_list )
    {
        for( i = 0; i < pkg_list->cnt; i++ )
        {
            name =  packages_get_list_attr( pkg_list, i, "name");

            if( !packages_has_installed( pm, name, NULL ) )
            {
                continue;
            }

            size =  packages_get_list_attr( pkg_list, i, "size");
            cur_pkg =  (YPackageChangePackage *)malloc( sizeof( YPackageChangePackage ) );
            len = strlen( name );
            cur_pkg->name = (char *)malloc( len + 1 );
            strncpy( cur_pkg->name, name, len );
            cur_pkg->name[len] = 0;
            cur_pkg->version = NULL;
            cur_pkg->type = 2;
            cur_pkg->size = size ? atoi( size ) : 0;
            dlist_insert( list, 1, cur_pkg );
        }
        packages_free_list( pkg_list );
    }

    if( list && list->cnt > 0 )
    {
        cur_pkg = dlist_head_data( list );
        while( cur_pkg  )
        {
            //here XXXX
            sub_list = packages_get_remove_list( pm, cur_pkg->name, depth+1 );
            if( sub_list && sub_list->cnt > 0 )
            {
                dlist_cat( sub_list, list );
                dlist_cleanup( list, packages_free_change_package );
                list = sub_list;
            }
            else
            {
                dlist_cleanup( sub_list, packages_free_change_package );
            }

            cur_pkg = dlist_next_data( list );
        }
    }

    //packages_clist_remove_duplicate_item( list );
    if( list->cnt == 0 )
    {
        dlist_cleanup( list, packages_free_change_package );
        return NULL;
    }

    return list;
}


/*
 * packages_remove_list
 */
int packages_remove_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg  )
{
    YPackageChangePackage    *cur_pkg;

    if( !list )
        return -1;

    cur_pkg = dlist_head_data( list );
    while( cur_pkg )
    {
        //printf("removing %s ...\n", cur_pkg->name );
        packages_remove_package( pm, cur_pkg->name, cb, cb_arg );
        cur_pkg = dlist_next_data( list );
    }
    return 0;
}

void packages_free_remove_list( YPackageChangeList *list )
{
    packages_free_change_list( list );
}

int packages_check_depend( YPackageManager *pm, YPackageData *pkg_data, char *extra, int extra_max_len )
{
    int                 i, len, offset = 0, ret = 0;
    char                *depend, *token, *saveptr, *version, *tmp, *tmp2;

    if( !pkg_data )
        return -1;

    for( i = 0; i < pkg_data->cnt; i++ )
    {
        tmp = packages_get_package_data_attr( pkg_data, i, "data_depend");
        if( tmp )
        {
            depend = strdup( tmp );
            tmp = NULL;
            version = NULL;
            token = strtok_r( depend, " ,", &saveptr );
            while( token )
            {
                tmp = util_strcat( token, NULL );
                if( (version = strchr( tmp, '(' )) )
                {
                    *version++ = 0;

                    while( *version == ' ' )
                        version++;

                    if( (tmp2 = strchr( version, ')' )) )
                        *tmp2 = 0;
                }

                if( !packages_has_installed( pm, tmp, version ) )
                {
                    if( extra && extra_max_len > 0 )
                    {
                        len = strlen( token );
                        if( extra_max_len > offset + len + 2 )
                        {
                            strncpy( extra + offset, token, len );
                            offset += len;
                            extra[offset] = ' ';
                            extra[++offset] = 0;
                        }

                    }

                    ret = -1;
                }

                free( tmp );
                version = NULL;
                token = strtok_r( NULL, " ,", &saveptr );
            }
            free( depend );
        }
    }

    return ret;
}

int packages_check_conflict( YPackageManager *pm, YPackageData *pkg_data, char *extra, int extra_max_len )
{
    int                 i, len, offset = 0, ret = 0;
    char                *conflict, *token, *saveptr, *version, *tmp, *tmp2;

    if( !pkg_data )
        return -1;

    for( i = 0; i < pkg_data->cnt; i++ )
    {
        tmp = packages_get_package_data_attr( pkg_data, i, "data_conflict");
        if( tmp )
        {
            conflict = strdup( tmp );
            tmp = NULL;
            version = NULL;
            token = strtok_r( conflict, " ,", &saveptr );
            while( token )
            {
                tmp = util_strcat( token, NULL );
                if( (version = strchr( tmp, '(' )) )
                {
                    *version++ = 0;

                    while( *version == ' ' )
                        version++;

                    if( (tmp2 = strchr( version, ')' )) )
                        *tmp2 = 0;
                }

                if( packages_has_installed( pm, tmp, version ) )
                {
                    if( extra && extra_max_len > 0 )
                    {
                        len = strlen( token );
                        if( extra_max_len > offset + len + 2 )
                        {
                            strncpy( extra + offset, token, len );
                            offset += len;
                            extra[offset] = ' ';
                            extra[++offset] = 0;
                        }
                    }

                    ret = -1;
                }
                free( tmp );
                version = NULL;
                token = strtok_r( NULL, " ,", &saveptr );
            }
            free( conflict );
        }
    }

    return ret;
}

/*
 * packages_check_package
 */
int packages_check_package( YPackageManager *pm, char *ypk_path, char *extra, int extra_max_len )
{
    int                 ret;
    YPackage            *pkg = NULL;
    YPackageData        *pkg_data = NULL;
    YPackageFile        *pkg_file = NULL;

    if( !ypk_path || access( ypk_path, R_OK ) )
        return -1;

    //get ypk's infomations
    if( packages_get_info_from_ypk( ypk_path, &pkg, &pkg_data, &pkg_file, NULL, NULL, NULL ) != 0 )
    {
        return -1;
    }
    packages_free_package_file( pkg_file );

    ret = packages_check_package2( pm, pkg, pkg_data, extra, extra_max_len );

    if( pkg )
        packages_free_package( pkg );

    if( pkg_data )
        packages_free_package_data( pkg_data );

    return ret;
}


int packages_check_package2( YPackageManager *pm, YPackage *pkg, YPackageData *pkg_data, char *extra, int extra_max_len )
{
    int                 ret, return_code = 0;
    char                *package_name, *arch, *arch2, *version, *version2, *repo, *repo2, line[32];
    struct utsname      buf;
    FILE                *fp;
    YPackage            *pkg2 = NULL;


    if( !pkg || !pkg_data )
        return -1;

    package_name = packages_get_package_attr( pkg, "name" );
    arch = packages_get_package_attr( pkg, "arch" );
    version = packages_get_package_attr( pkg, "version" );
    repo = packages_get_package_attr( pkg, "repo" );

    //check arch
    if( arch && ( arch[0] != 'a' || arch[1] != 'n' || arch[2] != 'y' ) )
    {
        if( ( fp = fopen( "/var/ypkg/modules/kernel", "r" ) ) != NULL )
        {
            if( fgets( line, 32, fp ) != NULL ) 
            {
                arch2 = util_rtrim( strrchr( line, ' ' ) + 1, 0 );
                if( strcmp( arch, arch2 ) )
                {
                    return_code = -2;

                    if( extra && extra_max_len > 0 )
                    {
                        strncpy( extra, arch2, extra_max_len );
                    }

                    goto exception_handler;
                }
            }
            fclose( fp );
        }
        else if( !uname( &buf ) )
        {
            if( strcmp( buf.machine, arch ) )
            {
                return_code = -2;

                if( extra && extra_max_len > 0 )
                {
                    strncpy( extra, buf.machine, extra_max_len );
                }

                goto exception_handler;
            }
        }
    }

    if( packages_check_depend( pm, pkg_data, extra, extra_max_len ) == -1 )
    {
        return_code = -3; 
        goto exception_handler;
    }


    if( packages_check_conflict( pm, pkg_data, extra, extra_max_len ) == -1 )
    {
        return_code = -4; 
        goto exception_handler;
    }

    //check installed
    if( (pkg2 = packages_get_package( pm, package_name, 1 ))  )
    {
        version2 = packages_get_package_attr( pkg2, "version" );
        repo2 = packages_get_package_attr( pkg2, "repo" );
        if( version && (strlen( version ) > 0) && version2 && (strlen( version2 ) > 0) )
        {
            ret = packages_compare_version( version, version2 );
            if( ret > 0 )
            {
                /*
                if( repo && repo2 && strcmp( repo, repo2 ) )
                {
                    return_code = 3; 
                }
                else
                {
                    return_code = 2; 
                }
                */

                    return_code = 3; 
            }
            else if( ret == 0 )
            {
                return_code = 2; 
            }
            else
            {
                if( repo && repo2 && strcmp( repo, repo2 ) )
                {
                    return_code = 1; 
                }
                else
                {
                    return_code = 2; 
                }
            }

            if( extra && extra_max_len > 0 )
            {
                strncpy( extra, version2, extra_max_len );
            }

            goto exception_handler;
        }
    }

exception_handler:
    if( pkg2 )
        packages_free_package( pkg2 );

    return return_code;
}

/*
 * packages_unpack_package
 *
 * params: unzip_info - 0 data, 1 data & info, 2 info
 */
int packages_unpack_package( char *ypk_path, char *dest_dir, int unzip_info, char *suffix )
{
    char                tmp_ypk_data[] = "/tmp/ypkdata.XXXXXX", tmp_ypk_info[] = "/tmp/ypkinfo.XXXXXX";
    char                *info_dest_dir;

    if( !ypk_path )
        return -1;

    if( !dest_dir )
        dest_dir = "./";

    //unzip pkgdata
    if( unzip_info < 2 )
    {
        mkstemp( tmp_ypk_data );
        if( archive_extract_file( ypk_path, "pkgdata", tmp_ypk_data ) == -1 )
        {
            return -2;
        }

        //copy files 
        //printf( "unpacking files ...\n");
        if( archive_extract_all( tmp_ypk_data, dest_dir, suffix ) == -1 )
        {
            remove( tmp_ypk_data );
            return -3;
        }
        
        remove( tmp_ypk_data );
    }

    //unzip pkginfo
    if( unzip_info )
    {
        mkstemp( tmp_ypk_info );
        if( archive_extract_file( ypk_path, "pkginfo", tmp_ypk_info ) == -1 )
        {
            return -4;
        }

        util_rtrim( dest_dir, '/' );
        info_dest_dir = util_strcat( dest_dir, "/YLMFOS", NULL );

        if( archive_extract_all( tmp_ypk_info, info_dest_dir, suffix ) == -1 )
        {
            free(info_dest_dir);
            remove( tmp_ypk_info );
            return -5;
        }
        
        free(info_dest_dir);
        remove( tmp_ypk_info );
    }

    return 0;
}

/*
 * packages_pack_package
 */
int packages_pack_package( char *source_dir, char *ypk_path, ypk_progress_callback cb, void *cb_arg )
{
    int                 ret, data_size, control_xml_size, del_var;
    time_t              now;
    char                *pkginfo, *pkgdata, *info_path, *control_xml, *control_xml_content, *filelist, *data_size_str, *time_str, *uri_str, *install, *install_script, *install_script_dest, *package_name, *version, *arch, *msg, *tmp;
    char                tmp_ypk_dir[] = "/tmp/ypkdir.XXXXXX";
    char                *exclude[] = { "YLMFOS", NULL };
    FILE                *fp_xml;
    xmlDocPtr           xmldoc = NULL;


    if( !source_dir )
        return -1;

    if( access( source_dir, R_OK | X_OK ) == -1 )
        return -1;

    if( !mkdtemp( tmp_ypk_dir ) )
        return -1;

    ret = 0;
    del_var = 0;
    pkginfo = NULL; 
    pkgdata = NULL; 
    info_path = NULL; 
    control_xml = NULL;
    filelist = NULL;
    install_script = NULL; 
    install_script_dest = NULL; 
    package_name = NULL; 
    version = NULL;
    arch = NULL;
    time_str = NULL;
    data_size_str = NULL;
    msg = NULL;

    //check control.xml
    control_xml = util_strcat( source_dir, "/YLMFOS/control.xml", NULL );
    if( access( control_xml, R_OK | W_OK ) == -1 )
    {
        ret = -1;
        goto cleanup;
    }

	if( ( xmldoc = xpath_open( control_xml ) ) == (xmlDocPtr)-1 )
    {
        ret = -2;
        goto cleanup;
    }

    package_name =  xpath_get_node( xmldoc, (xmlChar *)"//Package/@name" );
    if( !package_name )
    {
        ret = -2;
        goto cleanup;
    }

    version =  xpath_get_node( xmldoc, (xmlChar *)"//version" );
    if( !version )
    {
        ret = -2;
        goto cleanup;
    }

    arch =  xpath_get_node( xmldoc, (xmlChar *)"//arch" );
    if( !arch )
    {
        arch = strdup( "i686" );
    }

    install = xpath_get_node( xmldoc, (xmlChar *)"//install" );

    if( cb )
    {
        msg = util_strcat( "building package  `", package_name, ":", version, ":", arch, "`  in `", package_name, "_", version, "-", arch, ".ypk'\n", NULL );
        cb( cb_arg, "*", 0, 1, msg );
    }

    filelist = util_strcat( source_dir, "/YLMFOS/filelist", NULL );
    if( access( filelist, R_OK ) == -1 )
    {
        ret = -3;
        goto cleanup;
    }

    //copy install script
    if( install )
    {
        install_script = util_strcat( source_dir, "/YLMFOS/", install, NULL );
        if( access( install_script, R_OK ) != -1 )
        {
            install_script_dest = util_strcat( source_dir, "/var", NULL );
            if( access( install_script_dest, F_OK ) ) //not exists
            {
                del_var = 1;
            }
            free( install_script_dest );

            install_script_dest = util_strcat( source_dir, "/var/ypkg/db/", package_name, NULL );
            if( !util_mkdir( install_script_dest ) )
            {
                free( install_script_dest );
                install_script_dest = util_strcat( source_dir, "/var/ypkg/db/", package_name, "/", install, NULL );
                util_copy_file( install_script, install_script_dest );
            }
            free( install_script_dest );

        }
        free( install_script );
    }

    //pack pkgdata
    pkgdata = util_strcat( tmp_ypk_dir, "/", "pkgdata", NULL );
    ret = archive_create( pkgdata, 'J', 'c',  source_dir, exclude );

    if( del_var )
    {
        install_script_dest = util_strcat( source_dir, "/var", NULL );
    }
    else
    {
        install_script_dest = util_strcat( source_dir, "/var/ypkg", NULL );
    }
    util_remove_dir( install_script_dest );
    free( install_script_dest );

    if( ret == -1 )
    {
        free( pkgdata );
        ret = -4;
        goto cleanup;
    }

    //update control.xml
    if( (fp_xml = fopen( control_xml, "r+" )) )
    {
        control_xml_size = util_file_size( control_xml );
        control_xml_content = malloc( control_xml_size + 1 );
        memset( control_xml_content, 0, control_xml_size + 1 );
        if( control_xml_content )
        {
            if( fread( control_xml_content, 1, control_xml_size, fp_xml ) == control_xml_size )
            {
                now = time( NULL );
                if( now )
                {
                    tmp = util_int_to_str( now );
                    time_str = util_strcat( "<build_date>", tmp, "</build_date>", NULL );
                    free( tmp );
                }

                data_size = util_file_size( pkgdata );
                if( data_size )
                {
                    tmp = util_int_to_str( data_size );
                    data_size_str = util_strcat( "<size>", tmp, "</size>", NULL );
                    free( tmp );
                }

                tmp = preg_replace( "<build_date>.*?</build_date>|<build_date/>", time_str, control_xml_content, PCRE_CASELESS, 1 );
                free( control_xml_content );
                free( time_str );
                control_xml_content = tmp;

                tmp = preg_replace( "<size>.*?</size>|<size/>", data_size_str, control_xml_content, PCRE_CASELESS, 1 );
                free( control_xml_content );
                free( data_size_str );
                control_xml_content = tmp;

                uri_str =  xpath_get_node( xmldoc, (xmlChar *)"//uri" );
                if( !uri_str )
                {
                    tmp = strdup( package_name );
                    tmp[1] = 0;
                    uri_str = util_strcat( "<uri>", tmp, "/", package_name, "_", version, "-", arch, ".ypk</uri>", NULL );
                    free( tmp );
                    tmp = preg_replace( "<uri>.*?</uri>|<uri/>", uri_str, control_xml_content, PCRE_CASELESS, 1 );
                    if( tmp )
                    {
                        if( strlen( tmp ) > 1000 )
                        {
                            free( control_xml_content );
                            control_xml_content = tmp;
                        }
                        else
                        {
                            free( tmp );
                        }
                    }
                    free( uri_str );
                }

                tmp = NULL;

                rewind( fp_xml );
                control_xml_size = strlen( control_xml_content );
                fwrite( control_xml_content, control_xml_size, 1, fp_xml );
            }
            free( control_xml_content );
        }
        fclose( fp_xml );
        truncate( control_xml, control_xml_size );
    }

    free( pkgdata );

    //pack pkginfo
    info_path = util_strcat( source_dir, "/YLMFOS", NULL );
    pkginfo = util_strcat( tmp_ypk_dir, "/pkginfo", NULL );
    ret = archive_create( pkginfo, 'j', 't',  info_path, NULL );
    free( pkginfo );
    free( info_path );

    if( ret == -1 )
    {
        ret = -5;
        goto cleanup;
    }

    //pack ypk
    if( !ypk_path )
    {
        tmp = util_strcat( package_name, "_", version, "-", arch, ".ypk", NULL );
        if( archive_create( tmp, 0, 't', tmp_ypk_dir, NULL ) )
            ret = -6;
        free( tmp );
    }
    else
    {
        if( archive_create( ypk_path, 0, 't', tmp_ypk_dir, NULL ) )
            ret = -6;
    }

cleanup:
    if( control_xml )
        free( control_xml );

    if( filelist )
        free( filelist );

    if( package_name )
        free( package_name );

    if( version )
        free( version );

    if( arch )
        free( arch );

    util_remove_dir( tmp_ypk_dir );

    if( xmldoc )
    {
        xmlFreeDoc( xmldoc );
        xmlCleanupParser();
    }
    return ret;
}

/*
 * compare version
 */
#define MAX_VER_STR_LEN 64
int packages_compare_version( char *version1, char *version2 )
{
    int         result;
    char        *sub_ver1, *sub_ver2, *saveptr1, *saveptr2, ver_str1[MAX_VER_STR_LEN], ver_str2[MAX_VER_STR_LEN] ;

    if( !version1 || !version2 )
        return -2;

    memset( ver_str1, 0, MAX_VER_STR_LEN );
    strncpy( ver_str1, version1, MAX_VER_STR_LEN );
    memset( ver_str2, 0, MAX_VER_STR_LEN );
    strncpy( ver_str2, version2, MAX_VER_STR_LEN );

    sub_ver1 = strtok_r( ver_str1, "-", &saveptr1 );
    sub_ver2 = strtok_r( ver_str2, "-", &saveptr2 );
    if( !sub_ver1 || !sub_ver2 )
        return -2;

    result = packages_compare_main_version( sub_ver1, sub_ver2 );
    if( !result )
    {
        while( 1 )
        {
            sub_ver1 = strtok_r( NULL, "-", &saveptr1 );
            sub_ver2 = strtok_r( NULL, "-", &saveptr2 );
            if( !sub_ver1 && !sub_ver2 )
                break;

            result = packages_compare_sub_version( sub_ver1, sub_ver2 );
            if( result )
                break;
        }
    }

    if( result && result != -2 )
    {
        result = result > 0 ? 1 : -1;
    }

    return result;
}

int packages_compare_main_version( char *version1, char *version2 )
{
    int         ver1, ver2, result = 0;
    char        *sub_ver1, *sub_ver2, *saveptr1, *saveptr2, ver_str1[MAX_VER_STR_LEN], ver_str2[MAX_VER_STR_LEN] ;

    if( !version1 || !version2 )
        return -2;

    memset( ver_str1, 0, MAX_VER_STR_LEN );
    strncpy( ver_str1, version1, MAX_VER_STR_LEN );
    memset( ver_str2, 0, MAX_VER_STR_LEN );
    strncpy( ver_str2, version2, MAX_VER_STR_LEN );

    sub_ver1 = strtok_r( ver_str1, ".", &saveptr1 );
    sub_ver2 = strtok_r( ver_str2, ".", &saveptr2 );
    ver1 = atoi( sub_ver1 );
    ver2 = atoi( sub_ver2 );
    if( (result =  ver1 - ver2) )
    {
        result = result > 0 ? 1 : -1;
        return result;
    }

    while( 1 )
    {
        sub_ver1 = strtok_r( NULL, ".", &saveptr1 );
        sub_ver2 = strtok_r( NULL, ".", &saveptr2 );
        if( !sub_ver1 && !sub_ver2 )
        {
            return 0;
        }
        else if( !sub_ver1 )
        {
            return -1;
        }
        else if( !sub_ver2 )
        {
            return 1;
        }
        else
        {
            ver1 = atoi( sub_ver1 );
            ver2 = atoi( sub_ver2 );
            if( (result =  ver1 - ver2) )
            {
                result = result > 0 ? 1 : -1;
                return result;
            }
        }
    }
}

int packages_compare_sub_version( char *version1, char *version2 )
{
    int level1, level2;


    if( version1 == NULL )
        level1 = 4;
    else if( isdigit( version1[0] ) )
        level1 = 7;
    else if( version1[0] == 'r' && isdigit( version1[1] ) )
        level1 = 6;
    else if( !strncmp( version1, "ylmf", 4 ) && isdigit( version1[4] ) )
        level1 = 5;
    else if( version1[0] == 'r' && version1[1] == 'c' && isdigit( version1[2] ) )
        level1 = 3;
    else if( !strncmp( version1, "beta", 4 ) && isdigit( version1[4] ) )
        level1 = 2;
    else if( !strncmp( version1, "alpha", 5 ) && isdigit( version1[5] ) )
        level1 = 1;
    else 
        level1 = 0;

    if( version2 == NULL )
        level2 = 4;
    else if( isdigit( version2[0] ) )
        level2 = 7;
    else if( version2[0] == 'r' && isdigit( version2[1] ) )
        level2 = 6;
    else if( !strncmp( version2, "ylmf", 4 ) && isdigit( version2[4] ) )
        level2 = 5;
    else if( version2[0] == 'r' && version2[1] == 'c' && isdigit( version2[2] ) )
        level2 = 3;
    else if( !strncmp( version2, "beta", 4 ) && isdigit( version2[4] ) )
        level2 = 2;
    else if( !strncmp( version2, "alpha", 5 ) && isdigit( version2[5] ) )
        level2 = 1;
    else 
        level2 = 0;


    if( level1 == level2 )
    {
        if( level1 == 4 )
            return 0;
        else if( level1 == 7 )
            return atoi( version1 ) - atoi( version2 ); // 2 > 1
        else if( level1 == 6 )
            return atoi( version1+1 ) - atoi( version2+1 ); // r2 > r1
        else if( level1 == 5 )
            return atoi( version1+4 ) - atoi( version2+4 ); //ylmf
        else if( level1 == 3 )
            return atoi( version1+2 ) - atoi( version2+2 ); //rc
        else if( level1 == 2 )
            return atoi( version1+4 ) - atoi( version2+4 );
        else if( level1 == 1 )
            return atoi( version1+5 ) - atoi( version2+5 );
        else
            return 0;
    }
    else
    {
        return level1 - level2; // foo_1.0-1 > foo_1.0-r1 > foo_1.0-ylmf1 > foo_1.0 > foo_1.0-rc1
    }
}


int packages_compare_version_collate( void *arg, int len1, const void *version1, int len2, const void *version2 )
{
    if( len1 < 1 || len2 < 1 )
        return 0;

    return packages_compare_version( (char *)version1, (char *)version2 );
}

void packages_max_version_step( sqlite3_context *context, int argc, sqlite3_value **argv )
{
    char *tmp = sqlite3_aggregate_context( context, 32 );
    char *ver = (char *)sqlite3_value_text( argv[0] );

    if( ver && ver[0] ) 
    {
        if( tmp )
        {
            if( tmp[0] == 0 )
            {
                strncpy( tmp, ver, 32 );
            }
            else
            {
                if( packages_compare_version( ver, tmp ) > 0 )
                {
                    memset( tmp, 0, 32 );
                    strncpy( tmp, ver, 32 );
                }
            }
            
        }
    }
}

void packages_max_version_final( sqlite3_context *context )
{
    char *tmp = sqlite3_aggregate_context( context, 32 );
    if( tmp )
    {
        sqlite3_result_text( context, tmp, -1, SQLITE_TRANSIENT );
        return;
    }
    sqlite3_result_null( context );
}

/*
 * packages_exec_script
 */
int packages_exec_script( char *script, char *package_name, char *version, char *version2, char *action )
{
    int     status;
    char    *cmd;


    if( !script || !package_name || !version || !action )
        return -1;

    if( access( script, R_OK ) != 0 )
        return -1;

    cmd = util_strcat( "source '", script, "'; if type ", action, " >/dev/null 2>&1; then ", action, " ", package_name, " ", version, " ", version2 ? version2 : "", "; fi", NULL );
    //printf( "exec: %s\n", cmd );
    status = system( cmd );
    free( cmd );

    if( WEXITSTATUS( status ) )
    {
        return -1;
    }
    return 0;
}

/*
 * packages_install_local_package
 *
 * args: force == 1 force; force == 2 reinstall 
 */
int packages_install_local_package( YPackageManager *pm, char *ypk_path, char *dest_dir, int force, ypk_progress_callback cb, void *cb_arg  )
{
    int                 i, j, installed, upgrade, delete_file, ret, return_code;
    void                *filelist;
    char                *msg, *sql, *sql_data, *sql_filelist;
    char                *package_name, *version, *version2, *repo, *install, *file_type, *file_type2, *file_file, *file_size, *file_perms, *file_uid, *file_gid, *file_mtime, *file_extra, *tmp_file, *replace_name, *replace_file, *replace_with;
    char                tmp_ypk_install[] = "/tmp/ypkinstall.XXXXXX";
    char                extra[128];
    YPackage            *pkg, *pkg2;
    YPackageData        *pkg_data;
    YPackageFile        *pkg_file, *pkg_file2;
    YPackageReplaceFileList *replace_list;
    DB                  db;


    installed = 0;
    upgrade = 0;
    return_code = 0;
    msg = NULL;
    filelist = NULL;
    pkg = NULL; 
    pkg2 = NULL; 
    pkg_data = NULL;
    pkg_file = NULL;
    pkg_file2 = NULL;
    replace_list = NULL;
    package_name = NULL;
    version = NULL;
    install = NULL;
    replace_with = NULL;

    //check
    if( !ypk_path || access( ypk_path, R_OK ) )
        return -1;

    if( cb )
    {
        cb( cb_arg, ypk_path, 4, -1, "Checking dependency tree" );
    }

    mkstemp( tmp_ypk_install );
    ret = packages_get_info_from_ypk( ypk_path, &pkg, &pkg_data, &pkg_file, tmp_ypk_install, NULL, NULL );
    if( ret != 0 )
    {
        msg = util_strcat( "Error: invalid format or file not found.\n", NULL );
        cb( cb_arg, ypk_path, 3, 1, msg );
        free( msg );
        msg = NULL;

        return_code = -1;
        goto exception_handler;
    }

    memset( extra, 0, 128 );
    ret = packages_check_package2( pm, pkg, pkg_data, extra, 128 );

    if( cb )
    {
        switch( ret )
        {
            case 1:
            case 2:
                msg = util_strcat( "Newer or same version installed", NULL );
                break;

            case -1:
                msg = util_strcat( "Error: invalid format or file not found", NULL );
                break;

            case -2:
                msg = util_strcat( "Error: architecture mismatched. (",  extra, ")", NULL );
                break;

            case -3:
                msg = util_strcat( "Error: missing runtime dependencies: ", extra, NULL );
                break;

            case -4:
                msg = util_strcat( "Error: conflicting dependencies found: ", extra, NULL );
                break;

        }

        //cb( cb_arg, ypk_path, 3, 1, msg ? msg : "done" );
        cb( cb_arg, ypk_path, 4, 1, NULL );

        if( msg )
        {
            free( msg );
            msg = NULL;
        }
    }


    if( ret == -1 )
    {
        return_code = -1;
        goto exception_handler;
    }
    else if( ret < -1 && force != 1 )
    {
        return_code = ret;
        goto exception_handler;
    }
    else if( ret == 1 )
    {
        if( force != 1  )
        {
            return_code = 1;
            goto exception_handler;
        }

        installed = 1;
        upgrade = -1;
    }
    else if( ret == 2 )
    {
        if( !force ) //not force/reinstall
        {
            return_code = 1;
            goto exception_handler;
        }

        installed = 1;
    }
    else if( ret == 3 )
    {
        installed = 1;
        upgrade = 1;
    }



    if( !dest_dir )
        dest_dir = "/";


    //get package infomations
    if( cb )
    {
        cb( cb_arg, ypk_path, 4, -1, "Reading package information" );
    }


    package_name = packages_get_package_attr2( pkg, "name" );
    version = packages_get_package_attr2( pkg, "version" );
    repo = packages_get_package_attr2( pkg, "repo" );
    install = packages_get_package_attr2( pkg, "install" );

    pkg2 = packages_get_repo_package( pm, package_name, 0, NULL );


    pkg_file2 = packages_get_package_file( pm, package_name );

    replace_list = packages_get_replace_file_list( pm, pkg_data, pkg_file );

    if( cb )
    {
        cb( cb_arg, ypk_path, 4, 1, NULL );
    }

    //exec pre_x script
    if( cb )
    {
        cb( cb_arg, ypk_path, 5, -1, "Executing pre_install script\n" );
    }

    if( !access( tmp_ypk_install, R_OK ) )
    {
        if( !upgrade )
        {
            if( packages_exec_script( tmp_ypk_install, package_name, version, NULL, "pre_install" ) == -1 )
            {
                return_code = -7; 
                goto exception_handler;
            }
        }
        else if( upgrade == 1 )
        {
            version2 = extra;
            if( packages_exec_script( tmp_ypk_install, package_name, version, version2, "pre_upgrade" ) == -1 )
            {
                return_code = -7; 
                goto exception_handler;
            }
        }
        else //downgrade
        {
            version2 = extra;
            if( packages_exec_script( tmp_ypk_install, package_name, version, version2, "pre_downgrade" ) == -1 )
            {
                return_code = -7; 
                goto exception_handler;
            }
        }
    }

    if( cb )
    {
        cb( cb_arg, ypk_path, 5, 1, NULL );
    }

    //copy files 
    if( cb )
    {
        cb( cb_arg, ypk_path, 6, -1, "Copying files" );
    }

    if( (ret = packages_unpack_package( ypk_path, dest_dir, 0, "~ypk" )) != 0 )
    {
        //printf("unpack ret:%d\n",ret);
        return_code = -8; 
        goto exception_handler;
    }


    if( cb )
    {
        cb( cb_arg, ypk_path, 6, 1, NULL );
    }


    //update db
    if( cb )
    {
        cb( cb_arg, ypk_path, 8, -1, "Updating database" );
    }

    ret = db_init( &db, pm->db_name, OPEN_WRITE );
    db_cleanup( &db );
    db_exec( &db, "begin", NULL );  

    //update world
    sql = "replace into world (name, generic_name, is_desktop, category, arch, version, priority, install, exec, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count, install_time) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now'));";


    ret = db_exec( &db, sql,  
            package_name, //name
            packages_get_package_attr2( pkg, "generic_name"), //generic_name
            packages_get_package_attr2( pkg, "is_desktop"), //desktop
            packages_get_package_attr2( pkg, "category" ), //category
            packages_get_package_attr2( pkg, "arch" ), //arch
            version, //version
            packages_get_package_attr2( pkg, "priority" ), //priority
            packages_get_package_attr2( pkg, "install" ), //install
            packages_get_package_attr2( pkg, "exec" ), //exec
            packages_get_package_attr2( pkg, "license" ), //license
            packages_get_package_attr2( pkg, "homepage" ), //homepage
            repo, //repo
            packages_get_package_attr2( pkg, "size" ), //size
            packages_get_package_attr2( pkg, "sha" ), //repo
            packages_get_package_attr2( pkg, "build_date" ), //build_date
            packages_get_package_attr2( pkg, "packager" ), //packager
            packages_get_package_attr2( pkg, "uri" ), //uri
            packages_get_package_attr2( pkg, "description" ), //description
            packages_get_package_attr2( pkg, "data_count" ), //data_count
            NULL);
    

    if( ret != SQLITE_DONE )
    {
        db_exec( &db, "rollback", NULL );  
        //printf( "rollback world, db_ret:%d\n", ret );
        return_code = -10; 
        goto exception_handler;
    }
    
    //update world_data
    ret = db_exec( &db, "delete from world_data where name=?", package_name, NULL );  
    if( ret != SQLITE_DONE )
    {
        ret = db_exec( &db, "rollback", NULL );  
        //printf( "rollback world_data, db_ret:%d\n", ret );
        return_code = -10; 
        goto exception_handler;
    }

    sql_data = "insert into world_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    for( i = 0; i < pkg_data->cnt; i++ )
    {
        ret = db_exec( &db, sql_data,  
                package_name, //name
                version, //version
                packages_get_package_data_attr2( pkg_data, i, "data_name"), //data_name
                packages_get_package_data_attr2( pkg_data, i, "data_format" ), //data_format
                packages_get_package_data_attr2( pkg_data, i, "data_size" ), //data_size
                packages_get_package_data_attr2( pkg_data, i, "data_install_size" ), //data_install_size
                packages_get_package_data_attr2( pkg_data, i, "data_depend" ), //data_depend
                packages_get_package_data_attr2( pkg_data, i, "data_bdepend" ), //data_bdepend
                packages_get_package_data_attr2( pkg_data, i, "data_recommended" ), //data_recommended
                packages_get_package_data_attr2( pkg_data, i, "data_conflict" ), //data_conflict
                NULL);

        if( ret != SQLITE_DONE )
        {
            db_exec( &db, "rollback", NULL );  
            //printf( "rollback world_data, db_ret:%d\n", ret );
            return_code = -10; 
            goto exception_handler;
        }
    }


    //update file list
    ret = db_exec( &db, "delete from world_file where name=?", package_name, NULL );  
    if( ret != SQLITE_DONE )
    {
        db_exec( &db, "rollback", NULL );  
        //printf( "rollback file list, db_ret:%d\n", ret );
        return_code = -10; 
        goto exception_handler;
    }

    sql_filelist = "insert into world_file (name, version, type, file, size, perms, uid, gid, mtime, extra) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"; 

    for( i = 0; i < pkg_file->cnt; i++ )
    {
            file_type = packages_get_package_file_attr( pkg_file, i, "type");
            file_file = packages_get_package_file_attr( pkg_file, i, "file");
            file_size = packages_get_package_file_attr( pkg_file, i, "size");
            file_perms = packages_get_package_file_attr( pkg_file, i, "perms");
            file_uid = packages_get_package_file_attr( pkg_file, i, "uid");
            file_gid = packages_get_package_file_attr( pkg_file, i, "gid");
            file_mtime = packages_get_package_file_attr( pkg_file, i, "mtime");
            file_extra = packages_get_package_file_attr( pkg_file, i, "extra");

            ret = db_exec( &db, sql_filelist, 
                    package_name,
                    version,
                    file_type ? file_type : "",
                    file_file ? file_file : "",
                    file_size ? file_size : "",
                    file_perms ? file_perms : "",
                    file_uid ? file_uid : "",
                    file_gid ? file_gid : "",
                    file_mtime ? file_mtime : "",
                    file_extra ? file_extra : "",
                    NULL );

            if( ret != SQLITE_DONE )
            {
                db_exec( &db, "rollback", NULL );  
                //printf( "rollback file list, db_ret:%d\n", ret );
                return_code = -10; 
                goto exception_handler;
            }
    }

    //replace mark
    if( pkg_file2 )
    {
        for( i = 0; i < pkg_file2->cnt; i++ )
        {
            replace_name = packages_get_package_file_attr( pkg_file2, i, "replace");
            replace_file = packages_get_package_file_attr( pkg_file2, i, "file");
            if( replace_name && replace_file && strlen( replace_name ) > 0 )
            {
                ret = db_exec( &db, "update world_file set replace=?, replace_with='' where name=? and file=?", replace_name, package_name, replace_file, NULL );  
                if( ret != SQLITE_DONE )
                {
                    db_exec( &db, "rollback", NULL );  
                    return_code = -10; 
                    goto exception_handler;
                }
            }
        }
    }

    if( replace_list )
    {
        for( i = 0; i < replace_list->cur_cnt; i++ )
        {
            replace_name = hash_table_list_get_data( replace_list->htl, i, "replace" );
            replace_file = hash_table_list_get_data( replace_list->htl, i, "file" );
            if( replace_name && replace_file && strlen( replace_name ) > 0 )
            {
                ret = db_exec( &db, "update world_file set replace=? where name=? and file=?", replace_name, package_name, replace_file, NULL );  
                if( ret != SQLITE_DONE )
                {
                    db_exec( &db, "rollback", NULL );  
                    return_code = -10; 
                    goto exception_handler;
                }

                ret = db_exec( &db, "update world_file set replace='', replace_with=? where name=? and file=?", package_name, replace_name, replace_file, NULL );  
                if( ret != SQLITE_DONE )
                {
                    db_exec( &db, "rollback", NULL );  
                    return_code = -10; 
                    goto exception_handler;
                }
            }
        }
    }

    //set update flag
    if( pkg2 )
    {
        version2 = packages_get_package_attr( pkg2, "version" );
        if( version && (strlen( version ) > 0) && version2 && (strlen( version2 ) > 0) )
        {
            if( packages_compare_version( version2, version ) == 1 )
            {
                ret = db_exec( &db, "update world set can_update='1', version_available=? where name=?", version2, package_name, NULL );  
                if( ret != SQLITE_DONE )
                {
                    db_exec( &db, "rollback", NULL );  
                    //printf( "rollback universe, db_ret:%d\n", ret );
                    return_code = -10; 
                    goto exception_handler;
                }
            }
        }

    }

    ret = db_exec( &db, "commit", NULL );  
    if( ret != SQLITE_DONE )
    {
        db_exec( &db, "rollback", NULL );  
        //printf( "rollback commit, db_ret:%d\n", ret );
        return_code = -10; 
        goto exception_handler;
    }
    db_close( &db );

    if( cb )
    {
        cb( cb_arg, ypk_path, 8, 1, NULL );
    }

    //rename files
    if( replace_list )
    {
        for( i = 0; i < replace_list->cur_cnt; i++ )
        {
            replace_name = hash_table_list_get_data( replace_list->htl, i, "replace" );
            replace_file = hash_table_list_get_data( replace_list->htl, i, "file" );
            if( replace_name && replace_file && strlen( replace_name ) > 0 )
            {
                tmp_file = util_strcat( replace_file, "~", replace_name, ".orig", NULL );
                if( tmp_file )
                {
                    rename( replace_file, tmp_file );
                    free( tmp_file );
                    tmp_file = NULL;
                }
            }
        }
    }

    for( i = 0; i < pkg_file->cnt; i++ )
    {
        file_file = packages_get_package_file_attr( pkg_file, i, "file");
        if( file_file )
        {
            tmp_file = util_strcat( file_file, "~ypk", NULL );
            if( tmp_file )
            {
                rename( tmp_file, file_file );
                free( tmp_file );
                tmp_file = NULL;
            }
        }
    }

    if( install && strlen( install ) )
    {
        file_file = util_strcat( "/var/ypkg/db/", package_name, "/", install, NULL );
        if( file_file )
        {
            tmp_file = util_strcat( "/var/ypkg/db/", package_name, "/", install, "~ypk", NULL );
            if( tmp_file )
            {
                rename( tmp_file, file_file );
                free( tmp_file );
                tmp_file = NULL;
            }
            free( file_file );
            file_file = NULL;
        }
    }

    if( installed && pkg_file && pkg_file2 )
    {
        for( i = 0; i < pkg_file2->cnt; i++ )
        {
            file_file = packages_get_package_file_attr( pkg_file2, i, "file");
            if( file_file )
            {
                //skip directory
                file_type = packages_get_package_file_attr( pkg_file2, i, "type");
                if( !file_type || file_type[0] == 'D' )
                    continue;

                //remove backup file && skip
                replace_with = packages_get_package_file_attr( pkg_file2, i, "replace_with");
                if( replace_with && strlen( replace_with ) > 0 )
                {
                    tmp_file = util_strcat( file_file, "~", package_name, ".orig", NULL );
                    if( tmp_file )
                    {
                        remove( tmp_file );
                        free( tmp_file );
                        tmp_file = NULL;
                    }
                    continue;
                }

                //delete the files only in the old version
                delete_file = 1;
                for( j = 0; j < pkg_file->cnt; j++ )
                {
                    file_type2 = packages_get_package_file_attr( pkg_file, j, "type");
                    //if( !file_type2 || file_type2[0] != file_type[0] )
                    if( !file_type2 || file_type2[0] == 'D' )
                        continue;

                    if( !strcmp( file_file, packages_get_package_file_attr( pkg_file, j, "file" ) ) )
                    {
                        delete_file = 0;
                        break;
                    }

                }

                if( delete_file )
                {
                    remove( file_file );
                }
            }

        }
    }


    //exec post_x script
    if( cb )
    {
        cb( cb_arg, ypk_path, 7, -1, "Executing post_install script\n" );
    }

    if( !access( tmp_ypk_install, R_OK ) )
    {
        if( !upgrade )
        {
            if( packages_exec_script( tmp_ypk_install, package_name, version, NULL, "post_install" ) == -1 )
            {
                return_code = -11; 
                goto exception_handler;
            }
        }
        else if( upgrade == 1 )
        {
            version2 = extra;
            if( packages_exec_script( tmp_ypk_install, package_name, version, version2, "post_upgrade" ) == -1 )
            {
                return_code = -11; 
                goto exception_handler;
            }
        }
        else //downgrade
        {
            version2 = extra;
            if( packages_exec_script( tmp_ypk_install, package_name, version, version2, "post_downgrade" ) == -1 )
            {
                return_code = -11; 
                goto exception_handler;
            }
        }
    }

    if( cb )
    {
        cb( cb_arg, ypk_path, 7, 1, NULL );
    }

    if( !strcmp( package_name, "ypkg2" ) )
    {
        packages_upgrade_db( pm );
    }

    if( cb )
    {
        cb( cb_arg, ypk_path, 9, 1, NULL );
    }

exception_handler:

    //rollback
    if( return_code <= -8 && return_code > -11 )
    {
        for( i = 0; i < pkg_file->cnt; i++ )
        {
            tmp_file = util_strcat( packages_get_package_file_attr( pkg_file, i, "file"), "~ypk", NULL);
            if( tmp_file )
            {
                remove( tmp_file );
                free( tmp_file );
                tmp_file = NULL;
            }
        }
    }

    if( install && strlen( install ) )
    {
        tmp_file = util_strcat( "/var/ypkg/db/", package_name, "/", install, "~ypk", NULL );
        if( tmp_file )
        {
            remove( tmp_file );
            free( tmp_file );
            tmp_file = NULL;
        }
    }

    if( return_code <= -7 && return_code > -11 && access( tmp_ypk_install, R_OK ) == 0 )
    {
        if( !upgrade )
        {
            packages_exec_script( tmp_ypk_install, package_name, version, NULL, "pre_install_fallback" );
        }
        else if( upgrade == 1 )
        {
            version2 = extra;
            packages_exec_script( tmp_ypk_install, package_name, version, version2, "pre_upgrade_fallback" );
        }
        else
        {
            version2 = extra;
            packages_exec_script( tmp_ypk_install, package_name, version, version2, "pre_downgrade_fallback" );
        }
    }

    //clean up
    if( filelist )
        free( filelist );

    if( pkg )
        packages_free_package( pkg );

    if( pkg2 )
        packages_free_package( pkg2 );

    if( pkg_data )
        packages_free_package_data( pkg_data );

    if( pkg_file )
        packages_free_package_file( pkg_file );

    if( pkg_file2 )
        packages_free_package_file( pkg_file2 );

    if( replace_list )
        packages_free_replace_file_list( replace_list );

    remove( tmp_ypk_install );
    return return_code;
}

int packages_remove_package( YPackageManager *pm, char *package_name, ypk_progress_callback cb, void *cb_arg  )
{
    //int             i, return_code, repo_testing;
    int             i, return_code;
    //char            *install_file, *version, *install_file_path = NULL, *table, *sql, *file_path, *file_type, *replace, *replace_with, *tmp_file;
    char            *install_file, *version, *install_file_path = NULL, *file_path, *file_type, *replace, *replace_with, *tmp_file;
    YPackage        *pkg;
    YPackageFile    *pkg_file;
    DB              db;

    if( !package_name )
        return -1;

    return_code = 0;

    /*
    if( pm->accept_repo && !strcmp( pm->accept_repo, "testing" ) )
        repo_testing =1;
    else
        repo_testing =0;
        */

    if( cb )
    {
        cb( cb_arg, package_name, 0, 3, "Remove" );
        cb( cb_arg, package_name, 1, -1, "Initializing" );
    }

    //get info from db
    pkg = packages_get_package( pm, package_name, 1 );
    if( !pkg )
    {
        return -2;
    }

    install_file = packages_get_package_attr( pkg, "install" );
    version = packages_get_package_attr( pkg, "version" );

    pkg_file = packages_get_package_file( pm, package_name );
    if( !pkg_file )
    {
        return_code = -2; 
        goto exception_handler;
    }

    if( cb )
    {
        cb( cb_arg, package_name, 1, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 5, -1, "Executing pre_remove script\n" );
    }

    //exec pre_remove script
    if( install_file && strlen( install_file ) > 8 )
    {
        install_file_path = util_strcat( PACKAGE_DB_DIR, "/", package_name, "/", install_file, NULL );
        if( !access( install_file_path, R_OK ) )
        {
            //printf( "running pre remove script ...\n" );
            if( packages_exec_script( install_file_path, package_name, version, NULL, "pre_remove" ) == -1 )
            {
                return_code = -3; 
                goto exception_handler;
            }
        }
    }

    if( cb )
    {
        cb( cb_arg, package_name, 5, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 6, -1, "Deleting files" );
    }

    //delete files
    for( i = 0; i < pkg_file->cnt; i++ )
    {
        file_path = packages_get_package_file_attr( pkg_file, i, "file");
        file_type = packages_get_package_file_attr( pkg_file, i, "type");
        replace = packages_get_package_file_attr( pkg_file, i, "replace");
        replace_with = packages_get_package_file_attr( pkg_file, i, "replace_with");
        if( file_path && file_type && ( file_type[0] == 'F' || file_type[0] == 'S' ) )
        {
            if( replace_with && strlen( replace_with ) > 0 )
            {
                tmp_file = util_strcat( file_path, "~", package_name, ".orig", NULL );
                if( tmp_file )
                {
                    remove( tmp_file );
                    free( tmp_file );
                    tmp_file = NULL;
                }
            }
            else
            {
                remove( file_path );
                if( replace && strlen( replace ) > 0 )
                {
                    tmp_file = util_strcat( file_path, "~", replace, ".orig", NULL );
                    if( tmp_file )
                    {
                        rename( tmp_file, file_path );
                        free( tmp_file );
                        tmp_file = NULL;
                    }
                }
            }

        }
    }

    //delete directories
    for( i = 0; i < pkg_file->cnt; i++ )
    {
        file_path = packages_get_package_file_attr( pkg_file, i, "file");
        file_type = packages_get_package_file_attr( pkg_file, i, "type");
        if( file_path && file_type &&  file_type[0] == 'D' )
        {
            remove( file_path );
        }
    }

    if( cb )
    {
        cb( cb_arg, package_name, 6, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 7, -1, "Executing post_remove script\n" );
    }

    //exec post_remove script
    if( install_file_path && !access( install_file_path, R_OK ) )
    {
        //printf( "running post remove script ...\n" );
        if( packages_exec_script( install_file_path, package_name, version, NULL, "post_remove" ) == -1 )
        {
            return_code = -5; 
            goto exception_handler;
        }
    }


    //delete /var/ypkg/db/$N
    file_path = util_strcat( PACKAGE_DB_DIR, "/", package_name, NULL );
    //printf( "deleting %s ... \n", file_path );
    util_remove_dir( file_path );
    free( file_path );

    if( cb )
    {
        cb( cb_arg, package_name, 7, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 8, -1, "Updating database" );
    }

    //update db
    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "begin", NULL );  
    db_exec( &db, "delete from world where name=?", package_name, NULL );  
    db_exec( &db, "delete from world_data where name=?", package_name, NULL );  
    db_exec( &db, "delete from world_file where name=?", package_name, NULL );  
    for( i = 0; i < pkg_file->cnt; i++ )
    {
        file_path = packages_get_package_file_attr( pkg_file, i, "file");
        file_type = packages_get_package_file_attr( pkg_file, i, "type");
        replace = packages_get_package_file_attr( pkg_file, i, "replace");
        replace_with = packages_get_package_file_attr( pkg_file, i, "replace_with");
        if( file_path && file_type && ( file_type[0] == 'F' || file_type[0] == 'S' ) )
        {
            if( replace_with && strlen( replace_with ) > 0  )
            {
                if( replace && strlen( replace ) > 0  )
                {
                    db_exec( &db, "update world_file set replace=? where name=? and file=?", replace, replace_with, file_path, NULL );  

                    db_exec( &db, "update world_file set replace_with=? where name=? and file=?", replace_with, replace, file_path, NULL );  
                }
                else
                {
                    db_exec( &db, "update world_file set replace='' where name=? and file=?", replace_with, file_path, NULL );  
                }
            }
            else
            {
                if( replace && strlen( replace ) > 0  )
                    db_exec( &db, "update world_file set replace_with='' where name=? and file=?", replace, file_path, NULL );  
            }

        }
    }

    db_exec( &db, "end", NULL );  
    db_close( &db );

    if( cb )
    {
        cb( cb_arg, package_name, 8, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 9, 1, NULL );
    }


exception_handler:
    if( install_file_path )
        free( install_file_path );

    packages_free_package( pkg );

    packages_free_package_file( pkg_file );
    return return_code;
}


/*
 * clean up cache directory
 */
int packages_cleanup_package( YPackageManager *pm )
{
    YPackageSource          *source;

    source = dlist_head_data( pm->source_list );
    while( source )
    {
        util_remove_files( source->package_dest, ".ypk" );
        source = dlist_next_data( pm->source_list );
    }
    return 0;
}


/*
 * log
 */
int packages_log( YPackageManager *pm, char *package_name, char *msg )
{
    char    *time_str, *log;
    time_t  t; 
    FILE    *log_file;

    if( !msg )
        return -1;

    log = pm->log ? pm->log : LOG_FILE;
    log_file = fopen( log, "a" );

    if( log_file )
    {

        if( ( t = time( NULL ) ) == -1 )
        {
            fclose( log_file );
            return -1;
        }

        time_str = util_time_to_str( t );
        fprintf( log_file, "%s %s : %s\n", time_str, package_name ? package_name : "", msg );

        if( time_str )
            free( time_str );

        fflush( log_file );
        fclose( log_file );
        return 0;
    }

    return -1;
}
