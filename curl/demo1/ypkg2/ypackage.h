/* Libypk
 *
 * Copyright (c) 2013 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Date: 2013.3.11
 */
#ifndef PACKAGE_H
#define PACKAGE_H

#include <sqlite3.h>
#include "data.h"


#define CONFIG_FILE "/etc/yget.conf"
#define CONFIG_DIR "/etc/yget.conf.d"
#define DEFAULT_REPO "stable"
#define DEFAULT_URI "http://pkg.startos.org/packages"
#define DEFAULT_PKGDEST "/var/ypkg/packages"
#define LOG_FILE "/var/log/ypkg2.log"

#define PACKAGE_DB_DIR  "/var/ypkg/db"
#define DB_NAME "/var/ypkg/db/package.db"
#define DB_UPGRADE "/usr/share/ypkg/db_upgrade.list"
#define DB_SOURCE_TEMPLATE "/usr/share/ypkg/db_source_template.sql"
#define LOCK_FILE "/tmp/libypk.lock"
#define UPDATE_DIR "updates"
#define LIST_FILE "updates.list"
#define LIST_DATE_FILE "updates.date"
#define PACKAGE_ATTRS_LINE_LEN 1024

#define LOCAL_UNIVERSE "/var/ypkg/packages/universe"
#define LOCAL_WORLD "/var/ypkg/db/world"

#define ARGS_INCORRECT -1
#define MISSING_CONFIG -2
#define MISSING_DB -3
#define MISCONFIG -4
#define LOCK_ERROR -5
#define OTHER_FAILURES -6


typedef struct {
    char    *source_name;
    char    *source_uri;
    char    *accept_repo;
    char    *package_dest;
    int     updated;
}YPackageSource;

typedef DList YPackageSourceList;

typedef struct {
    int     lock_fd;
    //char    *source_uri;
    //char    *accept_repo;
    //char    *package_dest;
    char    *db_name;
    char    *log;
    YPackageSourceList   *source_list;
}YPackageManager;

typedef struct {
    HashTable              *ht;
}YPackage;

typedef struct {
    int                     cnt;
    HashTableList         *htl;
}YPackageData;

typedef struct {
    int                     cnt;
    int                     cnt_file;
    int                     cnt_link;
    int                     cnt_dir;
    int                     size;
    HashTableList         *htl;
}YPackageFile;


typedef struct {
    int                 max_cnt;
    int                 cur_cnt;
    HashTableList         *htl;
}YPackageReplaceFileList;

typedef struct {
    int                     cnt;
    HashTableList         *htl;
}YPackageList;



typedef struct _YPackageChangePackage {
    char                    *name;
    char                    *version;
    int                     size;
    int                     type; //self:1 ,depend:2, recommended:3, upgrade: 4, downgrade: 5, recursive_depend: 6, begin_tag: 7, end_tag: 8
}YPackageChangePackage;

typedef DList YPackageChangeList;

/*
typedef int (*ypk_download_callback)( void *cb_arg, double dltotal, double dlnow );
typedef struct {
    ypk_download_callback   cb;
    void                    *arg;
}YPackageDCB;
*/

typedef int (*ypk_download_callback)( void *cb_arg, char *package_name, double dltotal, double dlnow );
typedef int (*ypk_progress_callback)( void *cb_arg, char *package_name, int action, double progress, char *msg );
typedef struct {
    char                    *package_name;
    ypk_progress_callback   pcb;
    void                    *pcb_arg;
    ypk_download_callback   dcb;
    void                    *dcb_arg;
}YPackageDCB;


extern int libypk_errno;


/*
 * init & clean up
 */
YPackageManager *packages_manager_init();
void packages_manager_cleanup( YPackageManager *pm );

YPackageManager *packages_manager_init2( int type );
void packages_manager_cleanup2( YPackageManager *pm );

int packages_manager_add_source( YPackageSourceList *list, char *source_name, char *source_uri, char *accept_repo, char *package_dest );

void packages_manager_free_source( void *node );
/*
 * lock
 */
int packages_lock( int type );
int packages_unlock();
#define packages_read_lock() packages_lock( 1 )
#define packages_write_lock() packages_lock( 2 )

/*
 * update database
 */
int packages_check_update( YPackageManager *pm );

