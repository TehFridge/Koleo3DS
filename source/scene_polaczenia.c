#include "scene_polaczenia.h"
#include "scene_mainmenu.h"
#include "scene_manager.h"
#include "drawing.h"
#include "data.h"
#include "sprites.h"
#include "koleo_api.h"
#include "koleo_utils.h"
#include "audio_shit.h"
#include "utils.h"
#include "button.h"
#include <time.h>
#include <math.h>

typedef enum {
    STATE_FORM,
    STATE_SELECT_START,
    STATE_SELECT_END,
    STATE_SELECT_DATE,
    STATE_SELECT_HOUR,
    STATE_LOADING,
    STATE_RESULTS,
    STATE_FETCHING_ID
} PolaczeniaState;

static PolaczeniaState current_state = STATE_FORM;

static float fade_alpha = 255.0f;
static float spinner_angle = 0.0f;

static int start_station_id = -1;
static int end_station_id = -1;
static char start_station_name[128] = "Wybierz stacje początkową...";
static char end_station_name[128] = "Wybierz stacje końcową...";

static int selected_day_offset = 0; 
static int selected_hour = -1;      
static char datetime_button_name[128] = "Kiedy: Teraz";

static float scroll_y = 0.0f;
static float scroll_velocity = 0.0f;
static bool touching = false;
static float last_touch_y = 0.0f;
static float last_touch_x = 0.0f;
static bool touch_consumed = false;
static bool prevTouchHeld = false; 

static u16 cached_touch_x = 0;
static u16 cached_touch_y = 0;

static Stacja_List current_search_list = {NULL, 0};
static bool connections_parsed = false;
static int selected_polaczenie_idx = -1;

extern long long biletkup_connection_id; 

static GFX_TEXTBUF static_buf;
static GFX_TEXTBUF dyn_buf;
static GFX_TEXTBUF header_buf; 

static GFX_TEXT title_text;
static GFX_TEXT hint_text;
static GFX_TEXT btn_start_text;
static GFX_TEXT btn_end_text;
static GFX_TEXT btn_datetime_text;
static GFX_TEXT btn_search_text;

static GFX_TEXT top_start_text; 
static GFX_TEXT top_end_text;   

#define MAX_LIST_ITEMS 64 
static GFX_TEXT list_texts[MAX_LIST_ITEMS];

static Button btn_start;
static Button btn_end;
static Button btn_date;
static Button btn_search;

static Button list_buttons[MAX_LIST_ITEMS];

static void RebuildDynamicTextsForStations(void) {
    GFX_TextBufClear(dyn_buf);
    for (int i = 0; i < current_search_list.count && i < MAX_LIST_ITEMS; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s", current_search_list.station[i].nazwa);
        GFX_TextParse(&list_texts[i], dyn_buf, buf);
        GFX_TextOptimize(&list_texts[i]);
    }
}

static void RebuildDynamicTextsForDates(void) {
    GFX_TextBufClear(dyn_buf);
    time_t t = time(NULL);
    for(int i = 0; i <= 30; i++) {
        struct tm *tm_info = localtime(&t);
        char buf[64];
        if (i == 0) snprintf(buf, sizeof(buf), "Dzisiaj (%02d.%02d.%04d)", tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900);
        else if (i == 1) snprintf(buf, sizeof(buf), "Jutro (%02d.%02d.%04d)", tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900);
        else snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900);

        GFX_TextParse(&list_texts[i], dyn_buf, buf);
        GFX_TextOptimize(&list_texts[i]);
        t += 86400; 
    }
}

static void RebuildDynamicTextsForHours(void) {
    GFX_TextBufClear(dyn_buf);
    for(int i = 0; i < 24; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%02d:00", i);
        GFX_TextParse(&list_texts[i], dyn_buf, buf);
        GFX_TextOptimize(&list_texts[i]);
    }
}

