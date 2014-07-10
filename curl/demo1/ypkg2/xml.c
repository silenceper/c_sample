/* XML parser
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2011.10.24
 */

#include "xml.h"

/**
 * xPath
 */
xmlDocPtr xpath_open( char *docname ) 
{
	xmlDocPtr doc;
	doc = xmlParseFile( docname );
	
#ifdef DEBUG
	if( doc == NULL ) 
    {
		fprintf( stderr,"Document not parsed successfully. \n" );
	}
#endif

	return doc;
}

xmlDocPtr xpath_open2( char *buffer, int size ) 
{
	xmlDocPtr doc;

	doc = xmlParseMemory( buffer, size );
	
#ifdef DEBUG
	if( doc == NULL ) 
    {
		fprintf( stderr,"Document not parsed successfully. \n" );
	}
#endif

	return doc;
}

xmlXPathObjectPtr xpath_get_nodeset( xmlDocPtr doc, xmlChar *xpath )
{
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;

	context = xmlXPathNewContext(doc);
	if (context == NULL) 
    {
#ifdef DEBUG 
		fprintf( stderr, "Error in xmlXPathNewContext\n" );
#endif
		return NULL;
	}
	result = xmlXPathEvalExpression( xpath, context );
	xmlXPathFreeContext( context );
	if( result == NULL ) 
    {
#ifdef DEBUG
		fprintf( stderr, "Error in xmlXPathEvalExpression\n" );
#endif
		return NULL;
	}

	if( xmlXPathNodeSetIsEmpty( result->nodesetval ) )
    {
		xmlXPathFreeObject( result );
#ifdef DEBUG
        fprintf( stderr, "No result: xmlXPathNodeSetIsEmpty\n" );
#endif
		return NULL;
	}
	return result;
}

char *xpath_get_node( xmlDocPtr doc, xmlChar *xpath )
{
	xmlNodeSetPtr nodeset;
	xmlXPathObjectPtr xpathobj;
	xmlChar *keyword;
    char *result = NULL;
    int len;

	if( (xpathobj = xpath_get_nodeset( doc, xpath ) ) == NULL ) 
    {
#ifdef DEBUG
        fprintf( stderr, "get_xml_nodeset() failed, xpath: %s!\n", xpath );
#endif
        return NULL;
    }

    nodeset = xpathobj->nodesetval;
    if( nodeset->nodeNr <= 0)
    {
#ifdef DEBUG
        fprintf( stderr, "nodeset->nodeNr <= 0\n" );
#endif
        xmlXPathFreeObject(xpathobj);
        return NULL;
    }

    keyword = xmlNodeListGetString( doc, nodeset->nodeTab[0]->xmlChildrenNode, 1 );
    if( keyword && ( len = strlen( (char *)keyword ) )>0 )
    {
        result = (char *)malloc( len + 1 );
        strncpy( result, (char *)keyword, len + 1 );
        result[len] = '\0';
    }

    xmlFree( keyword );
    xmlXPathFreeObject( xpathobj );
    return result;
}


/**
 * xmlTextReader
 */
int reader_open( char *docname,  XMLReaderHandle *handle )
{

    handle->reader = xmlNewTextReaderFilename( docname );
	if ( handle->reader == NULL ) 
    {
#ifdef DEBUG
		fprintf( stderr, "xmlNewTextReaderFilename failed. \n" );
#endif
        return -1;
	}

    handle->ht = xmlHashCreate(XML_HashTable_SIZE);
	if ( handle->ht == NULL ) 
    {
#ifdef DEBUG
		fprintf( stderr, "xmlHashCreate failed. \n" );
#endif
        return -1;
	}

    return 0;
}