int packages_add_source( YPackageManager *pm, char *source_name, char *repo );

int packages_update( YPackageManager *pm, ypk_progress_callback cb, void *cb_arg );

int packages_update_single_xml( YPackageManager *pm, YPackageSource *source, char *xml_file, char *sum,  ypk_progress_callback cb, void *cb_arg  );

int packages_import_local_data( YPackageManager *pm );

int packages_upgrade_db( YPackageManager *pm );

/*
 * get package infomations
 */

/* get a single package infomations */
int packages_get_count( YPackageManager *pm, char *keys[], char *keywords[], int wildcards[], int installed  );
int packages_has_installed( YPackageManager *pm, char *name, char *version );
int packages_exists( YPackageManager *pm, char *name, char *version );

int packages_get_info_from_ypk( char *ypk_path, YPackage **package, YPackageData **package_data, YPackageFile **package_file, char *install_script, char *desktop_file, char *icon );

int packages_get_package_from_ypk( char *ypk_path, YPackage **package, YPackageData **package_data );

YPackage *packages_get_repo_package( YPackageManager *pm, char *name, int installed, char *repo );
YPackage *packages_get_package( YPackageManager *pm, char *name, int installed );
char *packages_get_package_attr( YPackage *pkg, char *key );
char *packages_get_package_attr2( YPackage *pkg, char *key );
void packages_free_package( YPackage *pkg );


YPackageData *packages_get_package_data( YPackageManager *pm, char *name, int installed );
char *packages_get_package_data_attr( YPackageData *pkg_data, int index, char *key );
char *packages_get_package_data_attr2( YPackageData *pkg_data, int index, char *key );
void packages_free_package_data( YPackageData *pkg_data );


/* get package file infomations */
YPackageFile *packages_get_package_file( YPackageManager *pm, char *name );
YPackageFile *packages_get_package_file_from_str( char *filelist );
YPackageFile *packages_get_package_file_from_ypk( char *ypk_path );
char *packages_get_package_file_attr( YPackageFile *pkg_file, int index, char *key );
char *packages_get_package_file_attr2( YPackageFile *pkg_file, int index, char *key );
void packages_free_package_file( YPackageFile *pkg_file );


YPackageReplaceFileList *packages_get_replace_file_list( YPackageManager *pm, YPackageData *pkg_data, YPackageFile *pkg_file );
void packages_free_replace_file_list( YPackageReplaceFileList *list );

/* get package list */
YPackageList *packages_get_list( YPackageManager *pm, int limit, int offset, char *keys[], char *keywords[], int wildcards[], int installed );

YPackageList *packages_get_list2( YPackageManager *pm, int page_size, int page_no, char *keys[], char *keywords[], int wildcards[], int installed );


YPackageList *packages_search_world_data( YPackageManager *pm, int limit, int offset, char *key, char *keyword );

YPackageList *packages_get_list_by_depend( YPackageManager *pm, int limit, int offset, char *depend, int installed );

YPackageList *packages_get_list_by_bdepend( YPackageManager *pm, int limit, int offset, char *bdepend, int installed );

YPackageList *packages_get_list_by_conflict( YPackageManager *pm, int limit, int offset, char *conflict, int installed );

YPackageList *packages_get_list_by_recommended( YPackageManager *pm, int limit, int offset, char *recommended, int installed );

YPackageList *packages_get_list_by_file( YPackageManager *pm, int limit, int offset, char *file, int absolute );

char *packages_get_list_attr( YPackageList *pkg_list, int index, char *key );
char *packages_get_list_attr2( YPackageList *pkg_list, int index, char *key );
void packages_free_list( YPackageList *pkg_list );


/*
 * compare version
 */
int packages_compare_version( char *version1, char *version2 );
int packages_compare_version_collate( void *arg, int len1, const void *version1, int len2, const void *version2 );
void packages_max_version_step( sqlite3_context *context, int argc, sqlite3_value **argv );
void packages_max_version_final( sqlite3_context *context );

/*
 * package install & remove & upgrade
 */