static void RebuildDynamicTextsForConnections(void) {
    GFX_TextBufClear(dyn_buf);
    for (int i = 0; i < global_polaczenia_list.count && i < MAX_LIST_ITEMS; i++) {
        char buf[256];
        Polaczenie* p = &global_polaczenia_list.polaczenia[i];
        
        if (p->bezposrednie || p->przesiadki == 0) {
            snprintf(buf, sizeof(buf), "%s (%d min) | bezpośredni", 
                p->nazwa, p->dlugosc_trasy);
        } else {
            char przesiadki_str[32];
            if (p->przesiadki == 1) {
                strcpy(przesiadki_str, "1 przesiadka");
            } else if (p->przesiadki % 10 >= 2 && p->przesiadki % 10 <= 4 && (p->przesiadki % 100 < 10 || p->przesiadki % 100 >= 20)) {
                snprintf(przesiadki_str, sizeof(przesiadki_str), "%d przesiadki", p->przesiadki);
            } else {
                snprintf(przesiadki_str, sizeof(przesiadki_str), "%d przesiadek", p->przesiadki);
            }
            snprintf(buf, sizeof(buf), "%s (%d min) | %s", 
                p->nazwa, p->dlugosc_trasy, przesiadki_str);
        }
        
        GFX_TextParse(&list_texts[i], dyn_buf, buf);
        GFX_TextOptimize(&list_texts[i]);
    }
}

static void RebuildFormTexts(void) {
    GFX_TextBufClear(dyn_buf);
    
    GFX_TextParse(&btn_start_text, dyn_buf, start_station_name);
    GFX_TextOptimize(&btn_start_text);

    GFX_TextParse(&btn_end_text, dyn_buf, end_station_name);
    GFX_TextOptimize(&btn_end_text);

    GFX_TextParse(&btn_datetime_text, dyn_buf, datetime_button_name);
    GFX_TextOptimize(&btn_datetime_text);
}

static void ExecuteStationSearch(const char* query, bool isStart) {
    if (current_search_list.station) {
        free(current_search_list.station);
        current_search_list.station = NULL;
        current_search_list.count = 0;
    }

    FILE *fp = fopen("/3ds/koleo3ds/all_stacjas.json", "r");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char *jsonBuffer = malloc(size + 1);
        if (jsonBuffer) {
            fread(jsonBuffer, 1, size, fp);
            jsonBuffer[size] = '\0';
            fclose(fp);

            current_search_list = search_stations(jsonBuffer, query);
            free(jsonBuffer);
        } else {
            fclose(fp);
        }
    }

    current_state = isStart ? STATE_SELECT_START : STATE_SELECT_END;
    scroll_y = 0.0f;
    scroll_velocity = 0.0f;
    RebuildDynamicTextsForStations();
}

void scenePolaczeniaInit(void) {   
    if (!bgm_playing) {
        playAudio(2, true, 0.4f);
        bgm_playing = true;
    }
    fade_alpha = 255.0f;
    spinner_angle = 0.0f;
    current_state = STATE_FORM;

    start_station_id = -1;
    end_station_id = -1;
    snprintf(start_station_name, sizeof(start_station_name), "Wybierz stacje początkową...");
    snprintf(end_station_name, sizeof(end_station_name), "Wybierz stacje końcową...");

    selected_day_offset = 0;
    selected_hour = -1;
    snprintf(datetime_button_name, sizeof(datetime_button_name), "Kiedy: Teraz");

    scroll_y = 0.0f;
    scroll_velocity = 0.0f;
    touching = false;
    touch_consumed = false;
    prevTouchHeld = false;
    connections_parsed = false;

    if (current_search_list.station) {
        free(current_search_list.station);
        current_search_list.station = NULL;
    }

    static_buf = GFX_TextBufNew(256);
    dyn_buf = GFX_TextBufNew(4096); 
    header_buf = GFX_TextBufNew(256); 

    GFX_TextParse(&title_text, static_buf, "WYSZUKAJ POŁĄCZENIE");
    GFX_TextParse(&hint_text, static_buf, "B = Powrót");
    GFX_TextParse(&btn_search_text, static_buf, "Szukaj Połączeń");

    GFX_TextOptimize(&title_text);
    GFX_TextOptimize(&hint_text);
    GFX_TextOptimize(&btn_search_text);

    RebuildFormTexts();

    Create_button(&btn_start, 10, 10, 300, 40, NULL, NULL,
        GFX_COLOR_RGBA(255,255,255,240),
        GFX_COLOR_RGBA(80,180,255,255), 0.8f);

    Create_button(&btn_end, 10, 55, 300, 40, NULL, NULL,
        GFX_COLOR_RGBA(255,255,255,240),
        GFX_COLOR_RGBA(80,180,255,255), 0.8f);

    Create_button(&btn_date, 10, 100, 300, 40, NULL, NULL,
        GFX_COLOR_RGBA(255,255,255,240),
        GFX_COLOR_RGBA(80,180,255,255), 0.8f);

    Create_button(&btn_search, 60, 150, 200, 50, NULL, NULL,
        GFX_COLOR_RGBA(150,150,150,255),
        GFX_COLOR_RGBA(80,180,80,255), 0.8f);

    for (int i = 0; i < MAX_LIST_ITEMS; i++) {
        Create_button(&list_buttons[i], 10, 0, 300, 45, NULL, NULL,
            GFX_COLOR_RGBA(200,200,200,255),
            GFX_COLOR_RGBA(150,150,150,255), 0.8f);
    }
}

