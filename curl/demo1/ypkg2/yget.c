/* yget2
 *
 * Copyright (c) 2013 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com> ChenYu_Xiao<yunsn0303@gmail.com>
 * Date: 2013.3.12
 */

#define LIBYPK 1

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ypackage.h"
#include "util.h"

#define COLOR_RED "\e[31;49m"
#define COLOR_GREEN "\e[32;49m"
#define COLOR_YELLO "\e[33;49m"
#define COLOR_BLUE "\e[34;49m"
#define COLOR_MAGENTA "\e[35;49m"
#define COLOR_CYAN "\e[36;49m"
#define COLOR_WHILE "\033[1m"
#define COLOR_RESET "\e[0m"

typedef struct {
    int             progress;
    double          cnt;
    struct timeval  st;
}DownloadStat;

int colorize;

struct option longopts[] = {
    { "help", no_argument, NULL, 'h' },
    { "install", no_argument, NULL, 'I' },
    { "reinstall", no_argument, NULL, 'r' },
    { "install-dev", no_argument, NULL, 'i' },
    { "remove", no_argument, NULL, 'R' },
    { "autoremove", no_argument, NULL, 'A' },
    { "search", no_argument, NULL, 'S' },
    { "show", no_argument, NULL, 's' },
    { "status", no_argument, NULL, 't' },
    { "clean", no_argument, NULL, 'C' },
    { "update", no_argument, NULL, 'u' },
    { "upgrade", no_argument, NULL, 'U' },
    { "color", no_argument, &colorize, 1 },
    { 0, 0, 0, 0}
};

void usage()
{
    char *usage = "\
Usage: yget command [options] pkg1 [pkg2 ...]\n\n\
yget is a simple command line interface for downloading and\n\
installing packages. The most frequently used commands are update and install.\n\n\
Commands:\n\
  --install           Install packages and dependencies (pkg is leafpad not leafpad.ypk)\n\
  --reinstall         Reinstall packages and dependencies (pkg is leafpad not leafpad.ypk)\n\
  --install-dev       Install build-dependencies for packages (pkg is leafpad not leafpad.ypk)\n\
  --remove            Remove package\n\
  --search            Search the package list for a regex pattern\n\
  --show              Show a readable record for the package\n\
  --status            Show the infomation of a installed packages\n\
  --clean             Erase downloaded archive files\n\
  --autoremove        Remove automatically all unused packages\n\
  --upgrade           Perform an upgrade\n\
  --update            Retrieve new lists of packages\n\
  --color             Colorize the output\n\n\
Options:\n\
  -p                  Instead of actually install, simply display what to do\n\
  -y                  Assume Yes to all queries\n\
  -d		      Download only - do NOT install\n\
  -f | --force        Force install, override problems\n\
";

    printf( "%s\n", usage );
}

int yget_progress_callback( void *cb_arg, char *package_name, int action, double progress, char *msg )
{
    YPackageManager *pm;

    pm = (YPackageManager *)cb_arg;

    if( !action )
    {
        printf( "%s %s \n", msg, package_name );
        packages_log( pm, package_name, msg );

    }
    else if( action == 9 )
    {
        packages_log( pm, package_name, "Finish" );
    }
    else
    {
        if( progress == 0 || progress == -1 )
        {
            printf( "%s... ", msg );
            packages_log( pm, package_name, msg );
        }

        if( progress == 1 )
        {
            if( action == 3)
            {
                printf( "\n%s\n", msg );
                packages_log( pm, package_name, msg );
            }
            else 
            {
                printf( "%s\n", "done" );
            }
        }
    }

    fflush( stdout );
    return 0;
}


int yget_download_progress_callback( void *cb_arg, char *package_name, double dltotal, double dlnow )
{
    int                 progress, bar_len;
    long                speed;
    char                *bar = "====================";
    char                *space = "                    ";
    struct timeval      now;
    DownloadStat        *statp;

    statp = (DownloadStat *)cb_arg;

    progress = (int)(dlnow * 100 / dltotal);
    if( progress < 0 || progress == statp->progress )
        return 0;


    speed = 0;
    statp->progress = progress;
    if( !gettimeofday( &now, NULL ) )
    {
        if( statp->st.tv_sec != now.tv_sec && statp->cnt != dlnow )
        {
            speed = (dlnow - statp->cnt) * 1000000 /  ( (now.tv_sec - statp->st.tv_sec) * 1000000 + (now.tv_usec - statp->st.tv_usec) );
            speed = speed < 0 ? 0 : speed;

            statp->st.tv_sec = now.tv_sec;
            statp->st.tv_usec = now.tv_usec; 
            statp->cnt = dlnow;
        }
    }


    bar_len = progress / 5;
    bar_len = bar_len > 0 ? bar_len : 1;

    printf( 
            "[%.*s>%.*s]   %.2f%s/%.2f%s[%3d%%]  %ld%s/s%s", 
            bar_len, 
            bar, 
            20 - bar_len, 
            space, 
            dlnow > 1048576 ? dlnow/1048576 : (dlnow > 1024 ? dlnow/1024 : dlnow),
            dlnow > 1048576 ? "MB" : (dlnow > 1024 ? "KB" : "B"),
            dltotal > 1048576 ? dltotal/1048576 : (dltotal > 1024 ? dltotal/1024 : dltotal),
            dltotal > 1048576 ? "MB" : (dltotal > 1024 ? "KB" : "B"),
            progress, 
            speed > 1024 ? speed/1024 : speed, 
            speed > 1024 ? "KB" : "B", 
            space 
            );

    if( progress == 100 )
        putchar( '\n' );
    else
        putchar( '\r' );

    fflush( stdout );

    return 0;
}

