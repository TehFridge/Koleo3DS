#include "request.h"
#include "koleo_api.h"
#include "koleo_utils.h"
#include "config.h"
#include "cJSON.h" // Add this to your includes
#define MAX_QUEUE 10
bool doing_debug_logs;
static Request request_queue[MAX_QUEUE];
static int request_count = 0;
static LightLock request_lock;
static bool request_running = true;
static Thread request_thread;
static LightEvent request_event;

bool youfuckedup = false;
bool czasfuckup = false;
bool requestdone = false;
bool need_to_load_image = false;
bool loadingshit = false;
long response_code = 0;

LightLock global_response_lock;

// the stuff where the converted png/jpg goes
C2D_SpriteSheet kuponobraz; 
C2D_Image kuponkurwa;
bool obrazekdone = false;

static inline u32 next_pow2(u32 n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
static inline u32 clamp(u32 n, u32 lower, u32 upper) {
    return n < lower ? lower : (n > upper ? upper : n);
}
static inline u32 rgba_to_abgr(u32 px) {
    u8 r = px & 0xff;
    u8 g = (px >> 8) & 0xff;
    u8 b = (px >> 16) & 0xff;
    u8 a = (px >> 24) & 0xff;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

typedef struct {
    char *data;
    size_t size;
    char etag[128];
} DownloadState;

// --- CHECK ETAG ELIGIBILITY ---
static bool is_etag_eligible(const char* url) {
    if (!url) return false;
    
    // Only allow the specific preload endpoints
    if (strstr(url, "v2/main/stations") ||
        strstr(url, "v2/main/seat_types") ||
        strstr(url, "v2/main/discount_cards") ||
        strstr(url, "v2/main/carriage_types") ||
        strstr(url, "v2/main/attribute_definitions") ||
        strstr(url, "v2/main/carriers") ||
        strstr(url, "v2/main/brands")) {
        return true;
    }
    return false;
}

static cJSON* load_etags_json() {
    FILE *f = fopen("/3ds/Koleo3DS/etag.json", "rb");
    if (!f) return cJSON_CreateObject();
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    if (len == 0) {
        fclose(f);
        return cJSON_CreateObject();
    }
    rewind(f);
    char* buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        return cJSON_CreateObject();
    }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if (!root) return cJSON_CreateObject();
    return root;
}

static void save_etags_json(cJSON* root) {
    char* out = cJSON_PrintUnformatted(root);
    if (out) {
        FILE *f = fopen("/3ds/Koleo3DS/etag.json", "w");
        if (f) {
            fputs(out, f);
            fclose(f);
        }
        free(out);
    }
}

// --- NEW HEADER CALLBACK ---
size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t total_size = size * nitems;
    DownloadState *state = (DownloadState *)userdata;

    if (strncasecmp(buffer, "ETag: ", 6) == 0) {
        const char* val = buffer + 6;
        strncpy(state->etag, val, sizeof(state->etag) - 1);
        state->etag[sizeof(state->etag) - 1] = '\0';

        // Strip \r and \n but keep the quotes and W/ prefix
        char *cr = strchr(state->etag, '\r');
        if (cr) *cr = '\0';
        char *lf = strchr(state->etag, '\n');
        if (lf) *lf = '\0';
    }
    return total_size;
}

// --- UPDATED WRITE CALLBACK ---
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    if (!ptr || !userdata) return 0;

    size_t total_size = size * nmemb;
    DownloadState *state = (DownloadState *)userdata;

    size_t needed_size = state->size + total_size + 1;
    char *new_data = realloc(state->data, needed_size);
    if (!new_data) {
        log_to_file("[write_callback] ERROR: realloc failed");
        return 0;
    }

    state->data = new_data;
    memcpy(state->data + state->size, ptr, total_size);
    state->size += total_size;
    state->data[state->size] = '\0';

    return total_size;
}

