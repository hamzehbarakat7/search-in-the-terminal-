#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

#define BUFFER_SIZE 100000

struct Memory {
    char *response;
    size_t size;
};

size_t write_callback(void *data, size_t size, size_t nmemb, void *userdata) {
    size_t realsize = size * nmemb;
    struct Memory *mem = (struct Memory *)userdata;
    
    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if (ptr == NULL) return 0;
    
    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;
    
    return realsize;
}

char *fetch_wikipedia_data(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    
    struct Memory chunk = {malloc(1), 0};
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Follow redirects
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        printf("CURL error: %s\n", curl_easy_strerror(res));
        free(chunk.response);
        return NULL;
    }
    
    return chunk.response;
}

void find_similar_pages(const char *search_term) {
    char *encoded_term = curl_easy_escape(NULL, search_term, 0);
    char url[512];
    snprintf(url, sizeof(url), "https://en.wikipedia.org/w/api.php?action=query&list=search&srsearch=%s&format=json", encoded_term);
    curl_free(encoded_term);
    
    char *response = fetch_wikipedia_data(url);
    if (!response) {
        printf("Error fetching data from Wikipedia.\n");
        return;
    }
    
    struct json_object *parsed_json = json_tokener_parse(response);
    struct json_object *query, *search;
    if (parsed_json && 
        json_object_object_get_ex(parsed_json, "query", &query) &&
        json_object_object_get_ex(query, "search", &search)) {
        int len = json_object_array_length(search);
        printf("\nMost similar pages:\n");
        for (int i = 0; i < len && i < 10; i++) {
            struct json_object *entry = json_object_array_get_idx(search, i);
            struct json_object *title;
            if (json_object_object_get_ex(entry, "title", &title)) {
                printf("%d. %s\n", i + 1, json_object_get_string(title));
            }
        }
    } else {
        printf("Failed to parse search results.\n");
    }
    
    if (parsed_json) json_object_put(parsed_json);
    free(response);
}

void display_page_summary(const char *page_title) {
    // Encode the page title to handle spaces and special characters
    char *encoded_title = curl_easy_escape(NULL, page_title, 0);
    
    char url[512];
    // Use the REST API with proper URL encoding
    snprintf(url, sizeof(url), "https://en.wikipedia.org/api/rest_v1/page/summary/%s", encoded_title);
    curl_free(encoded_title);
    
    char *response = fetch_wikipedia_data(url);
    if (!response) {
        printf("Error retrieving summary.\n");
        return;
    }
    
    struct json_object *parsed_json = json_tokener_parse(response);
    if (!parsed_json) {
        printf("Failed to parse JSON response.\n");
        free(response);
        return;
    }
    
    struct json_object *extract, *title;
    if (json_object_object_get_ex(parsed_json, "extract", &extract) &&
        json_object_object_get_ex(parsed_json, "title", &title)) {
        printf("\n--- Summary of %s ---\n%s\n", 
               json_object_get_string(title), 
               json_object_get_string(extract));
    } else {
        printf("Could not find summary in the response.\n");
        // Optional: print raw response for debugging
        printf("Raw response: %s\n", response);
    }
    
    json_object_put(parsed_json);
    free(response);
}

int main() {
    // Initialize CURL global environment
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    char search_term[256];
    printf("Enter a topic to search : ");
    fgets(search_term, sizeof(search_term), stdin);
    search_term[strcspn(search_term, "\n")] = 0;
    
    find_similar_pages(search_term);
    
    char page_title[256];
    printf("\nEnter the title of the topic you want to view: ");
    fgets(page_title, sizeof(page_title), stdin);
    page_title[strcspn(page_title, "\n")] = 0;
    
    display_page_summary(page_title);
    
    // Cleanup CURL global environment
    curl_global_cleanup();
    return 0;
}