void scenePolaczeniaUpdate(uint32_t kDown, uint32_t kHeld) {

    if (kDown & KEY_B) {
        if (current_state != STATE_FORM &&
            current_state != STATE_LOADING &&
            current_state != STATE_FETCHING_ID) {
            current_state = STATE_FORM;
            RebuildFormTexts();
        } else {
            sceneManagerSwitchTo(SCENE_MAINMENU);
        }
        return;
    }

    if (fade_alpha > 0.0f) {
        fade_alpha -= 5.0f;
        if (fade_alpha < 0.0f) fade_alpha = 0.0f;
    }

    if (current_state == STATE_LOADING) {
        spinner_angle += 0.15f;

        if (dawaj_stacje_for_me.done && !connections_parsed) {
            if (global_polaczenia_list.polaczenia) {
                free(global_polaczenia_list.polaczenia);
            }

            global_polaczenia_list = parse_Polaczenia(dawaj_stacje_for_me.data);
            connections_parsed = true;

            current_state = STATE_RESULTS;
            scroll_y = 0.0f;
            scroll_velocity = 0.0f;

            RebuildDynamicTextsForConnections();
        }
        return;
    }

    if (current_state == STATE_FETCHING_ID) {
        spinner_angle += 0.15f;

        if (dawaj_connection_id.done) {
            long long c_id = parse_ConnectionId(dawaj_connection_id.data);

            if (c_id > 0) {
                biletkup_connection_id = c_id;
                sceneManagerSwitchTo(SCENE_BILETKUP);
            } else {
                current_state = STATE_RESULTS;
            }
        }
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

    Update_button(&btn_start, touch, touchDown, touchHeld, touchUp);
    Update_button(&btn_end, touch, touchDown, touchHeld, touchUp);
    Update_button(&btn_date, touch, touchDown, touchHeld, touchUp);
    Update_button(&btn_search, touch, touchDown, touchHeld, touchUp);

    if (kHeld & KEY_UP)   scroll_y -= 8.0f;
    if (kHeld & KEY_DOWN) scroll_y += 8.0f;
    if (kHeld & KEY_LEFT)  scroll_y -= 4.0f;
    if (kHeld & KEY_RIGHT) scroll_y += 4.0f;

    if (current_state == STATE_FORM) {
        if (btn_start.clicked) {
            playNavAudio(10, true);
            SwkbdState swkbd;
            char buf[64] = "";
            swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 1, -1);
            swkbdSetHintText(&swkbd, "Wpisz nazwe stacji początkowej");
            if (swkbdInputText(&swkbd, buf, sizeof(buf)) && strlen(buf) > 0) {
                ExecuteStationSearch(buf, true);
            }
        }

        if (btn_end.clicked) {
            playNavAudio(9, true);
            SwkbdState swkbd;
            char buf[64] = "";
            swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 1, -1);
            swkbdSetHintText(&swkbd, "Wpisz nazwe stacji końcowej");
            if (swkbdInputText(&swkbd, buf, sizeof(buf)) && strlen(buf) > 0) {
                ExecuteStationSearch(buf, false);
            }
        }

        if (btn_date.clicked) {
            current_state = STATE_SELECT_DATE;
            scroll_y = 0;
            scroll_velocity = 0;
            RebuildDynamicTextsForDates();
        }

        if (btn_search.clicked && start_station_id != -1 && end_station_id != -1) {
            playNavAudio(12, true);
            dawaj_stacje_for_me.done = false;
            connections_parsed = false;
            char iso_date[64];

            if (selected_hour == -1) {
                strncpy(iso_date, get_current_datetime_iso(), sizeof(iso_date));
            } else {
                time_t t = time(NULL);
                t += selected_day_offset * 86400;

                struct tm *tm_info = localtime(&t);
                tm_info->tm_hour = selected_hour;
                tm_info->tm_min = 0;
                tm_info->tm_sec = 0;
                mktime(tm_info);

                strftime(iso_date, sizeof(iso_date),
                         "%Y-%m-%dT%H:%M:%S.000Z", tm_info);
            }

            getStacje_for_Me(start_station_id, end_station_id, iso_date, false);
            GFX_TextBufClear(header_buf);

            GFX_TextParse(&top_start_text, header_buf, start_station_name);
            GFX_TextOptimize(&top_start_text);

            GFX_TextParse(&top_end_text, header_buf, end_station_name);
            GFX_TextOptimize(&top_end_text);

            current_state = STATE_LOADING;
        }
    }

    if (current_state == STATE_SELECT_START ||
        current_state == STATE_SELECT_END) {

        float base_y = 10.0f;
        for (int i = 0; i < current_search_list.count && i < MAX_LIST_ITEMS; i++) {
            float y = base_y + (i * 50.0f) - scroll_y;
            
            list_buttons[i].y = y; 
            Update_button(&list_buttons[i], touch, touchDown, touchHeld, touchUp);

            if (list_buttons[i].clicked) {
                if (current_state == STATE_SELECT_START) {
                    start_station_id = current_search_list.station[i].stacja_id;
                    strncpy(start_station_name, current_search_list.station[i].nazwa, 127);
                } else {
                    end_station_id = current_search_list.station[i].stacja_id;
                    strncpy(end_station_name, current_search_list.station[i].nazwa, 127);
                }

                RebuildFormTexts();
                current_state = STATE_FORM;
                break;
            }
        }
    }

    if (current_state == STATE_SELECT_DATE) {
        float base_y = 10.0f;
        for (int i = 0; i < 31 && i < MAX_LIST_ITEMS; i++) {
            float y = base_y + (i * 50.0f) - scroll_y;
            
            list_buttons[i].y = y;
            Update_button(&list_buttons[i], touch, touchDown, touchHeld, touchUp);

            if (list_buttons[i].clicked) {
                selected_day_offset = i;
                current_state = STATE_SELECT_HOUR;
                scroll_y = 0;
                scroll_velocity = 0;
                RebuildDynamicTextsForHours();
                break;
            }
        }
    }

    if (current_state == STATE_SELECT_HOUR) {
        float base_y = 10.0f;
        for (int i = 0; i < 24 && i < MAX_LIST_ITEMS; i++) {
            float y = base_y + (i * 50.0f) - scroll_y;

            list_buttons[i].y = y;
            Update_button(&list_buttons[i], touch, touchDown, touchHeld, touchUp);

            if (list_buttons[i].clicked) {
                selected_hour = i;
                time_t t = time(NULL);
                t += selected_day_offset * 86400;
                struct tm *tm_info = localtime(&t);

                snprintf(datetime_button_name, sizeof(datetime_button_name),
                         "Kiedy: %02d.%02d.%04d %02d:00",
                         tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900, selected_hour);

                RebuildFormTexts();
                current_state = STATE_FORM;
                break;
            }
        }
    }

    if (current_state == STATE_RESULTS) {
        float base_y = 10.0f;
        for (int i = 0; i < global_polaczenia_list.count && i < MAX_LIST_ITEMS; i++) {
            float y = base_y + (i * 50.0f) - scroll_y;
            
            list_buttons[i].y = y;
            Update_button(&list_buttons[i], touch, touchDown, touchHeld, touchUp);

            if (list_buttons[i].clicked) {
                dawaj_connection_id.done = false;
                get_ConnectionId(global_polaczenia_list.polaczenia[i].uuid);
                if (global_polaczenia_list.polaczenia[i].bezposrednie) {
                    playNavAudio(7, true);
                } else {
                    playNavAudio(8, true);
                }

                selected_polaczenie_idx = i;
                current_state = STATE_FETCHING_ID;
                break;
            }
        }
    }

    if (current_state == STATE_SELECT_START ||
        current_state == STATE_SELECT_END ||
        current_state == STATE_RESULTS ||
        current_state == STATE_SELECT_DATE ||
        current_state == STATE_SELECT_HOUR) {

        scroll_y += scroll_velocity;
        scroll_velocity *= 0.92f; 

        if (fabs(scroll_velocity) < 0.05f)
            scroll_velocity = 0.0f;
    }

    float item_height = 50.0f;
    float content_height = 0;

    if (current_state == STATE_SELECT_START || current_state == STATE_SELECT_END) {
        content_height = current_search_list.count * item_height;
    } else if (current_state == STATE_RESULTS) {
        content_height = global_polaczenia_list.count * item_height;
    } else if (current_state == STATE_SELECT_DATE) {
        content_height = 31 * item_height;
    } else if (current_state == STATE_SELECT_HOUR) {
        content_height = 24 * item_height;
    }

    float max_scroll = content_height - 200.0f;
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