int yget_upgrade_self( char *package_path )
{
    pid_t           pid;
    struct stat     sb;
    char            lib_path[32];

    if( !package_path )
        return -1;

    if( util_mkdir( "/tmp/ypkg2_backup" ) != 0 )
        return -1;

    lstat( "/usr/lib/libypk.so", &sb );
    if( sb.st_mode & S_IFLNK )
    {
        memset( lib_path, 0, 32 );
        strncpy( lib_path, "/usr/lib/", 9 );
        if( readlink( "/usr/lib/libypk.so", lib_path+9, 23 ) == -1 )
            goto failed;

        if( util_copy_file( lib_path, "/tmp/ypkg2_backup/libypk.so" ) != 0 )
            goto failed;
        else
            chmod( "/tmp/ypkg2_backup/libypk.so", 0755 );
    }
    else
    {
        if( util_copy_file( "/usr/lib/libypk.so", "/tmp/ypkg2_backup/libypk.so" ) != 0 )
            goto failed;
        else
            chmod( "/tmp/ypkg2_backup/libypk.so", 0755 );
    }

    if( util_copy_file( "/usr/bin/ypkg2", "/tmp/ypkg2_backup/ypkg2" ) != 0 )
        goto failed;
    else
        chmod( "/tmp/ypkg2_backup/ypkg2", 0755 );

    if( util_copy_file( "/usr/bin/yget2", "/tmp/ypkg2_backup/yget2" ) != 0 )
        goto failed;
    else
        chmod( "/tmp/ypkg2_backup/yget2", 0755 );

    if( util_copy_file( "/usr/bin/ypkg2-upgrade", "/tmp/ypkg2_backup/ypkg2-upgrade" ) != 0 )
        goto failed;
    else
        chmod( "/tmp/ypkg2_backup/ypkg2-upgrade", 0755 );

    if( (pid = fork()) < 0 )
    {
        goto failed;
    }
    else if( pid == 0 )
    {
        if( execl( "/tmp/ypkg2_backup/ypkg2-upgrade", "ypkg2-upgrade", "/tmp/ypkg2_backup", package_path, NULL ) < 0 )
            goto failed;
    }

    return 0;

failed:
    util_remove_dir( "/tmp/ypkg2_backup" );
    return -1;
}


