#include "scene_bilety.h"
#include "scene_manager.h"
#include "koleo_api.h"
#include "koleo_utils.h"
#include "audio_shit.h"
#include "drawing.h"
#include "cJSON.h"
#include "scene_mainmenu.h"
#include "button.h" 
#include "sprites.h"
#include "scene_init.h"

static ActiveTicket tickets[MAX_TICKETS];
static int ticket_count = 0;
static Button ticket_buttons[MAX_TICKETS];

static bool prevTouchHeld = false;
static float cached_touch_x = 0.0f;
static float cached_touch_y = 0.0f;

#define MAX_SUB_TICKETS 5
static char current_ticket_qrs[MAX_SUB_TICKETS][16384];
static int current_sub_ticket_count = 0;
static int current_qr_index = 0;

typedef enum {
    STATE_LOADING_LIST,
    STATE_TICKET_LIST,
    STATE_LOADING_SINGLE_TICKET,
    STATE_SHOW_QR
} BiletState;

static BiletState current_state = STATE_LOADING_LIST;
static int selected_ticket_idx = -1;
static float spinner_angle = 0.0f;

static float fade_alpha = 1.0f;
static bool fading_in = true;

static char* read_file_to_string(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(f);
        return NULL;
    }
    char* string = malloc(fsize + 1);
    if (!string) {
        fclose(f);
        return NULL;
    }
    fread(string, 1, fsize, f);
    fclose(f);
    string[fsize] = '\0';
    return string;
}

static void write_string_to_file(const char* path, const char* data) {
    if (!data) return;
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, strlen(data), f);
        fclose(f);
    }
}

static float scroll_y = 0.0f;
static float scroll_velocity = 0.0f;
static bool touching = false;
static float last_touch_x = 0.0f;
static float last_touch_y = 0.0f;
static bool touch_consumed = false;

static GFX_TEXTBUF static_buf;
static GFX_TEXTBUF dyn_buf;
static GFX_TEXTBUF nav_buf; 
static GFX_TEXTBUF top_info_buf;

static GFX_TEXT title_text, hint_text, nav_text;
static GFX_TEXT top_info_text;
static GFX_TEXT ticket_texts[MAX_TICKETS];

extern bool* current_grid;
extern int current_N;

void update_nav_text() {
    GFX_TextBufClear(nav_buf);
    char buf[64] = "";
    if (current_sub_ticket_count > 1) {
        snprintf(buf, sizeof(buf), "<- Bilet %d z %d ->", current_qr_index + 1, current_sub_ticket_count);
    }
    GFX_TextParse(&nav_text, nav_buf, buf);
    GFX_TextOptimize(&nav_text);
}

void parse_ActiveTicketsList(const char* json_string) {
    ticket_count = 0;
    cJSON *root = cJSON_Parse(json_string);
    if (!root) return;

    if (cJSON_IsArray(root)) {
        int count = cJSON_GetArraySize(root);
        for (int i = 0; i < count && i < MAX_TICKETS; i++) {
            cJSON *order = cJSON_GetArrayItem(root, i);
            cJSON *id = cJSON_GetObjectItem(order, "id");
            cJSON *name = cJSON_GetObjectItem(order, "name");
            cJSON *valid_to = cJSON_GetObjectItem(order, "valid_to");
            cJSON *valid_from = cJSON_GetObjectItem(order, "valid_from");

            tickets[ticket_count].order_id = id ? (long long)id->valuedouble : 0;

            if (cJSON_IsString(name)) {
                strncpy(tickets[ticket_count].name, name->valuestring, 127);
                tickets[ticket_count].name[127] = '\0';
            }
            
            if (cJSON_IsString(valid_to) && strlen(valid_to->valuestring) >= 16) {
                format_time(valid_to->valuestring, tickets[ticket_count].valid_to, 32);
            }

            if (cJSON_IsString(valid_from) && strlen(valid_from->valuestring) >= 16) {
                format_time(valid_from->valuestring, tickets[ticket_count].valid_from, 32);
            }

            ticket_count++;
        }
    }
    cJSON_Delete(root);

    GFX_TextBufClear(dyn_buf);
    for (int i = 0; i < ticket_count; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s\n%s -> %s",
                 tickets[i].name,
                 tickets[i].valid_from,
                 tickets[i].valid_to);

        GFX_TextParse(&ticket_texts[i], dyn_buf, buf);
        GFX_TextOptimize(&ticket_texts[i]);
    }
}

