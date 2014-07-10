/* Database operating functions
 *
 * Copyright (c) 2013 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Date: 2013.3.5
 */
#ifndef DB_H
#define DB_H

#include <sqlite3.h>

#define OPEN_READ       1
#define OPEN_WRITE      2

#define DB_HashTable_SIZE 32

#define RESULT_NUM      1
#define RESULT_ASSOC    2
#define RESULT_BOTH     3

typedef struct _DB {
    sqlite3                 *handle;
    sqlite3_stmt            *stmt;
    int                     column_count;
    char                    **crow;
    struct hsearch_data     *ht;
} DB;

int db_init( DB *db, char *db_path, int open_mode );
int db_close(DB *db);
int db_cleanup( DB *db );
int db_exec(DB *db, char *sql, ...);

int db_query(DB *db, char *sql, ...);
int db_fetch_row( DB *db, int result_type );
#define db_fetch_assoc(db) db_fetch_row(db, RESULT_ASSOC)
#define db_fetch_num(db) db_fetch_row(db, RESULT_NUM)
#define db_fetch_both(db) db_fetch_row(db, RESULT_BOTH)
char *db_get_value_by_key( DB *db, char *key );
char *db_get_value_by_index( DB *db, int index );

int db_create_hash_table( DB *db );
void db_destory_hash_table( DB *db );

sqlite3_int64 db_last_insert_rowid( DB *db );

int db_create_collation( DB *db, const char *name, int(*cmp_func)(void*,int,const void*,int,const void*), void *arg );
int db_create_function( DB *db, const char *name, int argc, void (*xFunc)(sqlite3_context*,int,sqlite3_value**), void (*xStep)(sqlite3_context*,int,sqlite3_value**), void (*xFinal)(sqlite3_context*) );

#endif /* !DB_H */
