#include "downloader.h"
#include "definitions.h"

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realSize = size * nmemb;
    struct Memory *mem = (struct Memory *) userp;

    char *ptr = realloc(mem->memory, mem->size + realSize + 1);
    if (ptr == NULL) {
        printError("Chyba - realloc");
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realSize);
    mem->size += realSize;
    mem->memory[mem->size] = 0;

    return realSize;
}

void downloader_init(struct Downloader *downloader, char *url) {
    downloader->url = url;
    downloader->content = NULL;
}

int downloader_download(struct Downloader *downloader) {
    CURL *curl;
    CURLcode result = CURLE_CONV_FAILED;
    struct Memory chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, downloader->url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        result = curl_easy_perform(curl);
        if (result != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(result));
            free(chunk.memory);
        } else {
            downloader->content = chunk.memory;
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    return (int) result;
}

void downloader_free(struct Downloader *downloader) {
    if (downloader->content != NULL) {
        free(downloader->content);
        downloader->content = NULL;
    }
}