void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void log_request_to_file(const char *url, const char *data, struct curl_slist *headers, char *response) {
    FILE *log_file = fopen("request_log.txt", "a");
    if (log_file) {
        fprintf(log_file, "URL: %s\n", url);
        fprintf(log_file, "Data: %s\n", data ? data : "(None)");
        fprintf(log_file, "Headers:\n");
        struct curl_slist *header = headers;
        while (header) {
            fprintf(log_file, "  %s\n", header->data);
            header = header->next;
        }
        fprintf(log_file, "Response: %s\n", response ? response : "(None)");
        fprintf(log_file, "------------------------\n");
        fclose(log_file);
    } else {
        printf("Failed to open log file for writing.\n");
    }
}

int my_curl_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
    switch (type) {
        case CURLINFO_TEXT:
        case CURLINFO_HEADER_IN:
        case CURLINFO_HEADER_OUT:
        case CURLINFO_DATA_IN:
        case CURLINFO_DATA_OUT:
            if (!doing_debug_logs) {
                log_to_file("[curl_debug] %.*s", (int)size, data);
            }
            break;
        default:
            break;
    }
    return 0;
}

// request_type 
// 0: get
// 1: post
// 2: patch

bool refresh_data(CURL *curl, const char *url, const char *data, struct curl_slist *headers, ResponseBuffer *response_buf, RequestType request_type) {
    bool request_failed = false;
    bool retried_after_401 = false;
    bool etag_added = false;
    struct curl_slist *allocated_headers = NULL; 

retry_request:
    LightLock_Lock(&global_response_lock);
    youfuckedup = false;
    czasfuckup = false;
    requestdone = false;
    loadingshit = true;
    LightLock_Unlock(&global_response_lock);

    if (!url || url[0] == '\0' || !response_buf) {
        if (!doing_debug_logs) {
            log_to_file("[refresh_data] ERROR: Missing URL or response buffer.");
        }
        return true;
    }

    response_buf->done = false;
    log_to_file("Request: %s", data);
    
    const char *endpoint = url;
    size_t base_url_len = strlen(KOLEO_BASE_URL);
    if (strncmp(url, KOLEO_BASE_URL, base_url_len) == 0) {
        endpoint = url + base_url_len;
    }

    bool use_etag = (request_type == GET && is_etag_eligible(url));
    cJSON *etags_obj = NULL;

    struct curl_slist *current_headers = allocated_headers ? allocated_headers : headers;

    if (use_etag && !etag_added) {
        etags_obj = load_etags_json();
        cJSON *etag_item = cJSON_GetObjectItem(etags_obj, endpoint);
        if (etag_item && cJSON_IsString(etag_item)) {
            char etag_header[256];
            snprintf(etag_header, sizeof(etag_header), "If-None-Match: %s", etag_item->valuestring);
            
            if (!allocated_headers) {
                for (struct curl_slist *h = headers; h != NULL; h = h->next) {
                    allocated_headers = curl_slist_append(allocated_headers, h->data);
                }
                current_headers = allocated_headers;
            }
            current_headers = curl_slist_append(current_headers, etag_header);
        }
        etag_added = true;
    }

    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, current_headers);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);

    if (use_etag) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180L); 
    } else {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);  
    }

    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 15L);

    switch(request_type){
        case GET:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
            break;
        case POST:
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
            break;
        case PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            break;
        case PUT:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            break;
    }

    DownloadState dl_state = {NULL, 0, {0}};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dl_state);
    
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &dl_state);

    CURLcode res = curl_easy_perform(curl);
    
    long local_response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &local_response_code);
    
    LightLock_Lock(&global_response_lock);
    response_code = local_response_code; 
    response_buf->response_code = local_response_code;

    if (res != CURLE_OK) {
        request_failed = true;
        response_buf->response_code = 0; 
        response_code = 0;
        if (dl_state.data) free(dl_state.data);
    } else {
        if (local_response_code == 304) {
            if (!doing_debug_logs) log_to_file("[refresh_data] 304 Not Modified -> Keeping cached buffer.");
            if (dl_state.data) free(dl_state.data);
        } else {
            if (response_buf->data) free(response_buf->data);
            response_buf->data = dl_state.data;
            response_buf->size = dl_state.size;

            if (use_etag && dl_state.etag[0] != '\0' && (local_response_code >= 200 && local_response_code < 300)) {
                if (!etags_obj) etags_obj = load_etags_json();
                if (cJSON_HasObjectItem(etags_obj, endpoint)) {
                    cJSON_ReplaceItemInObject(etags_obj, endpoint, cJSON_CreateString(dl_state.etag));
                } else {
                    cJSON_AddStringToObject(etags_obj, endpoint, dl_state.etag);
                }
                save_etags_json(etags_obj);
            }
        }
    }
    LightLock_Unlock(&global_response_lock);
    
    if (etags_obj) {
        cJSON_Delete(etags_obj);
        etags_obj = NULL;
    }

    if (local_response_code == 401 && !retried_after_401) {
        log_to_file("[refresh_data] 401 → refreshing token");

        ResponseBuffer refresh_buf = {0};
        struct curl_slist *refresh_headers = NULL;
        refresh_headers = curl_slist_append(refresh_headers, "Content-Type: application/json");

        char refresh_request[512];
        snprintf(refresh_request, sizeof(refresh_request),
            "{\"grant_type\":\"refresh_token\",\"refresh_token\":\"%s\",\"client_id\":\"%s\"}",
            refreshToken, KOLEO_OAUTH_KEY);

        refresh_data(curl, sklejURL("v2/main/oauth/token"), refresh_request, refresh_headers, &refresh_buf, POST);
        curl_slist_free_all(refresh_headers);

        if (refresh_buf.data) {
            parse_AuthResponse(refresh_buf.data);
            free(refresh_buf.data);
        } else {
            log_to_file("refresh failed");
            if (allocated_headers) curl_slist_free_all(allocated_headers);
            return true;
        }

        if (allocated_headers) {
            curl_slist_free_all(allocated_headers);
            allocated_headers = NULL;
        }

        for (struct curl_slist *h = headers; h != NULL; h = h->next) {
            if (strncasecmp(h->data, "Authorization:", 14) == 0) continue;
            allocated_headers = curl_slist_append(allocated_headers, h->data);
        }

        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", authToken);
        allocated_headers = curl_slist_append(allocated_headers, auth_header);

        retried_after_401 = true;
        goto retry_request;
    }

    if (!request_failed && (strstr(url, ".png") || strstr(url, ".jpeg") || strstr(url, ".jpg")) &&
        response_buf->data && response_buf->size > 0) {
        need_to_load_image = true;
    } else {
        need_to_load_image = false;
    }

    LightLock_Lock(&global_response_lock);
    loadingshit = false;
    requestdone = true;
    if (response_code == 0) czasfuckup = true;

    if (!need_to_load_image) {
        response_buf->done = true; 
    } else {
        response_buf->done = false; 
    }
    LightLock_Unlock(&global_response_lock);

    if (allocated_headers) {
        curl_slist_free_all(allocated_headers);
    }
    
    log_to_file("Response: %s", response_buf->data ? response_buf->data : "(None)");
    if (!doing_debug_logs) log_to_file("[refresh_data] Request Done");
    return request_failed;
}

