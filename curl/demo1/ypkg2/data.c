/* Libypk data structure functions
 *
 * Copyright (c) 2013 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Date: 2013.3.13
 */
#include "data.h"

/*
 * hash table
 */
HashTable *hash_table_init()
{
    int                 buf_size;
    HashTable          *ht;


    ht = (HashTable *)malloc( sizeof( HashTable ) );
    if( !ht )
        return NULL;

    ht->index = (HashIndex *)malloc( sizeof( HashIndex ) );
    if( !ht->index )
    {
        free( ht );
        return NULL;
    }
    memset( ht->index, '\0',  sizeof( HashIndex )  );
    hcreate_r( 24, ht->index );

    buf_size = HashTable_SIZE;
    ht->data = hash_table_malloc_data( NULL, buf_size );
    if( !ht->data )
    {
        free( ht->index );
        free( ht );
        return NULL;
    }
    ht->cur_data = ht->data;

    return ht;
}

int hash_table_add_data( HashTable *ht, char *key, char *value )
{
    int             len, buf_size;
    ENTRY           item, *itemp;
    HashData       *new_data;

    if( !ht || !key )
        return -1;

    if( value  )
    {
        len = strlen( value );
        if( ht->cur_data->pos + len + 1 > ht->cur_data->size )
        {
            if( len > HashTable_SIZE )
                buf_size = HashTable_SIZE + len;
            else
                buf_size = HashTable_SIZE;

            new_data = hash_table_malloc_data( ht->cur_data, buf_size );
            if( !new_data )
                return -1;
            ht->cur_data = new_data;

        }

        memcpy( ht->cur_data->buf + ht->cur_data->pos, value, len );
        ht->cur_data->buf[ht->cur_data->pos + len] = '\0';
        item.key = key;
        item.data = (void *)(ht->cur_data->buf + ht->cur_data->pos);
        hsearch_r( item, ENTER, &itemp, ht->index );
        ht->cur_data->pos += len + 1;
    }
    else
    {
        item.key = key;
        item.data = NULL;
        hsearch_r( item, ENTER, &itemp, ht->index );
    }
    return 0;
}


char *hash_table_get_data( HashTable *ht, char *key )
{
    ENTRY   item, *itemp;

    if( !ht || !key )
        return NULL;

    item.key = key;
    item.data = NULL;
    hsearch_r( item, FIND, &itemp, ht->index );

    return itemp ? itemp->data : NULL;
}

void hash_table_cleanup( HashTable *ht )
{
    HashData   *cur_data, *next_data;

    if( !ht )
        return;

    if( ht->index )
    {
        hdestroy_r( ht->index );
        free( ht->index );
    }

    if( ht->data );
    {
        cur_data = ht->data;

        while( cur_data )
        {
            if( cur_data->buf )
                free( cur_data->buf );

            next_data = cur_data->next; 
            free( cur_data );
            cur_data = next_data;
        }
    }

    free( ht );
}


HashTableList *hash_table_list_init( int count )
{
    int                 buf_size;
    HashTableList     *htl;

    if( count < 1 )
        return NULL;

    htl = (HashTableList *)malloc( sizeof( HashTableList ) );
    if( !htl )
        return NULL;

    htl->max_cnt = count;
    htl->cur_cnt = 0;

    htl->list = (HashIndex **)malloc( sizeof( HashIndex * ) * count );
    if( !htl->list )
    {
        free( htl );
        return NULL;
    }
    memset( htl->list, '\0',  sizeof( HashIndex * ) * count );

    buf_size = HashTable_SIZE * count;
    htl->data = hash_table_malloc_data( NULL, buf_size );
    if( !htl->data )
    {
        free( htl->list );
        free( htl );
        return NULL;
    }
    htl->cur_data = htl->data;

    return htl;
}