void parse_SingleTicketDetail(const char* json_string, int idx) {
    current_sub_ticket_count = 0;
    current_qr_index = 0;
    memset(current_ticket_qrs, 0, sizeof(current_ticket_qrs));

    cJSON *root = cJSON_Parse(json_string);
    if (!root) return;

    cJSON *tickets_arr = cJSON_GetObjectItem(root, "tickets");
    if (cJSON_IsArray(tickets_arr)) {
        int count = cJSON_GetArraySize(tickets_arr);
        for (int i = 0; i < count && i < MAX_SUB_TICKETS; i++) {
            cJSON *t = cJSON_GetArrayItem(tickets_arr, i);
            cJSON *b64 = cJSON_GetObjectItem(t, "base64_img");
            if (cJSON_IsString(b64)) {
                strncpy(current_ticket_qrs[current_sub_ticket_count], b64->valuestring, 16383);
                current_ticket_qrs[current_sub_ticket_count][16383] = '\0';
                current_sub_ticket_count++;
            }
        }
    }
    cJSON_Delete(root);

    if (current_sub_ticket_count > 0) {
        try_load_base64_image(current_ticket_qrs[0]);
    }
    update_nav_text();
}

void sceneBiletyInit(void) {
    stopAudio(2);
    playAudio(3, true, 0.25f);
    bgm_playing = false;

    current_state = STATE_LOADING_LIST;
    selected_ticket_idx = -1;
    scroll_y = 0;

    fade_alpha = 1.0f;
    fading_in = true;

    static_buf = GFX_TextBufNew(256);
    dyn_buf = GFX_TextBufNew(8192);
    nav_buf = GFX_TextBufNew(128);
    top_info_buf = GFX_TextBufNew(256);

    GFX_TextParse(&title_text, static_buf, "MOJE BILETY");
    GFX_TextParse(&hint_text, static_buf, "B = Powrot");

    char* cached_list = read_file_to_string("/3ds/Koleo3DS/bilety_list.json");
    if (cached_list) {
        parse_ActiveTicketsList(cached_list);
        free(cached_list);
        current_state = STATE_TICKET_LIST;
    }

    if (osGetWifiStrength() > 0) {
        pobierz_bilety.done = false;
        pobierz_Bilety();
    } else {
        if (ticket_count > 0) {
            current_state = STATE_TICKET_LIST;
        }
    }
    for (int i = 0; i < MAX_TICKETS; i++) {
        
        Create_button(&ticket_buttons[i], 10.0f, 0.0f, 300.0f, 55.0f, NULL, NULL, 
                      GFX_COLOR_RGBA(255, 255, 255, 240), GFX_COLOR_RGBA(220, 220, 220, 255), 0.8f);
    }
}