// queue stuff

void request_worker(void* arg) {
    ResponseBuffer local_buf = {NULL, 0, false};
    
    CURL *worker_curl = curl_easy_init();
    if (!worker_curl) {
        log_to_file("[request_worker] FATAL: Failed to init cURL");
        return;
    }

    while (request_running) {
        LightLock_Lock(&request_lock);
        if (request_count == 0) {
            LightLock_Unlock(&request_lock);
            LightEvent_Wait(&request_event);
            continue;
        }

        Request req = request_queue[0];
        memmove(request_queue, request_queue + 1, (request_count - 1) * sizeof(Request));
        request_count--;
        LightLock_Unlock(&request_lock);

        local_buf.data = NULL;
        local_buf.size = 0;
        local_buf.done = false;

        ResponseBuffer *buf = req.response_buf;
        if (!buf) {
            buf = &local_buf;
        }

        if (req.url[0] == '\0') {
            log_to_file("[request_worker] ERROR: request has no URL");
            continue;
        }

        refresh_data(worker_curl, req.url, req.data, req.headers, buf, req.request_type);

        if (req.response && req.response_size && buf->data) {
            *req.response = buf->data;
            *req.response_size = buf->size;
        } else if (!req.response_buf && buf->data) {
            free(buf->data);
        }

        if (req.owns_data && req.data) free(req.data);
        if (req.headers) curl_slist_free_all(req.headers);

        if (need_to_load_image && buf->data && buf->size > 0) {
            if (kuponkurwa.tex) {
                C3D_TexDelete(kuponkurwa.tex);
                free(kuponkurwa.tex);
                kuponkurwa.tex = NULL;
            }
            if (kuponkurwa.subtex) {
                free(kuponkurwa.subtex);
                kuponkurwa.subtex = NULL;
            }

            unsigned char* decoded = NULL;
            unsigned width = 0, height = 0;

            unsigned error = lodepng_decode32(&decoded, &width, &height,
                                              (const unsigned char*)buf->data, buf->size);

            if (error) {
                if (decoded) { free(decoded); decoded = NULL; }

                struct jpeg_decompress_struct cinfo;
                struct my_error_mgr jerr;

                cinfo.err = jpeg_std_error(&jerr.pub);

                if (setjmp(jerr.setjmp_buffer)) {
                    jpeg_destroy_decompress(&cinfo);
                    log_to_file("JPEG jebł");
                    if (decoded) free(decoded);
                    
                    LightLock_Lock(&global_response_lock);
                    need_to_load_image = false;
                    if (req.response_buf) req.response_buf->done = true; 
                    LightLock_Unlock(&global_response_lock);
                    continue;
                }

                jpeg_create_decompress(&cinfo);
                jpeg_mem_src(&cinfo, (unsigned char*)buf->data, buf->size);
                jpeg_read_header(&cinfo, TRUE);

                cinfo.out_color_space = JCS_RGB; 
                jpeg_start_decompress(&cinfo);

                width = cinfo.output_width;
                height = cinfo.output_height;
                int channels = cinfo.output_components;

                if (channels != 3) {
                    log_to_file("masz rozjebanego jpega, ilość kanałów: %d", channels);
                    jpeg_finish_decompress(&cinfo);
                    jpeg_destroy_decompress(&cinfo);
                    
                    LightLock_Lock(&global_response_lock);
                    need_to_load_image = false;
                    if (req.response_buf) req.response_buf->done = true;
                    LightLock_Unlock(&global_response_lock);
                    continue;
                }

                int row_stride = width * channels;
                unsigned char* rgb = (unsigned char*)malloc(width * height * channels);
                
                if (rgb) {
                    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
                    unsigned char* p = rgb;
                    while (cinfo.output_scanline < height) {
                        jpeg_read_scanlines(&cinfo, buffer, 1);
                        memcpy(p, buffer[0], row_stride);
                        p += row_stride;
                    }
                }
                
                jpeg_finish_decompress(&cinfo);
                jpeg_destroy_decompress(&cinfo);

                if (!rgb) {
                    LightLock_Lock(&global_response_lock);
                    need_to_load_image = false;
                    if (req.response_buf) req.response_buf->done = true;
                    LightLock_Unlock(&global_response_lock);
                    continue;
                }

                decoded = (unsigned char*)malloc(width * height * 4);
                if (!decoded) {
                    free(rgb);
                    LightLock_Lock(&global_response_lock);
                    need_to_load_image = false;
                    if (req.response_buf) req.response_buf->done = true;
                    LightLock_Unlock(&global_response_lock);
                    continue;
                }

                for (unsigned i = 0; i < width * height; ++i) {
                    decoded[i*4 + 0] = rgb[i*3 + 0];
                    decoded[i*4 + 1] = rgb[i*3 + 1];
                    decoded[i*4 + 2] = rgb[i*3 + 2];
                    decoded[i*4 + 3] = 255;
                }
                free(rgb);
            }

            u32 tex_w = clamp(next_pow2(width), 64, 1024);
            u32 tex_h = clamp(next_pow2(height), 64, 1024);

            C3D_Tex* tex = malloc(sizeof(C3D_Tex));
            if (!tex || !C3D_TexInit(tex, tex_w, tex_h, GPU_RGBA8)) {
                log_to_file("tekstura jebła");
                free(decoded);
                if (tex) free(tex);
                
                LightLock_Lock(&global_response_lock);
                need_to_load_image = false;
                if (req.response_buf) req.response_buf->done = true;
                LightLock_Unlock(&global_response_lock);
                continue;
            }

            C3D_TexSetFilter(tex, GPU_LINEAR, GPU_NEAREST);
            memset(tex->data, 0, tex_w * tex_h * 4);

            u32 render_w = (width > tex_w) ? tex_w : width;
            u32 render_h = (height > tex_h) ? tex_h : height;

            for (u32 y = 0; y < render_h; ++y) {
                for (u32 x = 0; x < render_w; ++x) {
                    u32 src_i = (y * width + x) * 4;
                    u8 r = decoded[src_i];
                    u8 g = decoded[src_i + 1];
                    u8 b = decoded[src_i + 2];
                    u8 a = decoded[src_i + 3];

                    u32 rgba = (r << 24) | (g << 16) | (b << 8) | a;
                    u32 abgr = rgba_to_abgr(rgba);

                    u32 dst_offset = (((y >> 3) * (tex_w >> 3) + (x >> 3)) << 6) +
                                     ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                                      ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3));

                    ((u32*)tex->data)[dst_offset] = abgr;
                }
            }

            Tex3DS_SubTexture* subtex = malloc(sizeof(Tex3DS_SubTexture));
            subtex->width  = width; 
            subtex->height = height;
            subtex->left   = 0.0f;
            subtex->top    = 1.0f; 
            subtex->right  = (float)width / tex_w;
            subtex->bottom = 1.0f - ((float)height / tex_h);

            kuponkurwa.tex = tex;
            kuponkurwa.subtex = subtex;

            free(decoded);
            log_to_file("obrazek git: %dx%d", width, height);

            // SAFE ASSIGNMENT: The texture asset is completely generated and assigned
            LightLock_Lock(&global_response_lock);
            obrazekdone = true;
            need_to_load_image = false;
            if (req.response_buf) {
                req.response_buf->done = true;
            }
            LightLock_Unlock(&global_response_lock);
        } else {
            if (req.response_buf && !req.response_buf->done) {
                LightLock_Lock(&global_response_lock);
                req.response_buf->done = true;
                LightLock_Unlock(&global_response_lock);
            }
        }
    }
    curl_easy_cleanup(worker_curl);
}

