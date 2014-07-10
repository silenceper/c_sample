/* Libypk data structure functions
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2012.8.13
 */
#ifndef DATA_H
#define DATA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <search.h>

/*
 * hash table
 */
#define HashTable_SIZE 1024

typedef struct hsearch_data HashIndex;
typedef struct _HashData {
    int                 size;
    int                 pos;
    char                *buf;
    struct _HashData   *next;
}HashData;

typedef struct {
    HashIndex          *index;
    HashData           *data;
    HashData           *cur_data;
}HashTable;


typedef struct {
    int                 max_cnt;
    int                 cur_cnt;
    HashIndex          **list;
    HashData           *data;
    HashData           *cur_data;
}HashTableList;

HashTable *hash_table_init();
int hash_table_add_data( HashTable *ht, char *key, char *value );
char *hash_table_get_data( HashTable *ht, char *key );
void hash_table_cleanup( HashTable *ht );


HashTableList *hash_table_list_init();
int hash_table_list_extend( HashTableList *htl, int count );
int hash_table_list_add_data( HashTableList *htl, int index, char *key, char *value );
char *hash_table_list_get_data( HashTableList *htl, int index, char *key );
void hash_table_list_cleanup( HashTableList *htl );


HashData *hash_table_malloc_data( HashData *cur_data, int new_size );

/*
 * Double linked list
 */

typedef struct _DListNode {
    struct _DListNode        *prev;
    struct _DListNode        *next;
    void                     *data;
}DListNode;

typedef struct _DList {
    int            cnt;
    DListNode      *head;
    DListNode      *tail;
    DListNode      *cur;
}DList;


DList *dlist_init();
void dlist_cleanup( DList *list, void (*node_cleanup)(void *) );

int dlist_append( DList *list, void *data );
int dlist_insert( DList *list, int index, void *data );

int dlist_remove( DList *list, int index, void (*node_cleanup)(void *) );
int dlist_remove2( DList *list, DListNode *node, void (*node_cleanup)(void *) );

int dlist_search( DList *list, void *data,  int (*node_cmp_func)(void *, void *) );
DListNode *dlist_search2( DList *list, void *data,  int (*node_cmp_func)(void *, void *) );

DListNode *dlist_get( DList *list, int index );
DListNode *dlist_next( DList *list );
DListNode *dlist_prev( DList *list );
DListNode *dlist_head( DList *list );
DListNode *dlist_tail( DList *list );

void *dlist_get_data( DList *list, int index );
void *dlist_next_data( DList *list );
void *dlist_prev_data( DList *list );
void *dlist_head_data( DList *list );
void *dlist_tail_data( DList *list );

DList *dlist_cat( DList *lista, DList *listb );
DList *dlist_strip_duplicate( DList *list, int (*node_cmp_func)(void *, void *), void (*node_cleanup)(void *) );

#endif /* !DATA_H */