int packages_check_depend( YPackageManager *pm, YPackageData *pkg_data, char *extra, int extra_max_len );
int packages_check_conflict( YPackageManager *pm, YPackageData *pkg_data, char *extra, int extra_max_len );
int packages_check_package( YPackageManager *pm, char *ypk_path, char *extra, int extra_max_len );
int packages_check_package2( YPackageManager *pm, YPackage *pkg, YPackageData *pkg_data, char *extra, int extra_max_len );

int packages_unpack_package( char *ypk_path, char *dest_dir, int unzip_info, char *suffix );
int packages_pack_package( char *source_dir, char *ypk_path, ypk_progress_callback cb, void *cb_arg );


//int packages_download_package( YPackageManager *pm, char *package_name, char *url, char *dest, int force, ypk_progress_callback cb, void *cb_arg );
int packages_download_package( YPackageManager *pm, char *package_name, char *url, char *dest, int force, ypk_download_callback dcb, void *dcb_arg, ypk_progress_callback pcb, void *pcb_arg  );
int packages_exec_script( char *script, char *package_name, char *version, char *version2, char *action );
int packages_install_local_package( YPackageManager *pm, char *ypk_path, char *dest_dir, int force, ypk_progress_callback cb, void *cb_arg );
int packages_install_package( YPackageManager *pm, char *package_name, char *version, ypk_progress_callback cb, void *cb_arg  );
//int packages_install_history_package( YPackageManager *pm, char *package_name, char *version );

void packages_free_change_package( void *node );
int packages_clist_package_cmp( void *pkg1, void *pkg2 );
int packages_clist_append( YPackageChangeList *list, char *name, char *version, int size, int type );
YPackageChangeList *packages_clist_remove_duplicate_item( YPackageChangeList *change_list );


YPackageChangeList *packages_get_install_list( YPackageManager *pm, char *package_name, char *version );
int packages_get_depend_list_recursively( YPackageManager *pm, YPackageChangeList **depend_list_p, YPackageChangeList **missing_list_p,  char *package_name, char *version, char *skip, int self_type );
YPackageChangeList *packages_get_depend_list( YPackageManager *pm, char *package_name, char *version, char *skip );
YPackageChangeList *packages_get_recommended_list( YPackageManager *pm, char *package_name, char *version );
YPackageChangeList *packages_get_bdepend_list( YPackageManager *pm, char *package_name, char *version );

void packages_free_install_list( YPackageChangeList *list );


YPackageChangeList *packages_get_dev_list( YPackageManager *pm, char *package_name, char *version );
void packages_free_dev_list( YPackageChangeList *list );

int packages_install_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg  );

int packages_remove_package( YPackageManager *pm, char *package_name, ypk_progress_callback cb, void *cb_arg  );
YPackageChangeList *packages_get_remove_list( YPackageManager *pm, char *package_name, int depth );
void packages_free_remove_list( YPackageChangeList *list );
int packages_remove_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg  );


YPackageChangeList *packages_get_upgrade_list( YPackageManager *pm );
int packages_upgrade_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg   );
void packages_free_upgrade_list( YPackageChangeList *list );

//auto remove
int packages_cleanup_package( YPackageManager *pm );

/*
 * log
 */
int packages_log( YPackageManager *pm, char *package_name, char *msg );

/**********************************/
/* internal functions             */
/**********************************/
int packages_compare_main_version( char *version1, char *version2 );
int packages_compare_sub_version( char *version1, char *version2 );

YPackageManager *packages_manager_clone(  YPackageManager *pm );


int packages_get_last_check_timestamp( YPackageManager *pm, char *source_name, char *repo );
int packages_set_last_check_timestamp( YPackageManager *pm, char *source_name, char *repo, int last_check );

int packages_get_last_update_timestamp( YPackageManager *pm, char *source_name, char *repo );
int packages_set_last_update_timestamp( YPackageManager *pm, char *source_name, char *repo, int last_check );

char *packages_get_source_checksum( YPackageManager *pm, char *source_name, char *repo );
int packages_set_source_checksum( YPackageManager *pm, char *source_name, char *repo, char *checksum );


void packages_free_change_list( YPackageChangeList *list );

void *packages_check_update_backend_thread( void *data );
void *packages_update_backend_thread( void *data );
void *packages_get_list_backend_thread(void *data);
int packages_download_progress_callback( void *cb_arg,double dltotal, double dlnow, double ultotal, double ulnow );
#endif /* !PACKAGE_H */
