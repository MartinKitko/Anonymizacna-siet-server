#ifndef ANONSIETSERVER_DOWNLOADER_H
#define ANONSIETSERVER_DOWNLOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct Downloader {
    char *url;
    char *content;
};

struct Memory {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
void downloader_init(struct Downloader *downloader, char *url);
int downloader_download(struct Downloader *downloader);
void downloader_free(struct Downloader *downloader);

#endif //ANONSIETSERVER_DOWNLOADER_H