void sceneBiletyUpdate(uint32_t kDown, uint32_t kHeld) {

    if (kDown & KEY_B) {
        if (current_state == STATE_SHOW_QR) {
            current_state = STATE_TICKET_LIST;
            if (current_grid) { free(current_grid); current_grid = NULL; }
        } else {
            if (osGetWifiStrength() > 0) {
                sceneManagerSwitchTo(SCENE_MAINMENU);
            } 
        }
        return;
    }

    if (fading_in) {
        fade_alpha *= 0.90f;
        if (fade_alpha < 0.01f) {
            fade_alpha = 0.0f;
            fading_in = false;
        }
    }

    if (current_state == STATE_SHOW_QR) {
        if (kDown & (KEY_RIGHT | KEY_R | KEY_CPAD_RIGHT)) {
            if (current_qr_index < current_sub_ticket_count - 1) {
                current_qr_index++;
                try_load_base64_image(current_ticket_qrs[current_qr_index]);
                update_nav_text();
            }
        }
        if (kDown & (KEY_LEFT | KEY_L | KEY_CPAD_LEFT)) {
            if (current_qr_index > 0) {
                current_qr_index--;
                try_load_base64_image(current_ticket_qrs[current_qr_index]);
                update_nav_text();
            }
        }
    }

    if (pobierz_bilety.done) {
        if (pobierz_bilety.data && pobierz_bilety.data[0] == '[') {
            write_string_to_file("/3ds/Koleo3DS/bilety_list.json", pobierz_bilety.data);
            parse_ActiveTicketsList(pobierz_bilety.data);
        }
        if (current_state == STATE_LOADING_LIST) {
            current_state = STATE_TICKET_LIST;
        }
        pobierz_bilety.done = false;
    }

    if (current_state == STATE_LOADING_SINGLE_TICKET && pobierz_bilet.done) {
        if (pobierz_bilet.data && pobierz_bilet.data[0] == '{') {
            char path[128];
            snprintf(path, sizeof(path), "/3ds/Koleo3DS/bilet_%lld.json", tickets[selected_ticket_idx].order_id);
            write_string_to_file(path, pobierz_bilet.data);
            parse_SingleTicketDetail(pobierz_bilet.data, selected_ticket_idx);
            current_state = STATE_SHOW_QR;
        } else {
            current_state = STATE_TICKET_LIST;
        }
        pobierz_bilet.done = false;
    }

    if (current_state == STATE_LOADING_LIST || current_state == STATE_LOADING_SINGLE_TICKET) {
        spinner_angle += 0.15f;
        return; 
    }

    touchPosition touch;
    hidTouchRead(&touch);

    bool touchDown = (kDown & KEY_TOUCH) != 0;
    bool touchHeld = (kHeld & KEY_TOUCH) != 0;
    bool touchUp   = (!touchHeld && prevTouchHeld);
    prevTouchHeld  = touchHeld;

    if (touchDown || touchHeld) {
        cached_touch_x = touch.px;
        cached_touch_y = touch.py;
    }

    if (touchUp) {
        if (touch_consumed) {
            
            touch.px = 999; 
            touch.py = 999;
        } else {
            touch.px = cached_touch_x;
            touch.py = cached_touch_y;
        }
    }

    if (touchDown) {
        touching = true;
        last_touch_y = touch.py;
        last_touch_x = touch.px;
        touch_consumed = false;
        scroll_velocity = 0.0f;
    } else if (touchHeld && touching) {
        float delta_y = touch.py - last_touch_y;
        if (fabs(delta_y) > 3.0f) { 
            touch_consumed = true; 
        }
        if (touch_consumed) {
            scroll_y -= delta_y;
            scroll_velocity = -delta_y * 0.25f; 
        }
        last_touch_y = touch.py;
    } else if (!touchHeld) {
        touching = false;
    }

    if (kHeld & KEY_UP)   scroll_y -= 8.0f;
    if (kHeld & KEY_DOWN) scroll_y += 8.0f;

    if (current_state == STATE_TICKET_LIST) {
        float base_y = 10.0f;
        for (int i = 0; i < ticket_count && i < MAX_TICKETS; i++) {
            float y = base_y + (i * 60.0f) - scroll_y;
            
            ticket_buttons[i].x = 10;
            ticket_buttons[i].y = y; 
            ticket_buttons[i].width = 300;
            ticket_buttons[i].height = 55;

            Update_button(&ticket_buttons[i], touch, touchDown, touchHeld, touchUp);

            if (ticket_buttons[i].clicked) {
                selected_ticket_idx = i;

                GFX_TextBufClear(top_info_buf);
                char info_str[256];
                snprintf(info_str, sizeof(info_str),
                         "%s\nOd: %s\nDo: %s",
                         tickets[i].name,
                         tickets[i].valid_from,
                         tickets[i].valid_to);

                GFX_TextParse(&top_info_text, top_info_buf, info_str);
                GFX_TextOptimize(&top_info_text);

                char path[128];
                snprintf(path, sizeof(path), "/3ds/Koleo3DS/bilet_%lld.json",
                         tickets[i].order_id);

                char* cached_ticket = read_file_to_string(path);

                if (cached_ticket) {
                    parse_SingleTicketDetail(cached_ticket, selected_ticket_idx);
                    free(cached_ticket);
                    current_state = STATE_SHOW_QR;
                } else {
                    pobierz_bilet.done = false;
                    pobierz_Bilet(tickets[i].order_id);
                    current_state = STATE_LOADING_SINGLE_TICKET;
                }
                break;
            }
        }
    }

    if (current_state == STATE_TICKET_LIST) {
        scroll_y += scroll_velocity;
        scroll_velocity *= 0.92f; 

        if (fabs(scroll_velocity) < 0.05f)
            scroll_velocity = 0.0f;
    }

    float item_height = 60.0f;
    float content_height = 0;

    if (current_state == STATE_TICKET_LIST) {
        content_height = ticket_count * item_height;
    }

    float max_scroll = content_height - 180.0f; 
    if (max_scroll < 0) max_scroll = 0;

    if (scroll_y < 0) {
        scroll_y = 0;
        scroll_velocity = 0;
    }

    if (scroll_y > max_scroll) {
        scroll_y = max_scroll;
        scroll_velocity = 0;
    }
}