int yget_install_package( YPackageManager *pm, char *package_name, char *version, int download_only, int upgrade_self, int force )
{
    int                 ret, return_code;
    char                *source_uri = NULL, *package_dest = NULL, *target_url = NULL, *package_url = NULL, *package_path = NULL, *pkg_sha = NULL, *ypk_sha = NULL;
    YPackage            *pkg;
    //YPackageDCB         dcb;
    DownloadStat        dl_stat;

    if( !package_name )
        return -1;

    packages_log( pm, package_name, "Install" );
    packages_log( pm, package_name, "Initializing" );

    return_code = 0;
    if( !packages_exists( pm, package_name, version ) )
    {
        if( colorize )
            fprintf( stderr, COLOR_RED "Error: package %s(%s) not found.\n" COLOR_RESET, package_name, version );
        else
            fprintf( stderr, "Error: package %s(%s) not found.\n" , package_name, version );
        return -2;
    }

    pkg = packages_get_package( pm, package_name, 0 );
    if( !pkg )
    {
        if( colorize )
            fprintf( stderr, COLOR_RED "Error: package %s(%s) not found.\n" COLOR_RESET, package_name, version );
        else
            fprintf( stderr,  "Error: package %s(%s) not found.\n" , package_name, version );
        return -2;
    }

    package_url = packages_get_package_attr( pkg, "uri" );
    if( !package_url )
    {
        if( colorize )
            fprintf( stderr, COLOR_RED "Error: read download url of package failed.\n" COLOR_RESET );
        else
            fprintf( stderr,  "Error: read download url of package failed.\n"  );
        return_code = -3;
        goto return_point;
    }

    package_dest = packages_get_package_attr( pkg, "package_dest" );
    if( !package_dest )
    {
        return_code = -3;
        goto return_point;
    }

    source_uri = packages_get_package_attr( pkg, "source_uri" );
    if( !source_uri )
    {
        return_code = -3;
        goto return_point;
    }

    package_path = util_strcat( package_dest, "/", basename( package_url ), NULL );
    target_url = util_strcat( source_uri, "/", package_url, NULL );

    packages_log( pm, package_name, "Downloading" );
    printf( "Downloading %s to %s/\n", target_url, package_dest  );
    dl_stat.progress = 0;
    dl_stat.cnt = 0;
    dl_stat.st.tv_sec = 0;
    dl_stat.st.tv_usec = 0;
    //dcb.cb = yget_download_progress_callback;
    //dcb.arg = &dl_stat;

    pkg_sha = packages_get_package_attr( pkg, "sha" );

    if( !access( package_path, F_OK ) )
    {
        ypk_sha = util_sha1( package_path );

        if( strncmp( pkg_sha, ypk_sha, 41 ) )
        {
            if( packages_download_package( NULL, package_name, target_url, package_path, 1, yget_download_progress_callback,&dl_stat, yget_progress_callback, pm ) < 0 )
            {
                if( colorize )
                    fprintf( stderr, COLOR_RED "Error: download %s from %s failed.\n" COLOR_RESET, package_name, target_url );
                else
                    fprintf( stderr,  "Error: download %s from %s failed.\n" , package_name, target_url );
                return_code = -4;
                goto return_point;
            }

            free( ypk_sha );

            ypk_sha = util_sha1( package_path );
            if( ypk_sha && strncmp( pkg_sha, ypk_sha, 41 ) )
            {
                if( colorize )
                    fprintf( stderr, COLOR_RED "Error: checksum mismatched. [%s sha1sum: %s]\n" COLOR_RESET, package_path, ypk_sha );
                else
                    fprintf( stderr,  "Error: checksum mismatched. [%s sha1sum: %s]\n" , package_path, ypk_sha );
                return_code = -4;
                goto return_point;
            }
        }
    }
    else
    {
        if( packages_download_package( NULL, package_name, target_url, package_path, 0, yget_download_progress_callback,&dl_stat, yget_progress_callback, pm ) < 0 )
        {
            if( colorize )
                fprintf( stderr, COLOR_RED "Error: download %s from %s failed.\n" COLOR_RESET, package_name, target_url );
            else
                fprintf( stderr,  "Error: download %s from %s failed.\n" , package_name, target_url );
            return_code = -4;
            goto return_point;
        }

        ypk_sha = util_sha1( package_path );
        if( ypk_sha && strncmp( pkg_sha, ypk_sha, 41 ) )
        {
            if( colorize )
                fprintf( stderr, COLOR_RED "Error: checksum mismatched. [%s sha1sum: %s]\n" COLOR_RESET, package_path, ypk_sha );
            else
                fprintf( stderr,  "Error: checksum mismatched. [%s sha1sum: %s]\n" , package_path, ypk_sha );
            return_code = -4;
            goto return_point;
        }
    }

    if( !download_only )
    {
        printf( "Installing %s\n", package_path );
        if( upgrade_self )
        {
            yget_upgrade_self( package_path );
        }
        else
        {
            if( (ret = packages_install_local_package( pm, package_path, "/", force, yget_progress_callback, pm )) )
            {
                switch( ret )
                {
                    case 1:
                    case 2:
                        if( colorize )
                            printf( COLOR_YELLO "Newer or same version installed, skipped.\n" COLOR_RESET );
                        else
                            printf(  "Newer or same version installed, skipped.\n"  );
                        break;
                    case 0:
                        if( colorize )
                            printf( COLOR_GREEN "Installation successful.\n" COLOR_RESET );
                        else
                            printf(  "Installation successful.\n"  );
                        break;
                    case -1:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: invalid format or file not found.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: invalid format or file not found.\n"  );
                        break;
                    case -2:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: architecture mismatched.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: architecture mismatched.\n"  );
                        break;
                    case -3:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: missing runtime dependencies.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: missing runtime dependencies.\n"  );
                        break;
                    case -4:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: conflicting dependencies found.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: conflicting dependencies found.\n"  );
                        break;
                    case -5:
                    case -6:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: read state infomation failed.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: read state infomation failed.\n"  );
                        break;
                    case -7:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: an error occurred while executing pre_install script.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: an error occurred while executing pre_install script.\n"  );
                        break;
                    case -8:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: an error occurred while copying files.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: an error occurred while copying files.\n"  );
                        break;
                    case -9:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: an error occurred while executing post_install script.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: an error occurred while executing post_install script.\n"  );
                    case -10:
                        if( colorize )
                            fprintf( stderr, COLOR_RED "Error: an error occurred while updating database.\n" COLOR_RESET );
                        else
                            fprintf( stderr,  "Error: an error occurred while updating database.\n"  );
                        break;
                }

                return_code = -5;
            }
        }
    }

return_point:
    if( package_path )
        free( package_path );

    if( target_url )
        free( target_url );

    if( ypk_sha )
        free( ypk_sha );

    packages_free_package( pkg );
    pkg = NULL;
    return return_code;
}

int yget_install_list( YPackageManager *pm, YPackageChangeList *list, int download_only, int force )
{
    int                     skip = 0, skip_list = 0, ret, return_code = 0;
    YPackageChangePackage   *cur_pkg;

    if( !list )
        return 0;

    cur_pkg = dlist_head_data( list );
    while( cur_pkg )
    {
        if( cur_pkg->type == 7 )
        {
            if( skip )
                skip_list++;
        }
        else if( cur_pkg->type == 8 )
        {
            if( skip_list )
                skip_list--;
            else if( skip )
                skip--;
        }
        else if( skip )
        {
            if( cur_pkg->type == 6 && !skip_list )
            {
                skip++;
            }
            if( colorize )
                printf( "skip " COLOR_WHILE "%s" COLOR_RESET " ...\n", cur_pkg->name );
            else
                printf( "skip "  "%s"  " ...\n", cur_pkg->name );
        }
        else
        {
            if( colorize )
                printf( "Installing " COLOR_WHILE "%s" COLOR_RESET " ...\n", cur_pkg->name );
            else
                printf( "Installing "  "%s"  " ...\n", cur_pkg->name );
            ret = yget_install_package( pm, cur_pkg->name, NULL, download_only, 0, force );
            if( !ret )
            {
                if( colorize )
                    printf( COLOR_GREEN "Installation successful.\n" COLOR_RESET );
                else
                    printf(  "Installation successful.\n"  );
            }
            else
            {
                if( colorize )
                    fprintf( stderr, COLOR_RED "Error: installation failed.\n" COLOR_RESET );
                else
                    fprintf( stderr,  "Error: installation failed.\n"  );
                return_code = -1;
                if( cur_pkg->type != 3 )
                {
                    skip++;

                    if( cur_pkg->type == 6 )
                    {
                        skip++;
                    }
                }

                /*
                if( force != 1 )
                    return ret;
                    */
            }
        }

        cur_pkg = dlist_next_data( list );
    }

    return return_code;
}


