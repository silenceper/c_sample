/* Libypk download functions
 *
 * Copyright (c) 2012 StartOS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2012.04.18
 */
#include "download.h"

size_t memory_callback(void *data, size_t size, size_t nmemb, void *user)
{
    size_t realsize = size * nmemb;
    DownloadContent *content = (DownloadContent *)user;

    content->text = realloc(content->text, content->size + realsize + 1);
    if (content->text == NULL) 
    {
#if defined DEBUG_MODE
        /* out of memory! */ 
        fprintf(stderr, "not enough memory (realloc returned NULL)\n");
#endif
        //exit(EXIT_FAILURE);
        return -1;
    }

    memcpy(&(content->text[content->size]), data, realsize);
    content->size += realsize;
    content->text[content->size] = 0;

    return realsize;
}

size_t file_callback(void *data, size_t size, size_t nmemb, void *user)
{
    DownloadFile *file = (DownloadFile *)user;
    if(file && !file->stream)
    {
        file->stream = fopen(file->file, "wb");
        if(!file->stream)
            return -1;
    }
    return fwrite(data, size, nmemb, file->stream);
}

int get_content(char *url, DownloadContent *content)
{
    CURL *curl_handle;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.2)");
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)content);

    res = curl_easy_perform(curl_handle);

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    return res;
}

int post_content(char *url,char *postfields, DownloadContent *content)
{
    CURL *curl_handle;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.2)");
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)content);

    res = curl_easy_perform(curl_handle);

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    return res;
}


int download_file(char *url, DownloadFile *file)
{
    CURL *curl_handle;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.2)");
    curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_LIMIT, 5L); //5 bytes
    curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_TIME, 10L); //10 seconds
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)file);
    if( file->cb )
    {
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L );
        curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, file->cb );
        if( file->cb_arg )
            curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, file->cb_arg );
    }
    else
    {
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    }

    res = curl_easy_perform(curl_handle);

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    return res;
}