int hash_table_list_extend( HashTableList *htl, int count )
{
    HashIndex      **new_list;

    if( !htl )
        return -1;

    if( htl->max_cnt > count )
        return -1;

    new_list = (HashIndex **)realloc( htl->list, sizeof( HashIndex * ) * count );
    if( !new_list )
    {
        return -1;
    }
    memset( &(new_list[htl->max_cnt]), '\0',  sizeof( HashIndex * ) * ( count - htl->max_cnt ) );
    htl->max_cnt = count;
    htl->list = new_list;

    return 0;
}

int hash_table_list_add_data( HashTableList *htl, int index, char *key, char *value )
{
    int             len, buf_size, ret;
    ENTRY           item, *itemp;
    HashIndex      *cur_index;
    HashData       *new_data;

    if( !htl || !key || index < 0 )
        return -1;

    if( index > htl->max_cnt - 1 )
        return -1;

    if( index + 1 > htl->cur_cnt )
        htl->cur_cnt = index + 1;

    if( htl->list[index] == NULL )
    {
        cur_index = (HashIndex *)malloc( sizeof( HashIndex ) );
        if( !cur_index )
            return -1;

        memset( cur_index, '\0',  sizeof( HashIndex )  );
        hcreate_r( 24, cur_index );
        htl->list[index] = cur_index;
    }

    if( value  )
    {
        len = strlen( value );
        if( htl->cur_data->pos + len + 1 > htl->cur_data->size )
        {
            if( htl->max_cnt < 4 )
                buf_size = HashTable_SIZE * 4;
            else if( htl->cur_cnt > htl->max_cnt * 3 / 4 )
                buf_size = HashTable_SIZE * htl->max_cnt / 3;
            else if( htl->cur_cnt > htl->max_cnt * 2 / 3 )
                buf_size = HashTable_SIZE * htl->max_cnt / 2;
            else if( htl->cur_cnt > htl->max_cnt / 2 )
                buf_size = HashTable_SIZE * htl->max_cnt;
            else
                buf_size = HashTable_SIZE * htl->max_cnt * 2;

            if( len > buf_size )
                buf_size += len;

            new_data = hash_table_malloc_data( htl->cur_data, buf_size );
            if( !new_data )
                return -1;
            htl->cur_data = new_data;

        }

        memcpy( htl->cur_data->buf + htl->cur_data->pos, value, len );
        htl->cur_data->buf[htl->cur_data->pos + len] = '\0';
        item.key = key;
        item.data = (void *)(htl->cur_data->buf + htl->cur_data->pos);
        ret = hsearch_r( item, ENTER, &itemp, htl->list[index] );
        htl->cur_data->pos += len + 1;
    }
    else
    {
        item.key = key;
        item.data = NULL;
        ret = hsearch_r( item, ENTER, &itemp, htl->list[index] );
    }
    return ret;
}

char *hash_table_list_get_data( HashTableList *htl, int index, char *key )
{
    ENTRY   item, *itemp;

    if( !htl || !key || index < 0 )
        return NULL;

    if( index > htl->cur_cnt - 1 )
        return NULL;

    item.key = key;
    item.data = NULL;
    hsearch_r( item, FIND, &itemp, htl->list[index] );

    return itemp ? itemp->data : NULL;
}

void hash_table_list_cleanup( HashTableList *htl )
{
    int         i;
    HashData   *cur_data, *next_data;

    if( !htl )
        return;

    if( htl->list )
    {
        for( i = 0; i < htl->cur_cnt; i++ )
        {
            if( htl->list[i] )
            {
                hdestroy_r( htl->list[i] );
                free( htl->list[i] );
            }
        }
        free( htl->list );
    }

    if( htl->data );
    {
        cur_data = htl->data;

        while( cur_data )
        {
            if( cur_data->buf )
                free( cur_data->buf );

            next_data = cur_data->next; 
            free( cur_data );
            cur_data = next_data;
        }
    }

    free( htl );
}


