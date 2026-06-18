#ifndef KOLEOUTILS_H
#define KOLEOUTILS_H
static int is_base64_char(char c);
char* sanitize_base64(const char* src);
unsigned char* base64_decode(const char* input, size_t* out_len);
bool is_black(u8* img, int w, int x, int y);
int detect_grid_size(u8* img, int w, int h);
int detect_grid_size_2D(u8* img, int w, int h);
void extract_grid(u8* img, int w, int h, int N, bool* grid);
void draw_grid(bool* grid, int N);

void try_load_base64_image(const char* base64_png_data);

const char* get_current_datetime_iso();

bool loadAuth(void);
#endif
