#include <stdio.h>
#include <unistd.h>
#include "ypackage.h"

int main()
{
    YPackageManager *pm;

    if( geteuid() )
    {
        fprintf( stderr, "Permission Denied!\n" );
        return 1;
    }

    pm = packages_manager_init();
    if( !pm )
    {
        fprintf( stderr, "Error: open database failed.\n" );
        return 1;
    }

    packages_import_local_data( pm );
    packages_manager_cleanup( pm );
    return 0;
}
