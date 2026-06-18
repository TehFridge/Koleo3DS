#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "lodepng.h"
#include "koleo_api.h"
#include "koleo_utils.h"
#include "cJSON.h"
#include "utils.h"

bool* current_grid = NULL;
int current_N = 0;

static inline u32 next_pow2(u32 n) {
    n--;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    n |= n >> 8; n |= n >> 16;
    n++;
    return n;
}

static inline u32 clamp(u32 n, u32 lower, u32 upper) {
    return n < lower ? lower : (n > upper ? upper : n);
}

static int is_base64_char(char c) {
    return (isalnum((unsigned char)c) || c == '+' || c == '/');
}

char* sanitize_base64(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* clean = malloc(len + 4); 
    if (!clean) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = src[i];
        if (is_base64_char(c)) {
            clean[j++] = c;
        }
    }

    size_t rem = j % 4;
    if (rem == 2) {
        clean[j++] = '=';
        clean[j++] = '=';
    } else if (rem == 3) {
        clean[j++] = '=';
    } else if (rem == 1) {
        free(clean);
        return NULL; 
    }

    clean[j] = '\0';
    return clean;
}

unsigned char* base64_decode(const char* input, size_t* out_len) {
    static const unsigned char d[] = {
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
        52,53,54,55,56,57,58,59,60,61,64,64,64, 0,64,64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
        64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64
    };

    size_t len = strlen(input);
    if (len % 4 != 0) return NULL;

    size_t out_size = len / 4 * 3;
    if (input[len - 1] == '=') out_size--;
    if (input[len - 2] == '=') out_size--;

    unsigned char* out = malloc(out_size);
    if (!out) return NULL;

    size_t i, j;
    uint32_t v;

    for (i = 0, j = 0; i < len;) {
        v = d[(unsigned char)input[i++]] << 18;
        v |= d[(unsigned char)input[i++]] << 12;
        v |= d[(unsigned char)input[i++]] << 6;
        v |= d[(unsigned char)input[i++]];

        if (j < out_size) out[j++] = (v >> 16) & 0xFF;
        if (j < out_size) out[j++] = (v >> 8) & 0xFF;
        if (j < out_size) out[j++] = v & 0xFF;
    }

    *out_len = out_size;
    return out;
}

bool is_black(u8* img, int w, int x, int y) {
    int i = (y * w + x) * 4;

    u8 r = img[i + 0];
    u8 g = img[i + 1];
    u8 b = img[i + 2];
    u8 a = img[i + 3];

    if (a < 128) return false;

    int lum = (r * 299 + g * 587 + b * 114) / 1000;
    return lum < 128;
}

void get_content_bounds(u8* img, int w, int h, int* x1, int* y1, int* x2, int* y2) {
    *x1 = w; *y1 = h; *x2 = 0; *y2 = 0;
    bool found = false;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (is_black(img, w, x, y)) {
                if (x < *x1) *x1 = x;
                if (y < *y1) *y1 = y;
                if (x > *x2) *x2 = x;
                if (y > *y2) *y2 = y;
                found = true;
            }
        }
    }
    
    if (!found) { 
        *x1 = 0; *y1 = 0; 
        *x2 = w - 1; *y2 = h - 1; 
    }
}

