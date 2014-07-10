/* Libypk regular expression functions
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2012.3.6
 */

#include "preg.h"

int preg_match(PREGInfo *piptr, char *pattern, char *subject, int options, int first)
{
    const char *error;
    int erroffset;
    int offset;

    if( !piptr || !pattern || !subject )
    {
        return -1;
    }

    if(first)
    {
        piptr->re = pcre_compile(
          pattern,              /* the pattern */
          options,              /* default options */
          &error,               /* for error message */
          &erroffset,           /* for error offset */
          NULL                  /* use default character tables */
          );                
    }

    if (piptr->re == NULL)
    {
        printf("PCRE compilation failed at offset %d: %s\n", erroffset, error);
        return -1;
    }

    piptr->subject = subject;

    offset = first ? 0 : piptr->offset;
    piptr->stringcount = pcre_exec(
      piptr->re,                   /* the compiled pattern */
      NULL,                 /* no extra data - we didn't study the pattern */
      subject,              /* the subject string */
      (int)strlen(subject), /* the length of the subject */
      offset,                    /* start at offset 0 in the subject */
      0,                    /* default options */
      piptr->ovector,              /* output vector for substring information */
      OVECCOUNT             /* number of elements in the output vector */
      );           

    if (piptr->stringcount == 0)
    {
        printf("ovector only has room for %d captured substrings\n", OVECCOUNT/3 - 1);
    }
    else if(piptr->stringcount > 0)
    {
        piptr->offset = piptr->ovector[1];
    }
    return piptr->stringcount;
}


int preg_result(PREGInfo *piptr, int number, char *buf, int buf_size)
{
    int ret;

    ret = pcre_copy_substring(piptr->subject, piptr->ovector, piptr->stringcount, number, buf, buf_size);

    return ret;
}

int preg_result2(PREGInfo *piptr, char *name, char *buf, int buf_size)
{
    int ret;

    if(name == NULL)
    {
       ret = pcre_copy_substring(piptr->subject, piptr->ovector, piptr->stringcount, 0, buf, buf_size);
    }
    else
    {
       ret = pcre_copy_named_substring(piptr->re, piptr->subject, piptr->ovector, piptr->stringcount, name, buf, buf_size);
    }

    return ret;
}

void preg_free(PREGInfo *piptr)
{
    if( piptr && piptr->re )
        pcre_free(piptr->re);
}

char *preg_replace( char *pattern, char *replace, char *subject, int options, int once )
{

    pcre *re;
    int ovector[OVECCOUNT];
    const char *error;
    int erroffset;

    int subject_len, replace_len, matched_len, alloc_len;
    char *result;
    char *tmp;

    subject_len = strlen(subject); 
    replace_len = strlen(replace); 
    matched_len = 0;



    alloc_len =  2 * subject_len + 1;
    result = (char *)malloc( alloc_len );
    tmp = (char *)malloc( alloc_len );
    memset( result, 0, alloc_len );
    memset( tmp, 0, alloc_len );

    re = pcre_compile( pattern, options, &error, &erroffset, NULL );                

    memcpy(tmp, subject, subject_len + 1);

    while( pcre_exec( re, NULL, tmp, subject_len, 0, 0, ovector, OVECCOUNT ) > 0 )
    {
        matched_len = ovector[1] - ovector[0];
        if( replace_len - matched_len + subject_len > alloc_len )
        {
            alloc_len += replace_len * 2;
            result = (char *)realloc( result, alloc_len );
            if( !once )
                tmp = (char *)realloc( tmp, alloc_len );
            //printf("realloc to %d byte\n", alloc_len);
        }
        memset( result, 0, alloc_len );
        memcpy( result, tmp, ovector[0] );

        memcpy(result + ovector[0], replace, replace_len);

        memcpy(result + ovector[0] + replace_len, tmp + ovector[1], subject_len - ovector[1]);

        subject_len += replace_len - matched_len; 

        if( once )
            break;

        memset( tmp, 0, alloc_len );
        memcpy( tmp, result, subject_len );

    }
    result[subject_len] = 0;

    free(tmp);
    return result;
}

/*
int preg_demo(void)
{

    char *pattern = "\\d+";
    char *subject = "abcdabcdabc1354def82gbckswwer";

    //match
    PREGInfo pi;
    int first = 1;
    int buf_size = 64;
    char buf[buf_size];

    while( preg_match(&pi, pattern, subject, 0, first) > 0 )
    {
        preg_result(&pi, 0, buf, buf_size);
        printf("result: %s\n", buf);
        first = 0;
    }
    preg_free(&pi);

    //replace
    char *replace = "*";
    char *result;
    result = preg_replace(pattern, replace, subject, PCRE_CASELESS);
    printf("result: %s\n", result);
    free(result);

    return 0;
}
*/