HashData *hash_table_malloc_data( HashData *cur_data, int new_size )
{
    HashData *new_data;

    if( new_size < 1 )
        return NULL;

    new_data = (HashData *)malloc( sizeof( HashData ) );
    if( !new_data )
    {
        return NULL;
    }

    new_data->next = NULL;
    new_data->size = new_size;
    new_data->pos = 0;
    new_data->buf = (char *)malloc( new_size );
    if( !new_data->buf )
    {
        free( new_data );
        return NULL;
    }
    memset( new_data->buf, '\0',  new_size  );
    if( cur_data )
        cur_data->next = new_data;

    return new_data;
}

//Double linked list
DList *dlist_init()
{
    DList          *list;

    list = (DList *)malloc( sizeof( DList ) );
    if( !list )
        return NULL;

    list->cnt = 0;
    list->head = NULL;
    list->tail = NULL;
    list->cur = NULL;

    return list;
}

int dlist_append( DList *list, void *data )
{
    DListNode   *new;

    if( !list || !data )
        return -1;

    new = (DListNode *)malloc( sizeof( DListNode ) );
    if( !new )
        return -2;

    new->data = data;
    new->prev = list->tail;
    new->next = NULL;
    if( list->tail )
        list->tail->next = new;
    list->tail = new;
    if( !list->head )
        list->head = new;

    list->cnt++;

    return 0;
}

int dlist_insert( DList *list, int index, void *data )
{
    int         i;
    DListNode   *new, *cur;

    if( !list || index < 1 || index > list->cnt + 1 )
        return -1;

    new = (DListNode *)malloc( sizeof( DListNode ) );
    if( !new )
        return -2;


    i = 1;
    cur = list->head;
    while( i < index && cur )
    {
        cur = cur->next;
        i++;
    }

    new->data = data;

    if( cur )
    {
        new->prev = cur->prev;
        new->next = cur;

        if( cur->prev )
            cur->prev->next = new;

        cur->prev = new;
    }
    else
    {
        new->next = NULL;
        new->prev = list->tail ? list->tail : NULL;
    }

    if( index == 1 )
        list->head = new;

    if( index == list->cnt + 1 )
    {
        if( list->tail )
            list->tail->next = new;

        list->tail = new;
    }

    list->cnt++;

    return 0;
}

DListNode *dlist_get( DList *list, int index )
{
    int         i;

    if( !list || index < 1 || index > list->cnt )
        return NULL;

    i = 1;
    list->cur = list->head;
    while( i < index && list->cur )
    {
        list->cur = list->cur->next;
        i++;
    }

    return list->cur;
}

DListNode *dlist_next( DList *list )
{
    if( !list || !list->cur )
        return NULL;

    list->cur = list->cur->next;
    return list->cur;
}

DListNode *dlist_prev( DList *list )
{
    if( !list || !list->cur )
        return NULL;

    list->cur = list->cur->prev;
    return list->cur;
}

DListNode *dlist_head( DList *list )
{
    if( !list )
        return NULL;

    list->cur = list->head;
    return list->cur;
}

DListNode *dlist_tail( DList *list )
{
    if( !list )
        return NULL;

    list->cur = list->tail;
    return list->cur;
}

void *dlist_get_data( DList *list, int index )
{
    DListNode   *tmp;

    tmp = dlist_get( list, index );
    return tmp ? tmp->data : NULL;
}

void *dlist_head_data( DList *list )
{
    DListNode   *tmp;

    tmp = dlist_head( list );
    return tmp ? tmp->data : NULL;
}

void *dlist_tail_data( DList *list )
{
    DListNode   *tmp;

    tmp = dlist_tail( list );
    return tmp ? tmp->data : NULL;
}

void *dlist_next_data( DList *list )
{
    DListNode   *tmp;

    tmp = dlist_next( list );
    return tmp ? tmp->data : NULL;
}

void *dlist_prev_data( DList *list )
{
    DListNode   *tmp;

    tmp = dlist_prev( list );
    return tmp ? tmp->data : NULL;
}

