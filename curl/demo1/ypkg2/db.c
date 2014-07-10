/* Database operating functions
 *
 * Copyright (c) 2013 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Date: 2013.3.5
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <stdarg.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <search.h>

#include "db.h"

int db_init( DB *db, char *db_path, int open_mode )
{
    int extra_mode = 0;

    if( !open_mode || open_mode == OPEN_READ )
        extra_mode = SQLITE_OPEN_READONLY;  

    if( open_mode == OPEN_WRITE )
        extra_mode = SQLITE_OPEN_READWRITE;  

    //if(sqlite3_open( db_path, &(db->handle) ) != SQLITE_OK)
    if(sqlite3_open_v2( db_path, &(db->handle), extra_mode | SQLITE_OPEN_FULLMUTEX, NULL ) != SQLITE_OK)
    {
        return -1;
    }
    db->stmt = NULL;
    db->crow = NULL;
    db->ht = NULL;

    return 0;
}

int db_close( DB *db )
{
    db_cleanup( db );
    sqlite3_close(db->handle);

    return 0;
}

int db_cleanup( DB *db )
{
    if(db->stmt)
    {
        sqlite3_finalize( db->stmt );
        db->stmt = NULL;
    }

    if(db->crow)
    {
        free(db->crow);
        db->crow = NULL;
    }

    db_destory_hash_table( db );

    return 0;
}

int db_exec(DB *db, char *sql, ...)
{
    va_list ap;
    char *arg;
    int i = 1;
    int ret;


    sqlite3_prepare_v2 ( db->handle, sql, strlen(sql), &(db->stmt), NULL );

    va_start(ap, sql);
    while((arg = va_arg(ap, char *)) != NULL)
    {
        sqlite3_bind_text( db->stmt, i++, arg, strlen(arg), SQLITE_TRANSIENT );
    }
    va_end(ap);

    ret = sqlite3_step ( db->stmt ); 
    //printf("sql:%s return:%d\n",sql, ret);

    sqlite3_finalize( db->stmt );
    db->stmt = NULL;

    return ret;
}


int db_query(DB *db, char *sql, ...)
{
    va_list ap;
    char *arg;
    int i = 1;


    sqlite3_prepare_v2 ( db->handle, sql, strlen(sql), &(db->stmt), NULL );

    va_start(ap, sql);
    while((arg = va_arg(ap, char *)) != NULL)
    {
        sqlite3_bind_text( db->stmt, i++, arg, strlen(arg), SQLITE_TRANSIENT );
    }
    va_end(ap);

    return 0;
}

int db_create_hash_table( DB *db )
{
    db->ht = malloc( sizeof( struct hsearch_data ) );
    memset(db->ht, '\0',  sizeof( struct hsearch_data ) );
    return hcreate_r( DB_HashTable_SIZE, db->ht );
}

void db_destory_hash_table( DB *db )
{
    if(db->ht)
    {
        hdestroy_r( db->ht );
        free( db->ht );
        db->ht = NULL;
    }
}

int db_fetch_row( DB *db, int result_type )
{
    int     cnt, i, ret;
    char    *result;
    ENTRY   item, *itemp;

    if( result_type < 1 || result_type > 3 )
        result_type = 1;

    //printf("%s\n", sqlite3_sql( db->stmt ) );

    if( (ret = sqlite3_step( db->stmt )) == SQLITE_ROW)
    {
        cnt = sqlite3_column_count( db->stmt );
        db->column_count = cnt;

        if( result_type & RESULT_NUM )
        {
            if( db->crow )
                free( db->crow );

            db->crow = (char **)calloc( cnt, sizeof(char *) );
        }

        if( result_type & RESULT_ASSOC )
        {
            if( db->ht )
                db_destory_hash_table( db );

            db_create_hash_table( db );
        }


        for(i = 0; i < cnt; i++)
        {
            result = (char *)sqlite3_column_text( db->stmt, i);

            if( result_type & RESULT_ASSOC )
            {
                item.key = (char *)sqlite3_column_name( db->stmt, i );
                item.data = result;
                hsearch_r( item, ENTER, &itemp, db->ht );
            }

            if( result_type & RESULT_NUM )
            {
                db->crow[i] = result;
            }
        }
        
        return cnt;
    } 
    
    //printf("sqlite3_step return: %d\n", ret);

    sqlite3_finalize( db->stmt );
    db->stmt = NULL;
    return 0;
}

char *db_get_value_by_key( DB *db, char *key )
{
    ENTRY item, *itemp;

    if( !db->ht )
        return NULL;

    item.key = key;
    hsearch_r( item, FIND, &itemp, db->ht );

    return itemp ? itemp->data : NULL;
}

char *db_get_value_by_index( DB *db, int index )
{
    return db->crow && index < db->column_count && index >= 0 ? db->crow[index] : NULL;
}

sqlite3_int64 db_last_insert_rowid( DB *db )
{
    return sqlite3_last_insert_rowid( db->handle );
}

int db_create_collation( DB *db, const char *name, int(*cmp_func)(void*,int,const void*,int,const void*), void *arg )
{
    return sqlite3_create_collation( db->handle, name, SQLITE_UTF8, arg, cmp_func );
}

int db_create_function( DB *db, const char *name, int argc, void (*xFunc)(sqlite3_context*,int,sqlite3_value**), void (*xStep)(sqlite3_context*,int,sqlite3_value**), void (*xFinal)(sqlite3_context*) )
{
    return sqlite3_create_function( db->handle, name, argc, SQLITE_UTF8, NULL, xFunc, xStep, xFinal );
}
