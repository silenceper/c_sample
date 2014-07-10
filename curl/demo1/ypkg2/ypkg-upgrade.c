#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include "ypackage.h"

int main( int argc, char **argv )
{
    int                 i, ret, pid, force;
    char                *exec_path, *package_path, *dl_error;
    void                *dl_handle = NULL;

    YPackageManager     *pm;
    YPackageManager     *(*packages_manager_init2)() = NULL;
    void                (*packages_manager_cleanup2)( YPackageManager *pm ) = NULL;
    int                 (*packages_install_local_package)( YPackageManager *pm, char *ypk_path, char *dest_dir, int force, ypk_progress_callback cb, void *cb_arg ) = NULL;
    //ssize_t             (*packages_upgrade_db)( YPackageManager *pm ) = NULL;

    if( argc < 3 || argc > 4 )
        return 1;

    exec_path = argv[1];
    package_path = argv[2];
    force = argv[3] && argv[3][0] == '1' ? 1 : 0;

    if( access( exec_path, F_OK ) )
    {
        ret = 2;
        goto exception_handler;
    }

    if( access( package_path, F_OK ) )
    {
        ret = 2;
        goto exception_handler;
    }

    if( chdir( exec_path ) == -1 )
    {
        ret = 2;
        goto exception_handler;
    }

    if( access( "ypkg2", F_OK ) || access( "yget2", F_OK ) || access( "libypk.so", F_OK ) || access( "ypkg2-upgrade", F_OK ) )
    {
        ret = 2;
        goto exception_handler;
    }

    dl_handle = dlopen( "./libypk.so", RTLD_LAZY | RTLD_GLOBAL );
    if( !dl_handle )
    {
        ret = 3;
        fprintf( stderr, "Error: %s\n", dlerror() );
        goto exception_handler;
    }

    dlerror();

    packages_manager_init2 = dlsym( dl_handle, "packages_manager_init2" );
    if( (dl_error = dlerror()) != NULL )
    {
        ret = 3;
        fprintf( stderr, "Error: %s\n", dlerror() );
        goto exception_handler;
    }

    packages_manager_cleanup2 = dlsym( dl_handle, "packages_manager_cleanup2" );
    if( (dl_error = dlerror()) != NULL )
    {
        ret = 3;
        fprintf( stderr, "Error: %s\n", dlerror() );
        goto exception_handler;
    }

    packages_install_local_package = dlsym( dl_handle, "packages_install_local_package" );
    if( (dl_error = dlerror()) != NULL )
    {
        ret = 3;
        fprintf( stderr, "Error: %s\n", dlerror() );
        goto exception_handler;
    }

    /*
    packages_upgrade_db = dlsym( dl_handle, "packages_upgrade_db" );
    if( (dl_error = dlerror()) != NULL )
    {
        ret = 3;
        fprintf( stderr, "Error: %s\n", dlerror() );
        goto exception_handler;
    }
    */

    i = 0;

    do
    {
        sleep( 1 );
        pm = packages_manager_init2( 2 );
    }
    while( !pm && (i++ < 5) );

    if( !pm )
    {
        ret = 4;
        fprintf( stderr, "Error: open database failed.\n" );
        goto exception_handler;
    }



    if( packages_install_local_package( pm, package_path, "/", force, NULL, NULL ) != 0 )
    {
        ret = 5;
        fprintf( stderr, "Error: upgrade failed.\n" );
        goto exception_handler;
    }

    /*
    if( packages_upgrade_db( pm ) != 0 )
    {
        ret = 5;
        fprintf( stderr, "Error: Upgrade failed.\n" );
        goto exception_handler;
    }
    */

    printf( "Upgrade completed.\n" );

    packages_manager_cleanup2( pm );
    dlclose( dl_handle );

    if( (pid = fork()) < 0 )
    {
        ret = 6;
    }
    else if( pid == 0 )
    {
        sleep( 1 );
        if( execl( "/usr/bin/yget2", "yget2", "--upgrade", "-y", NULL ) < 0 )
        ret = 6;
    }

    return 0;

exception_handler:
    if( ret == 5 )
    {
        system( "cp libypk.so /usr/lib/libypk.so" );
        system( "cp ypkg2 /usr/bin/ypkg2" );
        system( "cp yget2 /usr/bin/yget2" );
        system( "cp ypkg2-upgrade /usr/bin/ypkg2-upgrade" );
    }

    if( dl_handle )
        dlclose( dl_handle );

    packages_manager_cleanup2( pm );

    return ret;
}