int dlist_remove( DList *list, int index, void (*node_cleanup)(void *) )
{
    int         i;
    DListNode   *cur;

    if( !list || index < 1 || index > list->cnt )
        return -1;

    i = 1;
    cur = list->head;
    while( i < index && cur )
    {
        cur = cur->next;
        i++;
    }

    if( cur->prev )
        cur->prev->next = cur->next;

    if( cur->next )
        cur->next->prev = cur->prev;

    if( index == 1 )
        list->head = cur->next;

    if( index == list->cnt )
        list->tail = cur->prev;

    if( node_cleanup )
        node_cleanup( cur->data );

    free( cur );

    list->cnt--;

    return 0;
}

int dlist_remove2( DList *list, DListNode *node, void (*node_cleanup)(void *) )
{
    DListNode   *cur;

    if( !list || !node )
        return -1;

    cur = list->head;
    while( cur )
    {
        if( cur == node )
            break;

        cur = cur->next;
    }

    if( cur )
    {
        if( cur->prev )
            cur->prev->next = cur->next;

        if( cur->next )
            cur->next->prev = cur->prev;

        if( cur == list->head )
            list->head = cur->next;

        if( cur == list->tail )
            list->tail = cur->prev;

        if( node_cleanup )
            node_cleanup( cur->data );

        free( cur );
        list->cnt--;
    }

    return 0;
}

int dlist_search( DList *list, void *data,  int (*node_cmp_func)(void *, void *) )
{
    int         i;
    DListNode   *cur;

    if( !list || !node_cmp_func )
        return -1;
    
    i = 1;
    cur = list->head;
    while( cur )
    {
        if( !node_cmp_func( cur->data, data ) )
            return i;

        cur = cur->next;
        i++;
    }

    return 0;
}

DListNode *dlist_search2( DList *list, void *data,  int (*node_cmp_func)(void *, void *) )
{
    DListNode   *cur;

    if( !list || !node_cmp_func )
        return NULL;
    
    cur = list->head;
    while( cur )
    {
        if( !node_cmp_func( cur->data, data ) )
            return cur;

        cur = cur->next;
    }

    return NULL;
}

void dlist_cleanup( DList *list, void (*node_cleanup)(void *) )
{
    DListNode *cur, *tmp;

    if( list )
    {
        cur = list->head;
        while( cur )
        {
            tmp = cur;
            cur = cur->next;

            if( tmp->next )
                tmp->next->prev = tmp->prev;

            if( tmp->prev )
                tmp->prev->next = tmp->next;

            if( node_cleanup )
                node_cleanup( tmp->data );

            free( tmp );
        }

        free( list );
        list = NULL;
    }
}

DList *dlist_cat( DList *lista, DList *listb )
{
    if( !lista )
        return NULL;

    if( !listb )
        return lista;

    if( listb->cnt == 0 )
        return lista;

    if( lista->cnt == 0 )
    {
        lista->head = listb->head;
        lista->tail = listb->tail;
        lista->cnt = listb->cnt;
    }
    else
    {
        lista->tail->next = listb->head;
        listb->head->prev = lista->tail;

        lista->tail = listb->tail;
        lista->cnt += listb->cnt;
    }

    listb->head = listb->tail = NULL;
    listb->cnt = 0;

    return lista;
}

DList *dlist_strip_duplicate( DList *list, int (*node_cmp_func)(void *, void *), void (*node_cleanup)(void *) )
{
    DListNode *src, *dest, *tmp;

    if( !list || !node_cmp_func )
        return NULL;

    src = list->head;
    while( src )
    {
        dest = src->next;
        while( dest )
        {
            tmp = dest;
            dest = dest->next;
            if( !node_cmp_func( src->data, tmp->data ) )
            {
                dlist_remove2( list, tmp, node_cleanup );
            }
        }
        src = src->next;
    }

    return list;
}
