/* XML parser
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2011.10.24
 */

#ifndef XML_H
#define XML_H

#include <string.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xmlreader.h>
#include <libxml/hash.h>

/* #define DEBUG 1 */

/**
 * xPath
 */
xmlDocPtr xpath_open( char *docname );
xmlDocPtr xpath_open2( char *buffer, int size );
xmlXPathObjectPtr xpath_get_nodeset( xmlDocPtr doc, xmlChar *xpath );
char *xpath_get_node( xmlDocPtr doc, xmlChar *xpath );



/**
 * xmlTextReader
 */
#define XML_HashTable_SIZE 32
#define XML_HASH_FULL_KEY_LEN 64
typedef struct {
    xmlTextReaderPtr    reader;
    xmlHashTablePtr     ht;
}XMLReaderHandle;

int reader_open( char *docname,  XMLReaderHandle *handle);

int reader_fetch_a_row( XMLReaderHandle *handle, int target_depth, char **attr_list );

int reader_fetch_fields( XMLReaderHandle *handle, int node_depth, char *prefix, char **attr_list );

void hash_data_cleanup( void *data, xmlChar *key );

void reader_hash_cleanup( XMLReaderHandle *handle );

void reader_cleanup( XMLReaderHandle *handle );

char *reader_get_value( XMLReaderHandle *handle, char *key );

char *reader_get_value2( XMLReaderHandle *handle, char *key );

#endif /* !XML_H */