void scenePolaczeniaRender(void) {

    for (int side = 0; side < 2; side++) {
        GFX_BeginSceneTop(side, true);
        GFX_DrawRectSolid(0, 0, 0.1f, 400, 240, GFX_COLOR_RGBA(48,74,130,255));
        narysujpociagi(40);
        
        GFX_DrawRectSolid(0, 0, 0.8f, 400, 50, GFX_COLOR_RGBA(255,255,255,190));
        GFX_DrawRectSolid(0, 50, 0.7f, 400, 4, GFX_COLOR_RGBA(0,0,0,80));
        GFX_DrawText(&title_text, 200, 15, 0.9f, 0.7f, 0.7f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(48,74,130,255));

        if (current_state == STATE_LOADING || current_state == STATE_RESULTS || current_state == STATE_FETCHING_ID) {
            GFX_DrawRectSolid(40, 95, 0.6f, 320, 60, GFX_COLOR_RGBA(0, 0, 0, 160));
            GFX_DrawText(&top_start_text, 200, 105, 0.9f, 0.65f, 0.65f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
            GFX_DrawText(&top_end_text, 200, 130, 0.9f, 0.65f, 0.65f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
        }

        GFX_DrawShadowedText(&hint_text, 200, 210, 0.5f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,255));
        
        if (fade_alpha > 0)
            GFX_DrawRectSolid(0,0,1.0f,400,240, GFX_COLOR_RGBA(0,0,0,(u8)fade_alpha));
    }

    GFX_BeginSceneBottom();
    GFX_DrawRectSolid(0, 0, 0.1f, 320, 240, GFX_COLOR_RGBA(48,74,130,255));
    narysujpociagi(40);
    GFX_DrawRectSolid(0, 0, 0.6f, 320, 240, GFX_COLOR_RGBA(0,0,0,120));

    if (current_state == STATE_FORM) {
        
        GFX_DrawRectSolid(12, 12, 0.7f, 296, 36, GFX_COLOR_RGBA(0,0,0,100));
        GFX_DrawRectSolid(10, 10, 0.8f, 300, 40, GFX_COLOR_RGBA(255,255,255,240));
        GFX_DrawText(&btn_start_text, 20, 23, 0.9f, 0.55f, 0.55f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(48, 74, 130, 255));

        GFX_DrawRectSolid(12, 57, 0.7f, 296, 36, GFX_COLOR_RGBA(0,0,0,100)); 
        GFX_DrawRectSolid(10, 55, 0.8f, 300, 40, GFX_COLOR_RGBA(255,255,255,240));
        GFX_DrawText(&btn_end_text, 20, 68, 0.9f, 0.55f, 0.55f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(48, 74, 130, 255));

        GFX_DrawRectSolid(12, 102, 0.7f, 296, 36, GFX_COLOR_RGBA(0,0,0,100)); 
        GFX_DrawRectSolid(10, 100, 0.8f, 300, 40, GFX_COLOR_RGBA(255,255,255,240));
        GFX_DrawText(&btn_datetime_text, 20, 113, 0.9f, 0.55f, 0.55f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(48, 74, 130, 255));

        bool ready = (start_station_id != -1 && end_station_id != -1);
        u32 btn_col = ready ? GFX_COLOR_RGBA(80, 180, 80, 255) : GFX_COLOR_RGBA(150, 150, 150, 255);
        
        GFX_DrawRectSolid(62, 152, 0.7f, 196, 46, GFX_COLOR_RGBA(0,0,0,100)); 
        GFX_DrawRectSolid(60, 150, 0.8f, 200, 50, btn_col);
        GFX_DrawText(&btn_search_text, 160, 165, 0.9f, 0.65f, 0.65f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
    } 
    else if (current_state == STATE_SELECT_START || current_state == STATE_SELECT_END || 
             current_state == STATE_RESULTS || current_state == STATE_SELECT_DATE || current_state == STATE_SELECT_HOUR) {
        
        float base_y = 10.0f;
        int count = 0;
        
        if (current_state == STATE_RESULTS) count = global_polaczenia_list.count;
        else if (current_state == STATE_SELECT_DATE) count = 31;
        else if (current_state == STATE_SELECT_HOUR) count = 24;
        else count = current_search_list.count;
        
        for (int i = 0; i < count && i < MAX_LIST_ITEMS; i++) {
            float y = base_y + (i * 50.0f) - scroll_y;
            if (y < -50 || y > 240) continue;

            GFX_DrawRectSolid(12, y+2, 0.7f, 296, 41, GFX_COLOR_RGBA(0,0,0,100)); 
            
            u32 bg_col = list_buttons[i].pressed ? GFX_COLOR_RGBA(200,200,200,255) : GFX_COLOR_RGBA(255,255,255,240);
            GFX_DrawRectSolid(10, y, 0.8f, 300, 45, bg_col);
            
            float text_scale = (current_state == STATE_RESULTS) ? 0.42f : 0.55f;
            float text_y = y + ((current_state == STATE_RESULTS) ? 17.0f : 15.0f);

            GFX_DrawText(&list_texts[i], 15, text_y, 0.9f, text_scale, text_scale, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(48, 74, 130, 255));
        }
    }
    else if (current_state == STATE_LOADING || current_state == STATE_FETCHING_ID) {
        float cx = 160, cy = 120;
        for (int i = 0; i < 8; i++) {
            float a = spinner_angle - i*(M_PI/4);
            u8 alpha = 255 - i*30;
            GFX_DrawLine(
                cx + cos(a)*10, cy + sin(a)*10, GFX_COLOR_RGBA(255,255,255,alpha),
                cx + cos(a)*18, cy + sin(a)*18, GFX_COLOR_RGBA(255,255,255,alpha),
                3.0f, 0.8f
            );
        }
    }

    if (fade_alpha > 0)
        GFX_DrawRectSolid(0,0,1.0f,320,240, GFX_COLOR_RGBA(0,0,0,(u8)fade_alpha));
}

void scenePolaczeniaExit(void) {
    if (current_search_list.station) {
        free(current_search_list.station);
        current_search_list.station = NULL;
    }
    if (global_polaczenia_list.polaczenia) {
        free(global_polaczenia_list.polaczenia);
        global_polaczenia_list.polaczenia = NULL;
    }
    
    GFX_TextBufClear(static_buf);
    GFX_TextBufDelete(static_buf);
    
    GFX_TextBufClear(dyn_buf);
    GFX_TextBufDelete(dyn_buf);

    GFX_TextBufClear(header_buf);
    GFX_TextBufDelete(header_buf);
}