int reader_fetch_a_row( XMLReaderHandle *handle, int node_depth, char **attr_list )
{
    int         ret, in_a_row = 0, end_a_row =0, current_depth, current_type;
    //char        **local_attr_list, *current_attr, *attr_value,prefix[XML_HASH_FULL_KEY_LEN];
    char        **local_attr_list, *current_attr;
    xmlChar     *attr_value;

    reader_hash_cleanup( handle );
    while( ( ret = xmlTextReaderRead( handle->reader ) ) == 1 )
    {
        current_depth = xmlTextReaderDepth( handle->reader );
        current_type = xmlTextReaderNodeType( handle->reader );

        if( current_depth < node_depth )
        {
            continue;
        }

        switch( current_type )
        {
            case XML_READER_TYPE_ELEMENT:
                if( current_depth == node_depth )
                {
                    if( in_a_row == 1 )
                    {
#ifdef DEBUG
        fprintf( stderr, "xml parse error: unclosed tag\n");
#endif
                        return -1;
                    }

                    if( attr_list )
                    {
                        //memset( prefix, '\0', XML_HASH_FULL_KEY_LEN );
                        local_attr_list = attr_list;
                        while( (current_attr = *local_attr_list++) )
                        {
                            if( (attr_value = xmlTextReaderGetAttribute( handle->reader, (const xmlChar *)current_attr )) )
                            {
                                /*
                                if( prefix[0] )
                                    strcat( prefix, "|" );
                                strncat( prefix, attr_value, XML_HASH_FULL_KEY_LEN - strlen( prefix ) );
                                */
                                xmlHashAddEntry( handle->ht, (const xmlChar *)current_attr, (void *)attr_value );
                            }
                        }
                    }

                    in_a_row = 1;
                }
                else if( current_depth > node_depth )
                {
                    ret = reader_fetch_fields( handle, current_depth, /*prefix[0] ? prefix : */NULL, attr_list );
                    if( ret != 1 )
                        return ret;
                }
                break;

            case XML_READER_TYPE_END_ELEMENT:
                if( current_depth == node_depth )
                {
                    end_a_row = 1;
                    in_a_row = 0;
                }
                break;
            default:
                break;
        }

        if( end_a_row )
            break;
    }

    return ret;
}


int reader_fetch_fields( XMLReaderHandle *handle, int node_depth, char *prefix, char **attr_list )
{
    int         ret, current_depth, current_type;
    char        *key, *value, **local_attr_list, *current_attr, *attr_value, full_key[XML_HASH_FULL_KEY_LEN];


    key = (char *)xmlTextReaderName( handle->reader );
    memset( full_key, '\0', XML_HASH_FULL_KEY_LEN );

    if( prefix )
    {
        snprintf( full_key, XML_HASH_FULL_KEY_LEN, "%s|%s", prefix, key );
    }
    else
    {
        strncpy( full_key, key, XML_HASH_FULL_KEY_LEN );
    }

    if( xmlTextReaderIsEmptyElement( handle->reader ) )
    {
        xmlHashAddEntry( handle->ht, (const xmlChar *)full_key, NULL );
        return 1;
    }

    if( attr_list )
    {
        local_attr_list = attr_list;
        while( (current_attr = *local_attr_list++) )
        {
            if( (attr_value = (char *)xmlTextReaderGetAttribute( handle->reader, (const xmlChar *)current_attr )) )
            {
                strcat( full_key, "|" );
                strncat( full_key, (char *)attr_value, XML_HASH_FULL_KEY_LEN - strlen( full_key ) );
                xmlFree( attr_value );
            }
        }
    }

    while( ( ret = xmlTextReaderRead( handle->reader ) ) == 1 )
    {
        current_depth = xmlTextReaderDepth( handle->reader );
        current_type = xmlTextReaderNodeType( handle->reader );
        if( current_type == XML_READER_TYPE_ELEMENT && current_depth == node_depth + 1 )
        {
            ret = reader_fetch_fields( handle, current_depth, full_key, attr_list );
            if( ret != 1 )
                return ret;
        }
        else if( current_type == XML_READER_TYPE_TEXT && current_depth == node_depth + 1 )
        {
            value = (char *)xmlTextReaderValue( handle->reader );
            xmlHashAddEntry( handle->ht, (const xmlChar *)full_key, (void *)value );
            value = NULL;
        }
        else if( current_type == XML_READER_TYPE_END_ELEMENT && current_depth == node_depth )
        {
            xmlFree( key );
            return 1;
        }
    }

    return ret;
}

void hash_data_cleanup( void *data, xmlChar *key )
{
    xmlFree( (xmlChar *)data );
}

void reader_hash_cleanup( XMLReaderHandle *handle )
{
    xmlHashFree( handle->ht, hash_data_cleanup );
    handle->ht = xmlHashCreate(XML_HashTable_SIZE);
}

void reader_cleanup( XMLReaderHandle *handle )
{
    xmlFreeTextReader( handle->reader );
    xmlHashFree( handle->ht, hash_data_cleanup );
}

char *reader_get_value( XMLReaderHandle *handle, char *key )
{
    return (char *)xmlHashLookup( handle->ht, (const xmlChar *)key );
}


char *reader_get_value2( XMLReaderHandle *handle, char *key )
{
    char *result;

    result = (char *)xmlHashLookup( handle->ht, (const xmlChar *)key );
    return result ? result : "";
}