int main( int argc, char **argv )
{
    int             arg_cnt, c, force, init, download_only, simulation, yes, unknown_arg, i, j, action, ret, err, len, size, install_size, pkg_count, upgrade_ypkg;
    char            confirm, *tmp, *package_name, *install_date, *build_date, *version,  *installed, *can_update, *repo;

    YPackageManager *pm;
    YPackage        *pkg, *pkg2;
    YPackageData    *pkg_data;
    YPackageList    *pkg_list;
    YPackageChangePackage  *cur_package;       
    YPackageChangeList     *sub_list, *missing_list, *install_list, *remove_list, *upgrade_list;

    if( argc == 1 )
    {

        usage();
        return 1;
    }

    action = 0;
    err = 0;
    init = 0;
    force = 0;
    download_only = 0;
    simulation = 0;
    yes = 0;
    unknown_arg = 0;
    upgrade_ypkg = 0;
    pm = NULL;


    while( ( c = getopt_long( argc, argv, ":hIiRASstCuUpydfr", longopts, NULL ) ) != -1 )
    {
        switch( c )
        {
            case 'I': //install
            case 'i': //install-dev
            case 'r': //reinstall
            case 'R': //remove
            case 'A': //autoremove
            case 'u': //update
            case 'U': //upgrade
                init++; //2 write

            case 's': //show
            case 'S': //search
            case 't': //status
            case 'C': //clean
                init++; //1 read


            case 'h': //show usage
                if( !action )
                    action = c;
                break;

            case 'y': //non-interactive
                yes = 1;
                break;

            case 'f': //force
                force = 1;
                break;

            case 'd':
                download_only = 1;
                break;

            case 'p': 
                simulation = 1;
                break;

            case '?':
                unknown_arg = 1;
                break;
        }
    }

    if( unknown_arg )
    {
        usage();
        return 1;
    }

    arg_cnt = argc - optind;

    if( init )
    {
        i = 0;
        do
        {
            sleep( 1 );
            pm = packages_manager_init2( init );
        }
        while( !pm && (i++ < 5) );

        //pm = packages_manager_init2( init );
        if( !pm && action != 's' )
        {
            switch( libypk_errno )
            {
                case MISSING_DB:
                    fprintf( stderr, "Error: open database failed.\n" );
                break;

                case LOCK_ERROR:
                    fprintf( stderr, "Error: database is locked.\n" );
                break;

                default:
                    fprintf( stderr, "Error: initialize failed.\n" );
            }
            return 1;
        }
    }

    switch( action )
    {
        /*
         * show usage
         */
        case 'h':
            usage();
            break;

        /*
         * remove package
         */
        case 'R':
            if( arg_cnt < 1 )
            {
                err = 1;
            }
            else if( geteuid() )
            {
                err = 2;
            }
            else
            {
                remove_list = NULL;
                for( i = optind; i < argc; i++)
                {

                    package_name = argv[i];

                    sub_list = packages_get_remove_list( pm, package_name, 0 );
                    if( sub_list )
                    {
                        dlist_cat( sub_list, remove_list );
                        dlist_cleanup( remove_list, packages_free_change_package );
                        remove_list = sub_list;
                    }
                    else
                    {
                        printf( "Error: Unable to locate package %s\n", package_name );
                    }
                }

                packages_clist_remove_duplicate_item( remove_list );

                confirm = 'N';

                if( remove_list )
                {
                    packages_clist_remove_duplicate_item( remove_list );
                    printf( "Remove:" );
                    cur_package = dlist_head_data( remove_list );
                    while( cur_package )
                    {
                        if( cur_package->type == 1 )
                            printf(" %s", cur_package->name );

                        cur_package = dlist_next_data( remove_list );;
                    }

                    printf( "\nAuto-remove:" );
                    cur_package = dlist_head_data( remove_list );
                    while( cur_package )
                    {
                        if( cur_package->type == 2 )
                            printf(" %s", cur_package->name );

                        cur_package = dlist_next_data( remove_list );;
                    }

                    if( yes )
                    {
                        confirm = 'Y';
                    }
                    else
                    {
                        printf( "\nDo you want to continue [y/N]?" );
                        confirm = getchar();
                    }

                    if( confirm == 'Y' || confirm == 'y' )
                    {
                        packages_remove_list( pm, remove_list, yget_progress_callback, pm );
                    }
                    packages_free_remove_list( remove_list );
                }
            }
            break;

        /*
         * autoremove
         */
        case 'A':
            if( geteuid() )
            {
                err = 2;
            }
            else
            {
                remove_list = NULL;
                pkg_count = packages_get_count( pm, NULL, NULL, NULL, 1 );
                if( pkg_count > 0 )
                {
                    pkg_list = packages_get_list( pm, pkg_count, 0, NULL, NULL, NULL, 1 );
                    if( pkg_list )
                    {
                        for( i = 0; i < pkg_list->cnt; i++ )
                        {
                            package_name = packages_get_list_attr( pkg_list, i, "name");
                            pkg_data = packages_get_package_data( pm, package_name, 1 );
                            if( pkg_data )
                            {
                                if( packages_check_depend( pm, pkg_data, NULL, 0 ) == -1 )
                                {
                                    if( !remove_list )
                                        remove_list = dlist_init();

                                    cur_package =  (YPackageChangePackage *)malloc( sizeof( YPackageChangePackage ) );
                                    len = strlen( package_name );
                                    cur_package->name = (char *)malloc( len + 1 );
                                    strncpy( cur_package->name, package_name, len );
                                    cur_package->name[len] = 0;
                                    cur_package->version = NULL;
                                    cur_package->type = 1;
                                    dlist_append( remove_list, cur_package );
                                }
                                packages_free_package_data( pkg_data );
                                pkg_data = NULL;
                            }
                        }
                        packages_free_list( pkg_list );
                    }
                }

                if( remove_list )
                {
                    printf( "Auto-remove:" );
                    cur_package = dlist_head_data( remove_list );
                    while( cur_package )
                    {
                        if( cur_package->type == 1 )
                            printf(" %s", cur_package->name );

                        cur_package = dlist_next_data( remove_list );
                    }

                    if( yes )
                    {
                        confirm = 'Y';
                    }
                    else
                    {
                        printf( "\nDo you want to continue [y/N]?" );
                        confirm = getchar();
                    }

                    if( confirm == 'Y' || confirm == 'y' )
                    {
                        packages_remove_list( pm, remove_list, yget_progress_callback, pm );
                    }
                    packages_free_remove_list( remove_list );
                }
                puts( "No package is unused." );
            }
            break;

        /*
         * install package
         */
        case 'r':
            force ?: (force = 2);

        case 'I':
            if( arg_cnt < 1 )
            {
                err = 1;
            }
            else if( geteuid() )
            {
                err = 2;
            }
            else
            {
                install_list = NULL;
                missing_list = NULL;
                for( i = optind; i < argc; i++)
                {
                    package_name = argv[i];

                    if( !packages_exists( pm, package_name, NULL ) )
                    {
                        printf( "Error: %s not found.\n",  package_name );
                        continue;
                    }

                    if( (pkg = packages_get_package( pm, package_name, 0 )) )
                    {
                        version = packages_get_package_attr( pkg, "version" );
                        if( !force )
                        {
                            if( packages_has_installed( pm, package_name, version ) )
                            {
                                printf( "%s_%s is already the newest version.\n", package_name, version );
                                packages_free_package( pkg );
                                pkg = NULL;
                                continue;
                            }
                        }
                    }
                    else
                    {
                        printf( "Error: %s not found.\n",  package_name );
                        continue;
                    }
                    if( !install_list )
                        install_list = dlist_init();

                    if( !packages_get_depend_list_recursively( pm, &sub_list, &missing_list, package_name, version, NULL, 1 ) )
                    {
                        dlist_cat( sub_list, install_list );
                        dlist_cleanup( install_list, packages_free_change_package );
                        install_list = sub_list;
                        sub_list = NULL;
                    }
                    else
                    {
                        dlist_cleanup( sub_list, packages_free_change_package );
                        sub_list = NULL;
                        err = 3;
                    }

                }

                if( missing_list )
                {
                    fprintf( stderr,  "Error: missing runtime dependencies:"  );

                    cur_package = dlist_head_data( missing_list );
                    while( cur_package )
                    {
                        printf(" %s ", cur_package->name );
                        cur_package = dlist_next_data( install_list );
                    }
                    putchar( '\n' );
                }
                else if( install_list )
                {
                    confirm = 'N';
                    
                    printf( "Install: " );
                    cur_package = dlist_head_data( install_list );
                    while( cur_package )
                    {
                        if( cur_package->type == 1 )
                            printf(" %s ", cur_package->name );

                        cur_package = dlist_next_data( install_list );
                    }

                    printf( "\nAuto-install: " );
                    cur_package = dlist_head_data( install_list );
                    while( cur_package )
                    {
                        if( cur_package->type == 2 ||  cur_package->type == 3 || cur_package->type == 6 )
                            printf(" %s ", cur_package->name );

                        cur_package = dlist_next_data( install_list );
                    }

                    /*
                    if( recommended_list )
                    {
                        cur_package = dlist_head_data( recommended_list );
                        printf( "\nRecommended-install: %s", cur_package->name );
                        cur_package = dlist_next_data( recommended_list );
                        while( cur_package )
                        {
                            printf(" %s ", cur_package->name );
                            cur_package = dlist_next_data( recommended_list );
                        }
                    }
                    */
                    putchar( '\n' );

                    if( !simulation )
                    {
                        if( yes )
                        {
                            confirm = 'Y';
                        }
                        else
                        {
                            printf( "Do you want to continue [Y/n]?" );
                            confirm = getchar();
                        }

                        if( confirm != 'n' && confirm != 'N' )
                        {
                            if( yget_install_list( pm, install_list, download_only, force ) )
                            {
                                err = 3;
                            }
                            else
                                err = 3;
                        }
                        else
                            err = 3;
                    }

                    if( pkg )
                    {
                        packages_free_package( pkg );
                        pkg = NULL;
                    }

                }

                if( install_list )
                {
                    packages_free_install_list( install_list );
                    install_list = NULL;
                }

                if( missing_list )
                {
                    packages_free_install_list( missing_list );
                    missing_list = NULL;
                }
            }
            break;

        /*
         * install dev
         */
        case 'i':
            if( arg_cnt < 1 )
            {
                err = 1;
            }
            else if( geteuid() )
            {
                err = 2;
            }
            else
            {
                for( i = optind; i < argc; i++)
                {
                    package_name = argv[i];
                    install_list = packages_get_dev_list( pm, package_name, NULL );
                    confirm = 'N';

                    if( install_list && install_list->cnt > 0 )
                    {
                        cur_package = dlist_head_data( install_list );
                        printf( "Install list: %s", cur_package->name );
                        cur_package = dlist_next_data( install_list );
                        while( cur_package )
                        {
                            printf(" %s ", cur_package->name );
                            cur_package = dlist_next_data( install_list );
                        }

                        if( yes )
                        {
                            confirm = 'Y';
                        }
                        else
                        {
                            printf( "\nDo you want to continue [y/N]?" );
                            confirm = getchar();
                        }

                        if( confirm == 'Y' || confirm == 'y' )
                        {
                            yget_install_list( pm, install_list, 0, force );
                        }

                    }
                    else
                    {
                        printf( "Build-dependencies for %s installed.\n", package_name );
                    }

                    if( install_list )
                        packages_free_dev_list( install_list );
                }
            }
            break;

        /*
         * Clean
         */
        case 'C':
            if( yes )
            {
                confirm = 'Y';
            }
            else
            {
                printf( "Do you want to remove all downloaded packages? [y/N]?" );
                confirm = getchar();
            }
            if( confirm == 'Y' || confirm == 'y' )
            {
                packages_cleanup_package( pm );
            }
            break;
        /*
         * Search
         */
        case 'S':
            if( arg_cnt < 1 )
            {
                err = 1;
            }
            else
            {
                //printf( "Status \tName  \tChannel  \tDescription\n====================================================================\n" );
                printf( "Name  \tChannel  \tDescription\n====================================================================\n" );

                for( i = optind; i < argc; i++)
                {
                    package_name = argv[i];

                    char *keys[] = { "*", NULL }; 
                    char *keywords[] = { package_name, NULL }; 
                    int wildcards[] = { 2,  0 }; 

                    pkg_count = packages_get_count( pm,  keys, keywords, wildcards, 0 );
                    if( pkg_count > 0 )
                    {

                        pkg_list = packages_get_list( pm, pkg_count, 0, keys, keywords, wildcards, 0 );
                        if( pkg_list )
                        {
                            for( j = 0; j < pkg_list->cnt; j++ )
                            {
                                /*
                                installed = packages_get_list_attr( pkg_list, j, "installed" );
                                can_update = packages_get_list_attr( pkg_list, j, "can_update" );

                                if( installed && installed[0] == '0' )
                                {
                                    installed = "[*]";
                                }
                                else if( can_update[0] == '1' )
                                {
                                    installed = "[U]";
                                }
                                else if( can_update[0] == '-' &&  can_update[1] == '1' )
                                {
                                    installed = "[D]";
                                }
                                else
                                {
                                    installed = "[I]";
                                }
                                */

                                repo = packages_get_list_attr( pkg_list, j, "repo" );

                                printf( 
                                        "%-s \t%-8s \t%-s\n",
                                        //if( colorize )
                                            //COLOR_GREEN "%-s "  COLOR_RESET  "\t%-s \t%-8s \t%-s\n",
                                            //else
                                            // "%-s "    "\t%-s \t%-8s \t%-s\n",
                                        //installed, 
                                        packages_get_list_attr( pkg_list, j, "name"), 
                                        repo, 
                                        packages_get_list_attr( pkg_list, j, "description") 
                                        );

                            }
                            packages_free_list( pkg_list );
                        }
                        else
                        {
                            if( colorize )
                                printf( COLOR_RED "%s not found\n" COLOR_RESET,  package_name );
                            else
                                printf(  "%s not found\n" ,  package_name );
                        }
                    }
                    else
                    {
                        if( colorize )
                            printf( COLOR_RED "%s not found\n" COLOR_RESET,  package_name );
                        else
                            printf(  "%s not found\n" ,  package_name );
                    }
                }
            }

            break;

        /*
         * Show
         */
        case 's':
            if( arg_cnt != 1  )
            {
                err = 1;
            }
            else
            {
                package_name = argv[optind];
                pkg = NULL;
                pkg2 = NULL;
                pkg_data = NULL;
                install_date = NULL;
                build_date = NULL;

                len = strlen( package_name );
                if( strncmp( package_name+len-4, ".ypk", 4 ) || access( package_name, R_OK ) )
                {
                    if( !pm )
                    {
                        switch( libypk_errno )
                        {
                            case MISSING_DB:
                                fprintf( stderr, "Error: open database failed.\n" );
                            break;

                            case LOCK_ERROR:
                                fprintf( stderr, "Error: database is locked.\n" );
                            break;

                            default:
                                fprintf( stderr, "Error: initialize failed.\n" );
                        }
                        return 1;
                    }
                    i = 1;
                }
                else
                {
                    i = 0;
                }

                if( i )
                {
                    pkg = packages_get_package( pm, package_name, 0 );
                }
                else
                {
                    if( packages_get_package_from_ypk( package_name, &pkg, &pkg_data ) < 0 )
                    {
                        if( colorize )
                            printf( COLOR_RED "%s not found\n" COLOR_RESET,  package_name );
                        else
                            printf(  "%s not found\n" ,  package_name );
                        return 1;
                    }
                }

                if( pkg )
                {

                    tmp = packages_get_package_attr( pkg, "build_date");
                    if( tmp )
                        build_date = util_time_to_str( atoi( tmp ) );
                    else
                        build_date = NULL;


                    tmp = packages_get_package_attr( pkg, "size");
                    size = tmp ? atoi( tmp ) : 0;

                    if( i )
                    {
                        pkg_data = packages_get_package_data( pm, package_name, 0 );
                    }

                    if( pkg_data )
                    {
                        tmp = packages_get_package_data_attr( pkg_data, 0, "data_install_size");
                        install_size = tmp ? atoi( tmp ) : 0;

                        repo = packages_get_package_attr( pkg, "repo");
                        if(repo == NULL)
                        {
                            repo = "stable";
                        }

                        printf( 
                                "Name: %s\nVersion: %s\nArch: %s\nRepo: %s\nCategory: %s\nPriority: %s\nLicense: %s\nPackager: %s\nInstall Script: %s\nExec: %s\nSize: %d%c\nSha: %s\nBuild_date: %s\nUri: %s\nInstall_size: %d%c\nDepend: %s\nBdepend: %s\nRecommended: %s\nConflict: %s\nReplace: %s\nDescription: %s\nHomepage: %s\n", 
                                package_name,
                                pkg2 ? packages_get_package_attr( pkg2, "version") : packages_get_package_attr( pkg, "version"), 
                                packages_get_package_attr( pkg, "arch"), 
                                repo,
                                util_null2empty( packages_get_package_attr( pkg, "category") ), 
                                util_null2empty( packages_get_package_attr( pkg, "priority") ), 
                                util_null2empty( packages_get_package_attr( pkg, "license") ), 
                                util_null2empty( packages_get_package_attr( pkg, "packager") ), 
                                util_null2empty( packages_get_package_attr( pkg, "install") ), 
                                util_null2empty( packages_get_package_attr2( pkg, "exec") ), 
                                size > 1000000 ? size / 1000000 : (size > 1000 ? size / 1000 : size), 
                                size > 1000000 ? 'M' : (size > 1000 ? 'K' : 'B'), 
                                util_null2empty( packages_get_package_attr( pkg, "sha") ), 
                                util_null2empty( build_date ),
                                util_null2empty( packages_get_package_attr( pkg, "uri") ), 
                                install_size > 1000000 ? install_size / 1000000 : (install_size > 1000 ? install_size / 1000 : install_size), 
                                install_size > 1000000 ? 'M' : (install_size > 1000 ? 'K' : 'B'), 
                                util_null2empty( util_chr_replace( packages_get_package_data_attr( pkg_data, 0, "data_depend"), ',', ' ' ) ),  //depend
                                util_null2empty( util_chr_replace( packages_get_package_data_attr( pkg_data, 0, "data_bdepend"), ',', ' ' ) ),  //bdepend
                                util_null2empty( util_chr_replace( packages_get_package_data_attr( pkg_data, 0, "data_recommended"), ',', ' ' ) ),  //recommended
                                util_null2empty( packages_get_package_data_attr( pkg_data, 0, "data_conflict") ),  //conflict
                                util_null2empty( packages_get_package_data_attr( pkg_data, 0, "data_replace") ),  //replace
                                util_null2empty( packages_get_package_attr( pkg, "description") ),
                                util_null2empty( packages_get_package_attr( pkg, "homepage") )
                                );
                    }

                    if( build_date )
                        free( build_date );

                    if( install_date )
                        free( install_date );

                    packages_free_package_data( pkg_data );
                    pkg_data = NULL;
                    packages_free_package( pkg );
                    pkg = NULL;
                }
                else
                {
                    if( colorize )
                        printf( COLOR_RED "%s not found\n" COLOR_RESET,  package_name );
                    else
                        printf(  "%s not found\n" ,  package_name );
                }

            }

            break;

        /*
         * status
         */
        case 't':
            if( arg_cnt != 1 )
            {
                err = 1;
            }
            else
            {
                package_name = argv[optind];
                if( (pkg = packages_get_package( pm, package_name, 1 )) )
                {
                    pkg_data = packages_get_package_data( pm, package_name, 1 );

                    can_update = packages_get_package_attr( pkg, "can_update" );

                    tmp = packages_get_package_attr( pkg, "install_time");
                    if( tmp )
                        install_date = util_time_to_str( atoi( tmp ) );
                    else
                        install_date = NULL;


                    if( can_update[0] == '1' )
                    {
                        installed = "U";
                    }
                    else
                    {
                        installed = "I";
                    }

                    repo = packages_get_package_attr( pkg, "repo");
                    if(repo == NULL)
                    {
                        repo = "stable";
                    }

                    tmp = packages_get_package_attr( pkg, "build_date");
                    if( tmp )
                        build_date = util_time_to_str( atoi( tmp ) );
                    else
                        build_date = NULL;

                    tmp = packages_get_package_attr( pkg, "size");
                    size = tmp ? atoi( tmp ) : 0;
                    tmp = NULL;

                    if( pkg_data )
                        tmp = packages_get_package_data_attr( pkg_data, 0, "data_install_size");

                    install_size = tmp ? atoi( tmp ) : 0;

                    printf( 
                            "Name: %s\nVersion: %s\nArch: %s\nRepo: %s\nCategory: %s\nPriority: %s\nStatus: %s\nInstall_date: %s\nAvailable: %s\nLicense: %s\nPackager: %s\nInstall Script: %s\nExec: %s\nSize: %d%c\nBuild_date: %s\nInstall_size: %d%c\nDepend: %s\nBdepend: %s\nRecommended: %s\nConflict: %s\nReplace: %s\nDescription: %s\nHomepage: %s\n", 
                            package_name,
                            packages_get_package_attr( pkg, "version"),
                            packages_get_package_attr( pkg, "arch"), 
                            repo,
                            util_null2empty( packages_get_package_attr( pkg, "category") ), 
                            util_null2empty( packages_get_package_attr( pkg, "priority") ), 
                            installed,
                            util_null2empty( install_date ),  //install date
                            util_null2empty( packages_get_package_attr( pkg, "version_available") ), 
                            util_null2empty( packages_get_package_attr( pkg, "license") ), 
                            util_null2empty( packages_get_package_attr( pkg, "packager") ), 
                            util_null2empty( packages_get_package_attr( pkg, "install") ), 
                            util_null2empty( packages_get_package_attr2( pkg, "exec") ), 
                            size > 1000000 ? size / 1000000 : (size > 1000 ? size / 1000 : size), 
                            size > 1000000 ? 'M' : (size > 1000 ? 'K' : 'B'), 
                            util_null2empty( build_date ),
                            install_size > 1000000 ? install_size / 1000000 : (install_size > 1000 ? install_size / 1000 : install_size), 
                            install_size > 1000000 ? 'M' : (install_size > 1000 ? 'K' : 'B'), 
                            util_null2empty( util_chr_replace( packages_get_package_data_attr( pkg_data, 0, "data_depend"), ',', ' ' ) ),  //depend
                            util_null2empty( util_chr_replace( packages_get_package_data_attr( pkg_data, 0, "data_bdepend"), ',', ' ' ) ),  //bdepend
                            util_null2empty( util_chr_replace( packages_get_package_data_attr( pkg_data, 0, "data_recommended"), ',', ' ' ) ),  //recommended
                            util_null2empty( packages_get_package_data_attr( pkg_data, 0, "data_conflict") ),  //conflict
                            util_null2empty( packages_get_package_data_attr( pkg_data, 0, "data_replace") ),  //replace
                            util_null2empty( packages_get_package_attr( pkg, "description") ),
                            util_null2empty( packages_get_package_attr( pkg, "homepage") )
                            );

                    if( build_date )
                        free( build_date );

                    if( install_date )
                        free( install_date );

                    packages_free_package( pkg );
                    pkg = NULL;
                    packages_free_package_data( pkg_data );
                    pkg_data = NULL;
                }
                else
                {
                    if( colorize )
                        printf( COLOR_RED "%s not found\n" COLOR_RESET,  package_name );
                    else
                        printf(  "%s not found\n" ,  package_name );
                }
            }

            break;

        /*
         * Upgrade
         */
        case 'U':
            if( geteuid() )
            {
                err = 2;
            }
            else
            {
                    install_list = NULL;
                    //recommended_list = NULL;
                    confirm = 'N';

                    upgrade_list = packages_get_upgrade_list( pm );

                    if( upgrade_list )
                    {
                        //cur_package = upgrade_list;
                        cur_package = dlist_head_data( upgrade_list );

                        while( cur_package )
                        {
                            if( !strcmp( cur_package->name, "ypkg2" ) )
                            {
                                upgrade_ypkg = 1;
                            }

                            if( !packages_get_depend_list_recursively( pm, &sub_list, &missing_list, cur_package->name, cur_package->version, NULL, 1 ) )
                            {
                                dlist_cat( sub_list, install_list );
                                dlist_cleanup( install_list, packages_free_change_package );
                                install_list = sub_list;
                                sub_list = NULL;
                            }
                            else
                            {
                                dlist_cleanup( sub_list, packages_free_change_package );
                                sub_list = NULL;
                                err = 3;
                            }

                            if( sub_list )
                            {
                                if( !install_list )
                                {
                                    install_list = sub_list;
                                }
                                else
                                {
                                    dlist_cat( install_list, sub_list );
                                    dlist_cleanup( sub_list, packages_free_change_package );
                                }
                            }

                            cur_package = dlist_next_data( upgrade_list );
                        }
                        packages_free_upgrade_list( upgrade_list );

                        packages_clist_remove_duplicate_item( install_list );

                        printf( "Upgrade:" );
                        cur_package = dlist_head_data( install_list );
                        while( cur_package )
                        {
                            if( cur_package->type == 1 )
                                printf(" %s ", cur_package->name );

                            cur_package = dlist_next_data( install_list );
                        }

                        printf( "\nAuto-install: " );
                        cur_package = dlist_head_data( install_list );
                        while( cur_package )
                        {
                            if( cur_package->type == 2 ||  cur_package->type == 3 || cur_package->type == 6 )
                                printf(" %s ", cur_package->name );

                            cur_package = dlist_next_data( install_list );
                        }

                        putchar( '\n' );

                        if( yes )
                        {
                            confirm = 'Y';
                        }
                        else
                        {
                            printf( "\nDo you want to continue [y/N]?" );
                            confirm = getchar();
                        }

                        if( confirm == 'Y' || confirm == 'y' )
                        {
                            if( upgrade_ypkg ) //upgrade self
                            {
                                if( yget_install_package( pm, "ypkg2", NULL, 0, 1, 0 ) != 0 )
                                {
                                    err = 3;
                                }
                            }
                            else //normal upgrade
                            {
                                if( yget_install_list( pm, install_list, download_only, force ) )
                                {
                                    err = 3;
                                }
                            }
                        }

                        packages_free_upgrade_list( install_list );
                    }
                    else
                    {
                        printf( "Done.\n" );
                    }
            }

            break;

        /*
         * update
         */
        case 'u':
            if( geteuid() )
            {
                err = 2;
            }
            else
            {
                ret = packages_check_update( pm );
                if(ret)
                {
                    if( packages_update( pm, yget_progress_callback, pm ) == -1 )
                    {
                        err = 3;
                        //printf("Failed!\n");
                    }
                    else
                    {
                        printf("Done.\n");
                    }

                }
                else
                {
                    printf("Done.\n");
                }
            }
            break;

        default:
            usage();
    }

    if( pm )
        packages_manager_cleanup2( pm );

    if( err == 1 )
        usage();
    else if( err == 2 )
        fprintf( stderr, "Permission Denied!\n" );
    else if( err == 3 )
        fprintf( stderr, "Failed!\n" );

    return err;
}