void sceneBiletyRender(void) {

    for (int side = 0; side < 2; side++) {
        GFX_BeginSceneTop(side, true);

        GFX_DrawRectSolid(0, 0, 0.1f, 400, 240, GFX_COLOR_RGBA(48,74,130,255));
        
        narysujpociagi(40);

        if (current_state == STATE_SHOW_QR || current_state == STATE_LOADING_SINGLE_TICKET) {
            GFX_DrawRectSolid(0, 60, 0.5f, 400, 100, GFX_COLOR_RGBA(0,0,0,160));
            GFX_DrawShadowedText(&top_info_text, 200, 80, 0.5f, 0.45f, 0.6f,
                                 GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,255));
        } else {
            if (VOICEACT == 1) {
                if (ticket_count > 0) {
                    GFX_DrawImageAt(kun_excited, 10, 20, 0.5f, NULL, 1.0f, 1.0f);
                } else {
                    GFX_DrawImageAt(kun_diss, 10, 20, 0.5f, NULL, 1.0f, 1.0f);
                }
            } else {
                if (ticket_count > 0) {
                    GFX_DrawImageAt(chan_excited, 10, 20, 0.5f, NULL, 1.0f, 1.0f);
                } else {
                    GFX_DrawImageAt(chan_diss, 10, 20, 0.5f, NULL, 1.0f, 1.0f);
                }
            }
            GFX_DrawRectSolid(0, 0, 0.8f, 400, 60, GFX_COLOR_RGBA(255,255,255,190));
            GFX_DrawRectSolid(0, 60, 0.7f, 400, 4, GFX_COLOR_RGBA(0,0,0,80));
            GFX_DrawText(&title_text, 200, 15, 0.9f, 0.7f, 0.7f,
                         GFX_ALIGN_CENTER, GFX_COLOR_RGBA(48,74,130,255));
        }
        if (osGetWifiStrength() > 0) {
            GFX_DrawShadowedText(&hint_text, 200, 210, 0.5f, 0.6f, 0.6f,
                                GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,255));
        }
        if (current_sub_ticket_count > 1) {
            GFX_DrawShadowedText(&nav_text, 200, 190, 0.9f, 0.5f, 0.5f,
                         GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,255));
        }
        
        if (fade_alpha > 0.0f) {
            u8 a = (u8)(fade_alpha * 255);
            GFX_DrawRectSolid(0, 0, 1.0f, 400, 240, GFX_COLOR_RGBA(0,0,0,a));
        }
    }

    GFX_BeginSceneBottom();

    GFX_DrawRectSolid(0, 0, 0.1f, 320, 240, GFX_COLOR_RGBA(48,74,130,255));
    narysujpociagi(40);
    GFX_DrawRectSolid(0, 0, 0.6f, 320, 240, GFX_COLOR_RGBA(0,0,0,120));

    if (current_state == STATE_LOADING_LIST || current_state == STATE_LOADING_SINGLE_TICKET) {
        float cx = 160, cy = 120;
        for (int i = 0; i < 8; i++) {
            float a = spinner_angle - i*(M_PI/4);
            GFX_DrawLine(cx + cos(a)*10, cy + sin(a)*10,
                         GFX_COLOR_RGBA(255,255,255,255 - i*30),
                         cx + cos(a)*18, cy + sin(a)*18,
                         GFX_COLOR_RGBA(255,255,255,255 - i*30),
                         3.0f, 0.8f);
        }
    } else if (current_state == STATE_TICKET_LIST) {
        for (int i = 0; i < ticket_count; i++) {
            float y = 10.0f + (i * 60.0f) - scroll_y;
            if (y < -60 || y > 240) continue;

            ticket_buttons[i].y = y;

            Draw_button(&ticket_buttons[i]);

            GFX_DrawText(&ticket_texts[i], 15, y + 10, 0.9f, 0.4f, 0.5f,
                         GFX_ALIGN_LEFT, GFX_COLOR_RGBA(48, 74, 130, 255));
        }
    } else if (current_state == STATE_SHOW_QR) {
        if (current_grid) {
            draw_grid(current_grid, current_N);
        }
    }

    if (fade_alpha > 0.0f) {
        u8 a = (u8)(fade_alpha * 255);
        GFX_DrawRectSolid(0, 0, 1.0f, 320, 240, GFX_COLOR_RGBA(0,0,0,a));
    }
}

void sceneBiletyExit(void) {
    stopAudio(3);
    if (current_grid) { free(current_grid); current_grid = NULL; }

    GFX_TextBufDelete(static_buf);
    GFX_TextBufDelete(dyn_buf);
    GFX_TextBufDelete(nav_buf);
    GFX_TextBufDelete(top_info_buf);
}