//pretty self-explanatory
void queue_request(const char *url, const char *data, struct curl_slist *headers,
                   ResponseBuffer *response_buf, bool is_binary, RequestType request_type) {
    if (!url || url[0] == '\0' || !response_buf) {
        log_to_file("[queue_request] ERROR: Missing critical args");
        return;
    }

    LightLock_Lock(&request_lock);

    if (request_count >= MAX_QUEUE) {
        log_to_file("[queue_request] ERROR: Request queue full");
        LightLock_Unlock(&request_lock);
        return;
    }

    Request *req = &request_queue[request_count];
    memset(req, 0, sizeof(Request));

    strncpy(req->url, url, sizeof(req->url) - 1);
    req->url[sizeof(req->url) - 1] = '\0';

    if (data) {
        req->data = strdup(data);
        if (!req->data) {
            log_to_file("[queue_request] ERROR: strdup(data) failed");
            LightLock_Unlock(&request_lock);
            return;
        }
        req->owns_data = true;
    }

    req->headers = headers;
    req->response_buf = response_buf;
    req->is_binary = is_binary;
    req->request_type = request_type;

    request_count++;
    LightEvent_Signal(&request_event);
    LightLock_Unlock(&request_lock);
}

// thread stuff (if u want threaded requests, run this at the beggining/end of ur app)
void start_request_thread() {

    LightLock_Init(&request_lock);
    LightLock_Init(&global_response_lock);
    LightEvent_Init(&request_event, RESET_ONESHOT);
    request_thread = threadCreate(request_worker, NULL, 32 * 1024, 0x30, -2, false);
}

void stop_request_thread() {
    LightLock_Lock(&request_lock);
    request_running = false;
    LightEvent_Signal(&request_event);
    LightLock_Unlock(&request_lock);

    threadJoin(request_thread, UINT64_MAX);
    threadFree(request_thread);
}