void try_load_base64_image(const char* base64_png_data) {
    char* clean = sanitize_base64(base64_png_data);
    if (!clean) return;

    size_t png_size;
    unsigned char* png = base64_decode(clean, &png_size);
    free(clean);
    if (!png) return;

    unsigned char* img;
    unsigned w, h;
    if (lodepng_decode32(&img, &w, &h, png, png_size)) {
        free(png);
        return;
    }
    free(png);

    int x1, y1, x2, y2;
    get_content_bounds(img, w, h, &x1, &y1, &x2, &y2);

    int contentW = (x2 - x1) + 1;
    int contentH = (y2 - y1) + 1;

    if (contentW <= 0 || contentH <= 0) {
        free(img);
        return;
    }

    int midY = y1 + contentH / 2;
    int midX = x1 + contentW / 2;

    int min_run_x = contentW;
    int run = 0;
    bool last = is_black(img, w, x1, midY);

    for (int x = x1; x <= x2; x++) {
        bool curr = is_black(img, w, x, midY);
        if (curr == last) {
            run++;
        } else {
            if (run > 2 && run < min_run_x) min_run_x = run;
            run = 1;
            last = curr;
        }
    }
    if (run > 2 && run < min_run_x) min_run_x = run;
    if (min_run_x < 1) min_run_x = 1;

    int sum_runs_x = 0;
    int count_runs_x = 0;
    run = 0;
    last = is_black(img, w, x1, midY);

    for (int x = x1; x <= x2; x++) {
        bool curr = is_black(img, w, x, midY);
        if (curr == last) {
            run++;
        } else {
            if (run > 2 && run <= min_run_x + 2) {
                sum_runs_x += run;
                count_runs_x++;
            }
            run = 1;
            last = curr;
        }
    }
    if (run > 2 && run <= min_run_x + 2) {
        sum_runs_x += run;
        count_runs_x++;
    }
    
    float avg_module_w = count_runs_x > 0 ? (float)sum_runs_x / count_runs_x : (float)min_run_x;

    int min_run_y = contentH;
    run = 0;
    last = is_black(img, w, midX, y1);

    for (int y = y1; y <= y2; y++) {
        bool curr = is_black(img, w, midX, y);
        if (curr == last) {
            run++;
        } else {
            if (run > 2 && run < min_run_y) min_run_y = run;
            run = 1;
            last = curr;
        }
    }
    if (run > 2 && run < min_run_y) min_run_y = run;
    if (min_run_y < 1) min_run_y = 1;

    int sum_runs_y = 0;
    int count_runs_y = 0;
    run = 0;
    last = is_black(img, w, midX, y1);

    for (int y = y1; y <= y2; y++) {
        bool curr = is_black(img, w, midX, y);
        if (curr == last) {
            run++;
        } else {
            if (run > 2 && run <= min_run_y + 2) {
                sum_runs_y += run;
                count_runs_y++;
            }
            run = 1;
            last = curr;
        }
    }
    if (run > 2 && run <= min_run_y + 2) {
        sum_runs_y += run;
        count_runs_y++;
    }

    float avg_module_h = count_runs_y > 0 ? (float)sum_runs_y / count_runs_y : (float)min_run_y;

    int Nx = (int)((float)contentW / avg_module_w + 0.5f);
    int Ny = (int)((float)contentH / avg_module_h + 0.5f);

    int N = (Nx + Ny) / 2;
    
    if (N >= 47 && N <= 53) N = 49;
    if (N >= 19 && N <= 23) N = 21;

    if (current_grid) free(current_grid);
    current_grid = malloc(N * N);
    if (!current_grid) {
        free(img);
        return;
    }

    float cell_w = (float)contentW / N;
    float cell_h = (float)contentH / N;

    for (int gy = 0; gy < N; gy++) {
        for (int gx = 0; gx < N; gx++) {
            
            float bias = 0.5f; 

            int px = x1 + (int)((gx + bias) * cell_w);
            int py = y1 + (int)((gy + bias) * cell_h);

            if (px >= 0 && px < (int)w && py >= 0 && py < (int)h) {
                current_grid[gy * N + gx] = is_black(img, w, px, py);
            } else {
                current_grid[gy * N + gx] = false;
            }
        }
    }

    current_N = N;

    free(img);
}

void draw_grid(bool* grid, int N) {
    int screenW = 320; 
    int screenH = 240; 

    int padding = 10;  
    int maxDisplaySize = screenH - (padding * 2); 
    
    int cellSize = maxDisplaySize / N;
    if (cellSize < 1) cellSize = 1;

    int totalGridSize = cellSize * N;

    int offsetX = (screenW - totalGridSize) / 2;
    int offsetY = (screenH - totalGridSize) / 2;

    int borderSize = 8; 
    C2D_DrawRectSolid(
        offsetX - borderSize, 
        offsetY - borderSize, 
        0.75f, 
        totalGridSize + (borderSize * 2), 
        totalGridSize + (borderSize * 2), 
        C2D_Color32(255, 255, 255, 255)
    );

    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            if (grid[y * N + x]) {
                C2D_DrawRectSolid(
                    offsetX + x * cellSize,
                    offsetY + y * cellSize,
                    0.8f, 
                    cellSize,
                    cellSize,
                    C2D_Color32(0, 0, 0, 255)
                );
            }
        }
    }
}

const char* get_current_datetime_iso() {
    static char buffer[20]; 

    time_t now = time(NULL);
    struct tm *t = localtime(&now); 

    if (!t) {
        return "1970-01-01T00:00:00";
    }

    snprintf(buffer, sizeof(buffer),
        "%04d-%02d-%02dT%02d:%02d:%02d",
        t->tm_year + 1900,
        t->tm_mon + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec
    );

    return buffer;
}

bool loadAuth(void) {
    FILE *fp = fopen("/3ds/koleo3ds/auth.json", "r");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *jsonBuffer = (char *)malloc(fileSize + 1);
    if (!jsonBuffer) {
        fclose(fp);
        return false;
    }

    fread(jsonBuffer, 1, fileSize, fp);
    jsonBuffer[fileSize] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(jsonBuffer);
    free(jsonBuffer);

    if (!root) return false;

    cJSON *access_token = cJSON_GetObjectItem(root, "access_token");
    cJSON *refresh_token_item = cJSON_GetObjectItem(root, "refresh_token");

    if (access_token && access_token->valuestring) {
        if (authToken) free(authToken);
        authToken = strdup(access_token->valuestring);
    }
    
    if (refresh_token_item && refresh_token_item->valuestring) {
        if (refreshToken) free(refreshToken);
        refreshToken = strdup(refresh_token_item->valuestring);
    }

    cJSON_Delete(root);
    return true;
}

