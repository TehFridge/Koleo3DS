#include "scene_mainmenu.h"
#include "scene_manager.h"
#include "main.h"
#include "sprites.h"
#include "drawing.h"
#include "koleo_api.h"
#include "scene_init.h"
#include "audio_shit.h"
#include "utils.h"
#include "button.h" 
#include <dirent.h>  
#include <string.h>

extern char activeVoicePackSource[16];

static float fade_alpha = 255.0f;
static float spinner_angle = 0.0f;

static bool data_requested = false;
static bool data_parsed = false;
static float top_banner_y = -80.0f;

static bool passenger_requested = false;
static bool passenger_parsed = false;
static bool wagon_typy_saved = false;
static bool miejsce_saved = false;
static float bottom_panel_offset = 240.0f;

#define MAX_VOICE_PACKS 16
static bool in_options = false;

typedef enum {
    VP_ROMFS,
    VP_SD
} VoicePackSource;

char voice_packs[MAX_VOICE_PACKS][64];
VoicePackSource voice_pack_source[MAX_VOICE_PACKS];
int voice_pack_count = 0;
static char selected_vp[64] = "Default";

static Button vp_buttons[MAX_VOICE_PACKS];
static GFX_TEXT vp_texts[MAX_VOICE_PACKS];
static Button tutorial_button;
static GFX_TEXT tutorial_text;
static Button apply_button;
static GFX_TEXT apply_text;
static Button credits_button;
static GFX_TEXT credits_text;
static GFX_TEXT options_header_text;
static GFX_TEXT options_top_title;

static float scroll_y = 0.0f;
static float scroll_velocity = 0.0f;

static bool touching = false;
static float last_touch_x = 0.0f;
static float last_touch_y = 0.0f;
static bool touch_consumed = false;
static bool prevTouchHeld = false; 

static u16 cached_touch_x = 0;
static u16 cached_touch_y = 0;

static GFX_TEXTBUF text_buf;
static GFX_TEXT welcome_text;
static GFX_TEXT name_text;
static GFX_TEXT lr_text;
static GFX_TEXT op_text;
static char name_buffer[128];

#define MAX_PASSENGERS 64

static GFX_TEXT passenger_texts[MAX_PASSENGERS];
static GFX_TEXT passengers_header_text;
static Button passenger_buttons[MAX_PASSENGERS]; 

static char balance_buffer[64];
static GFX_TEXT balance_text;

static bool stacje_saved = false;
bool bgm_playing = false;

void scanVoicePacks(void) {
    voice_pack_count = 0;

    DIR* dir_romfs = opendir("romfs:/vp");
    DIR* dir_sd    = opendir("/3ds/Koleo3DS/VP");

    struct dirent* ent;

    if (dir_romfs) {
        strncpy(voice_packs[voice_pack_count], "Nikt", 63);
        voice_packs[voice_pack_count][63] = '\0';
        voice_pack_source[voice_pack_count] = VP_ROMFS;
        voice_pack_count++;
        while ((ent = readdir(dir_romfs)) != NULL) {
            if (ent->d_name[0] == '.' || strcmp(ent->d_name, "Default") == 0)
                continue;

            strncpy(voice_packs[voice_pack_count], ent->d_name, 63);
            voice_packs[voice_pack_count][63] = '\0';

            voice_pack_source[voice_pack_count] = VP_ROMFS;

            if (++voice_pack_count >= MAX_VOICE_PACKS)
                break;
        }

        closedir(dir_romfs);
    }

    if (dir_sd && voice_pack_count < MAX_VOICE_PACKS) {
        while ((ent = readdir(dir_sd)) != NULL) {
            if (ent->d_name[0] == '.' || strcmp(ent->d_name, "Default") == 0)
                continue;

            strncpy(voice_packs[voice_pack_count], ent->d_name, 63);
            voice_packs[voice_pack_count][63] = '\0';

            voice_pack_source[voice_pack_count] = VP_SD;

            if (++voice_pack_count >= MAX_VOICE_PACKS)
                break;
        }

        closedir(dir_sd);
    }
}

static GFX_TEXT nav_header_text;
static Button va_kun_button;
static GFX_TEXT va_kun_text;
static Button va_chan_button;
static GFX_TEXT va_chan_text;
static int selected_va = 1; 

void sceneMainmenuInit(void) {
    if (!bgm_playing) {
        playAudio(2, true, 0.25f);
        bgm_playing = true;
    }

    fade_alpha = 255.0f;
    spinner_angle = 0.0f;

    data_requested = false;
    data_parsed = false;
    top_banner_y = -80.0f;

    passenger_requested = false;
    passenger_parsed = false;
    bottom_panel_offset = 240.0f;

    scroll_y = 0.0f;
    scroll_velocity = 0.0f;
    touching = false;
    touch_consumed = false;
    prevTouchHeld = false;
    in_options = false;

    for (int i = 0; i < MAX_PASSENGERS; i++) {
        Create_button(&passenger_buttons[i], 0, 0, 320, 60, NULL, NULL,
            GFX_COLOR_RGBA(255,255,255,240),
            GFX_COLOR_RGBA(200,200,200,255), 0.8f);
    }

    text_buf = GFX_TextBufNew(1024);

    scanVoicePacks();
    strncpy(selected_vp, activeVoicePack, 64);

    for (int i = 0; i < voice_pack_count; i++) {
        Create_button(&vp_buttons[i], 10, 0, 300, 40, NULL, NULL,
            GFX_COLOR_RGBA(255,255,255,240), GFX_COLOR_RGBA(200,200,200,255), 0.75f);
        GFX_TextParse(&vp_texts[i], text_buf, voice_packs[i]);
        GFX_TextOptimize(&vp_texts[i]);
    }

    Create_button(&tutorial_button, 10, 195, 85, 35, NULL, NULL,
        GFX_COLOR_RGBA(48,74,130,255), GFX_COLOR_RGBA(30,50,90,255), 0.9f);
    GFX_TextParse(&tutorial_text, text_buf, "Tutorial");
    GFX_TextOptimize(&tutorial_text);

    Create_button(&apply_button, 115, 195, 85, 35, NULL, NULL,
        GFX_COLOR_RGBA(80,180,80,255), GFX_COLOR_RGBA(50,120,50,255), 0.9f);
    GFX_TextParse(&apply_text, text_buf, "Zastosuj");
    GFX_TextOptimize(&apply_text);

    Create_button(&credits_button, 220, 195, 85, 35, NULL, NULL,
        GFX_COLOR_RGBA(180,80,80,255), GFX_COLOR_RGBA(150,20,50,255), 0.9f);
    GFX_TextParse(&credits_text, text_buf, "Credits");
    GFX_TextOptimize(&credits_text);

    GFX_TextParse(&options_header_text, text_buf, "Ustawienia - Voice Packi");
    GFX_TextOptimize(&options_header_text);

    GFX_TextParse(&options_top_title, text_buf, "Ustawienia Aplikacji");
    GFX_TextOptimize(&options_top_title);

    selected_va = VOICEACT; 

    GFX_TextParse(&nav_header_text, text_buf, "Nawigator:");
    GFX_TextOptimize(&nav_header_text);

    Create_button(&va_kun_button, 10, 25, 140, 35, NULL, NULL,
        GFX_COLOR_RGBA(255,255,255,240), GFX_COLOR_RGBA(200,200,200,255), 0.8f);
    GFX_TextParse(&va_kun_text, text_buf, "Koleo-kun");
    GFX_TextOptimize(&va_kun_text);

    Create_button(&va_chan_button, 170, 25, 140, 35, NULL, NULL,
        GFX_COLOR_RGBA(255,255,255,240), GFX_COLOR_RGBA(200,200,200,255), 0.8f);
    GFX_TextParse(&va_chan_text, text_buf, "Koleo-chan");
    GFX_TextOptimize(&va_chan_text);
    
    get_UserDane();
    data_requested = true;

    get_Pasazerowie();
    passenger_requested = true;
    GFX_TextParse(&passengers_header_text, text_buf, "Pasażerowie");
    GFX_TextParse(&lr_text, text_buf, "L - Bilety | R - Połączenia");
    GFX_TextOptimize(&passengers_header_text);
    GFX_TextOptimize(&lr_text);
    GFX_TextParse(&op_text, text_buf, "(SELECT) - Opcje");
    GFX_TextOptimize(&op_text);
    getAll_Stacje();
    getAll_WagonTypy(); 
    getAll_TypyMiejsc();
    playNavAudio(4, true);
}

void sceneMainmenuUpdate(uint32_t kDown, uint32_t kHeld) {
    
    if (fade_alpha > 0.0f) {
        fade_alpha -= 5.0f;
        if (fade_alpha < 0.0f) fade_alpha = 0.0f;
    }
    if (!data_parsed || !passenger_parsed || !stacje_saved || !wagon_typy_saved || !miejsce_saved) {
        spinner_angle += 0.15f;
    }

    if (in_options) {
        if (kDown & KEY_SELECT) {
            
            if (strcmp(selected_vp, activeVoicePack) == 0 && selected_va == VOICEACT) {
                in_options = false;
                scroll_y = 0;
                scroll_velocity = 0;
            }
            return;
        }

        if (kDown & KEY_B) {
            
            if (strcmp(selected_vp, activeVoicePack) == 0 && selected_va == VOICEACT) {
                in_options = false;
                scroll_y = 0;
                scroll_velocity = 0;
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
        
        if (kHeld & KEY_UP)   scroll_y -= 8.0f;
        if (kHeld & KEY_DOWN) scroll_y += 8.0f;

        if (!touching) {
            scroll_y += scroll_velocity;
            scroll_velocity *= 0.92f;
            if (fabs(scroll_velocity) < 0.05f) scroll_velocity = 0.0f;
        }

        Update_button(&va_kun_button, touch, touchDown, touchHeld, touchUp);
        if (va_kun_button.clicked) selected_va = 1;

        Update_button(&va_chan_button, touch, touchDown, touchHeld, touchUp);
        if (va_chan_button.clicked) selected_va = 2;
        
        float item_height = 45.0f;
        float content_height = voice_pack_count * item_height;
        float max_scroll = content_height - 85.0f; 

        if (max_scroll < 0) max_scroll = 0;
        if (scroll_y < 0) { scroll_y = 0; scroll_velocity = 0; }
        if (scroll_y > max_scroll) { scroll_y = max_scroll; scroll_velocity = 0; }

        float base_y = 100.0f; 
        for (int i = 0; i < voice_pack_count; i++) {
            float y = base_y + (i * item_height) - scroll_y;
            vp_buttons[i].y = y;

            if (y < 95 || y > 185) {
                touchPosition dummy = {999, 999};
                Update_button(&vp_buttons[i], dummy, false, false, false);
            } else {
                Update_button(&vp_buttons[i], touch, touchDown, touchHeld, touchUp);
            }

            if (vp_buttons[i].clicked) {
                strncpy(selected_vp, voice_packs[i], 64);
            }
        }

        Update_button(&tutorial_button, touch, touchDown, touchHeld, touchUp);
        if (tutorial_button.clicked && (strcmp(selected_vp, activeVoicePack) == 0)) {
            sceneManagerSwitchTo(SCENE_TUTORIAL);
            return;
        }

        Update_button(&apply_button, touch, touchDown, touchHeld, touchUp);
        if (apply_button.clicked) {
            int selected_vp_index = 0;
            for (int i = 0; i < voice_pack_count; i++) {
                if (strcmp(voice_packs[i], selected_vp) == 0) {
                    selected_vp_index = i;
                    break;
                }
            }

            strncpy(activeVoicePack, selected_vp, 64);
            strcpy(activeVoicePackSource, voice_pack_source[selected_vp_index] == VP_SD ? "sd" : "romfs");
            VOICEACT = selected_va; 
            
            loadNavVoiceLines(activeVoicePack); 

            json_t *jsonfl = NULL;
            
            if (access("/3ds/Koleo3DS/opcje.json", F_OK) == 0) {
                jsonfl = json_load_file("/3ds/Koleo3DS/opcje.json", 0, NULL);
            }
            
            if (!jsonfl) {
                jsonfl = json_object();
            }

            if (jsonfl) {
                json_object_set_new(jsonfl, "voicepack", json_string(activeVoicePack));
                json_object_set_new(jsonfl, "VA", json_integer(VOICEACT));
                json_object_set_new(jsonfl, "voicepack_source", json_string(activeVoicePackSource));
                
                json_dump_file(jsonfl, "/3ds/Koleo3DS/opcje.json", JSON_INDENT(4));
                json_decref(jsonfl);
            }
            unloadAllVoicePackAudio();
            unloadAllNavAudio(); 
            freeAllAudios();
            sceneManagerSwitchTo(SCENE_INIT); 
            
        }

        Update_button(&credits_button, touch, touchDown, touchHeld, touchUp);
        if (credits_button.clicked && (strcmp(selected_vp, activeVoicePack) == 0)) {
            bgm_playing = false;
            stopAudio(2);
            sceneManagerSwitchTo(SCENE_CREDITS);
            return;
        }
        return; 
    }

    if (kDown & KEY_SELECT) {
        in_options = true;
        playNavAudio(6, true);
        scroll_y = 0;
        scroll_velocity = 0;
        strncpy(selected_vp, activeVoicePack, 64);
        selected_va = VOICEACT; 
        return;
    }

    if (kDown & KEY_R) {
        sceneManagerSwitchTo(SCENE_POLACZENIA);
        return;
    }
    if (kDown & KEY_L) {
        playNavAudio(0, true);
        sceneManagerSwitchTo(SCENE_BILETY);
        return;
    }

    if (all_stacjas.done && all_stacjas.response_code == 304) {
        stacje_saved = true; 
    }
    if (typy_wagon.done && typy_wagon.response_code == 304) {
        if (!wagon_typy_saved) {
            load_seat_types();
        }
        wagon_typy_saved = true; 
    }
    if (typy_miejsc.done && typy_miejsc.response_code == 304) {
        miejsce_saved = true; 
    }   
    if (all_stacjas.done && !stacje_saved && all_stacjas.response_code == 200) {
        FILE *fp = fopen("/3ds/koleo3ds/all_stacjas.json", "w");
        if (fp) {
            fwrite(all_stacjas.data, 1, strlen(all_stacjas.data), fp);
            fclose(fp);
        }
        stacje_saved = true;
        free(all_stacjas.data);
    }
    if (typy_wagon.done && !wagon_typy_saved && typy_wagon.response_code == 200) {
        FILE *fp = fopen("/3ds/koleo3ds/typy_wagon.json", "w");
        if (fp) {
            fwrite(typy_wagon.data, 1, strlen(typy_wagon.data), fp);
            fclose(fp);
        }
        wagon_typy_saved = true;
        free(typy_wagon.data); 
        typy_wagon.data = NULL;
    }
    if (typy_miejsc.done && !miejsce_saved && typy_miejsc.response_code == 200) {
        FILE *fp = fopen("/3ds/koleo3ds/typy_miejsc.json", "w");
        if (fp) {
            fwrite(typy_miejsc.data, 1, strlen(typy_miejsc.data), fp);
            fclose(fp);
        }
        miejsce_saved = true;
        free(typy_miejsc.data); 
        typy_miejsc.data = NULL;
        load_seat_types();
    }
    
    if (data_requested && user_dane.done && !data_parsed) {
        if (user_dane.data && user_dane.size > 0) {
            if (user_dane.data != NULL) {
                parse_UserDane(user_dane.data);
            }
            data_parsed = true;

            snprintf(name_buffer, sizeof(name_buffer), "%s %s!",
                dane_uzytkownika.imie, dane_uzytkownika.nazwisko);

            GFX_TextParse(&welcome_text, text_buf, "Witaj z powrotem,");
            GFX_TextParse(&name_text, text_buf, name_buffer);
            
            GFX_TextOptimize(&welcome_text);
            GFX_TextOptimize(&name_text);
            snprintf(balance_buffer, sizeof(balance_buffer),
                "Saldo: %.2f zł", dane_uzytkownika.stan_konta);

            GFX_TextParse(&balance_text, text_buf, balance_buffer);
            GFX_TextOptimize(&balance_text);
        }
    }

    if (passenger_requested && pasazerowie.done && !passenger_parsed) {

        if (pasazerowie.data && pasazerowie.size > 0) {

            global_lista = parse_Pasazerowie(pasazerowie.data);

            passenger_parsed = true;

            for (int i = 0; i < global_lista.count && i < MAX_PASSENGERS; i++) {
                char temp[64];
                snprintf(temp, sizeof(temp), "%s %s",
                    global_lista.ziomy[i].imie,
                    global_lista.ziomy[i].nazwisko);

                GFX_TextParse(&passenger_texts[i], text_buf, temp);
                
                GFX_TextOptimize(&passenger_texts[i]);
            }

            log_to_file("[mainmenu] passengers parsed: %d", global_lista.count);
        }
    }
    
    if ((data_parsed && stacje_saved && wagon_typy_saved && miejsce_saved) && top_banner_y < 0.0f) {
        top_banner_y += (0.0f - top_banner_y) * 0.1f;
        if (top_banner_y > -0.5f) top_banner_y = 0.0f;
    }

    if ((passenger_parsed && stacje_saved && wagon_typy_saved && data_parsed && miejsce_saved) && bottom_panel_offset > 0.0f) {
        bottom_panel_offset += (0.0f - bottom_panel_offset) * 0.1f;
        if (bottom_panel_offset < 0.5f) bottom_panel_offset = 0.0f;
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

    if (!touching) {
        scroll_y += scroll_velocity;
        scroll_velocity *= 0.92f;
        if (fabs(scroll_velocity) < 0.05f) {
            scroll_velocity = 0.0f;
        }
    }

    float item_height = 60.0f;
    float content_height = global_lista.count * item_height;
    float max_scroll = content_height - 205.0f;

    if (max_scroll < 0) max_scroll = 0;

    if (scroll_y < 0) {
        scroll_y = 0;
        scroll_velocity = 0;
    }
    if (scroll_y > max_scroll) {
        scroll_y = max_scroll;
        scroll_velocity = 0;
    }

    float base_y = 35 + bottom_panel_offset;
    for (int i = 0; i < global_lista.count && i < MAX_PASSENGERS; i++) {
        float y = base_y + (i * 60.0f) - scroll_y;
        
        passenger_buttons[i].y = y; 
        Update_button(&passenger_buttons[i], touch, touchDown, touchHeld, touchUp);

        if (passenger_buttons[i].clicked) {
            global_lista.ziomy[i].selected = !global_lista.ziomy[i].selected;

            if (global_lista.ziomy[i].selected) {
                select_Pasazer(global_lista.ziomy[i].pasazer_id);
            } else {
                deselect_Pasazer(global_lista.ziomy[i].pasazer_id);
            }
        }
    }
}

void sceneMainmenuRender(void) {
    GFX_BeginSceneTop(0, true);
    GFX_DrawRectSolid(0, 0, 0.1f, 400, 240, GFX_COLOR_RGBA(48,74,130,255));
    narysujpociagi(40);

    if (in_options) {
        if (selected_va == 1) {
            GFX_DrawImageAt(kun_idle, 10, 20, 0.6f, NULL, 1.0f, 1.0f);
        } else {
            GFX_DrawImageAt(chan_idle, 10, 20, 0.6f, NULL, 1.0f, 1.0f);
        }
        GFX_DrawRectSolid(0, 0, 0.8f, 400, 60, GFX_COLOR_RGBA(255,255,255,190));
        GFX_DrawRectSolid(0, 60, 0.7f, 400, 4, GFX_COLOR_RGBA(0,0,0,80));
        GFX_DrawText(&options_top_title, 15, 18, 0.9f, 0.8f, 0.8f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(48,74,130,255));
    } else {
        if (!data_parsed || !stacje_saved || !wagon_typy_saved || !passenger_parsed || !miejsce_saved) {
            float cx = 200, cy = 120;
            for (int i = 0; i < 8; i++) {
                float a = spinner_angle - i*(M_PI/4);
                u8 alpha = 255 - i*30;

                GFX_DrawLine(cx + cos(a)*15, cy + sin(a)*15,GFX_COLOR_RGBA(255,255,255,alpha),cx + cos(a)*25, cy + sin(a)*25,GFX_COLOR_RGBA(255,255,255,alpha),4.0f, 0.8f);
            }
        } else {
            float y = top_banner_y;
            if (VOICEACT == 1) {
                GFX_DrawImageAt(kun_idle, 10, (-y * 4.0) + 20, 0.6f, NULL, 1.0f, 1.0f);
            } else {
                GFX_DrawImageAt(chan_idle, 10, (-y * 4.0) + 20, 0.6f, NULL, 1.0f, 1.0f);
            }
            GFX_DrawRectSolid(0, y+60, 0.7f, 400, 4, GFX_COLOR_RGBA(0,0,0,80));
            GFX_DrawRectSolid(0, y, 0.8f, 400, 60, GFX_COLOR_RGBA(255,255,255,190));

            GFX_DrawText(&welcome_text, 15, y+10, 0.9f, 0.6f, 0.6f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(100,100,100,220));
            GFX_DrawText(&name_text, 15, y+28, 0.9f, 0.8f, 0.8f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(48,74,130,255));
            GFX_DrawText(&balance_text, 380, y + 10, 0.9f, 0.6f, 0.6f, GFX_ALIGN_RIGHT, GFX_COLOR_RGBA(80, 120, 80, 255));
            GFX_DrawShadowedText(&op_text, 200, 3*-y+190, 0.9f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,255));
            GFX_DrawShadowedText(&lr_text, 200, 3*-y+210, 0.9f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,255));
        }
    }

    if (fade_alpha > 0)
        GFX_DrawRectSolid(0,0,1.0f,400,240,
            GFX_COLOR_RGBA(0,0,0,(u8)fade_alpha));
    
    GFX_BeginSceneBottom();
    GFX_DrawRectSolid(0, 0, 0.1f, 320, 240, GFX_COLOR_RGBA(48,74,130,255));
    narysujpociagi(40);
    GFX_DrawRectSolid(0, 0, 0.6f, 320, 240, GFX_COLOR_RGBA(0,0,0,120));

    if (in_options) {
        
        GFX_DrawRectSolid(0, 0, 0.8f, 320, 70, GFX_COLOR_RGBA(0,0,0,160));
        GFX_DrawText(&nav_header_text, 10, 5, 1.0f, 0.7f, 0.7f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255,255,255,255));

        va_kun_button.color = (selected_va == 1) ? GFX_COLOR_RGBA(200, 230, 255, 255) : GFX_COLOR_RGBA(255, 255, 255, 240);
        Draw_button(&va_kun_button);
        GFX_DrawText(&va_kun_text, va_kun_button.x + 70, va_kun_button.y + 10, 1.0f, 0.65f, 0.65f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(48, 74, 130, 255));

        va_chan_button.color = (selected_va == 2) ? GFX_COLOR_RGBA(200, 230, 255, 255) : GFX_COLOR_RGBA(255, 255, 255, 240);
        Draw_button(&va_chan_button);
        GFX_DrawText(&va_chan_text, va_chan_button.x + 70, va_chan_button.y + 10, 1.0f, 0.65f, 0.65f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(48, 74, 130, 255));

        GFX_DrawRectSolid(0, 70, 1.0f, 320, 25, GFX_COLOR_RGBA(0,0,0,130));
        GFX_DrawText(&options_header_text, 10, 75, 1.0f, 0.7f, 0.7f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255,255,255,255));

        float base_y = 100.0f;
        for (int i = 0; i < voice_pack_count; i++) {
            float y = base_y + (i * 45.0f) - scroll_y;
            if (y < 60 || y > 185) continue; 

            bool is_selected = (strcmp(voice_packs[i], selected_vp) == 0);
            bool is_active = (strcmp(voice_packs[i], activeVoicePack) == 0);

            if (is_selected) {
                vp_buttons[i].color = GFX_COLOR_RGBA(200, 230, 255, 255);
            } else {
                vp_buttons[i].color = GFX_COLOR_RGBA(255, 255, 255, 240);
            }

            vp_buttons[i].y = y;
            Draw_button(&vp_buttons[i]);

            if (is_active) {
                GFX_DrawRectSolid(290, y + 15, 0.9f, 10, 10, GFX_COLOR_RGBA(80, 200, 80, 255));
            }

            GFX_DrawText(&vp_texts[i], 20, y + 10, 0.9f, 0.65f, 0.65f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(48, 74, 130, 255));
        }

        GFX_DrawRectSolid(0, 185, 0.9f, 320, 55, GFX_COLOR_RGBA(0,0,0,180));

        Draw_button(&tutorial_button);
        GFX_DrawText(&tutorial_text, tutorial_button.x + 40, tutorial_button.y + 10, 1.0f, 0.45f, 0.45f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255));

        Draw_button(&apply_button);
        GFX_DrawText(&apply_text, apply_button.x + 40, apply_button.y + 10, 1.0f, 0.45f, 0.45f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255));

        Draw_button(&credits_button);
        GFX_DrawText(&credits_text, credits_button.x + 40, credits_button.y + 10, 1.0f, 0.45f, 0.45f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255));

    } else {
        if (!passenger_parsed || !stacje_saved || !wagon_typy_saved || !data_parsed || !miejsce_saved) {
            float cx = 160, cy = 120;
            for (int i = 0; i < 8; i++) {
                float a = spinner_angle - i*(M_PI/4);
                u8 alpha = 255 - i*30;

                GFX_DrawLine(
                    cx + cos(a)*10, cy + sin(a)*10,
                    GFX_COLOR_RGBA(255,255,255,alpha),
                    cx + cos(a)*18, cy + sin(a)*18,
                    GFX_COLOR_RGBA(255,255,255,alpha),
                    3.0f, 0.8f
                );
            }
        } else {
            float base_y = 35 + bottom_panel_offset;
            
            GFX_DrawRectSolid(0, 0 + bottom_panel_offset, 1.0f,
                320, 30, GFX_COLOR_RGBA(0,0,0,150));
                
            GFX_DrawText(
                &passengers_header_text,
                10, 5 + bottom_panel_offset,
                1.0f,
                0.7f, 0.7f,
                GFX_ALIGN_LEFT,
                GFX_COLOR_RGBA(255,255,255,255)
            );
            
            for (int i = 0; i < global_lista.count; i++) {

                float y = base_y + (i * 60.0f) - scroll_y;

                if (y < -100 || y > 240) continue;

                bool selected = global_lista.ziomy[i].selected;

                GFX_DrawRectSolid(5, y+5, 0.7f,
                    310, 60, GFX_COLOR_RGBA(0,0,0,100));

                if (selected) {
                    passenger_buttons[i].color = GFX_COLOR_RGBA(200, 230, 255, 255);
                } else {
                    passenger_buttons[i].color = GFX_COLOR_RGBA(255, 255, 255, 240);
                }

                passenger_buttons[i].y = y;
                Draw_button(&passenger_buttons[i]);

                GFX_DrawRectSolid(280, y+20, 0.9f,
                    20, 20,
                    selected ? GFX_COLOR_RGBA(48,74,130,255)
                            : GFX_COLOR_RGBA(200,200,200,255));

                GFX_DrawText(
                    &passenger_texts[i],
                    15, y + 20,
                    0.9f,
                    0.65f, 0.65f,
                    GFX_ALIGN_LEFT,
                    GFX_COLOR_RGBA(48, 74, 130, 255)
                );
            }
        }
    }

    if (fade_alpha > 0)
        GFX_DrawRectSolid(0,0,1.0f,320,240,
            GFX_COLOR_RGBA(0,0,0,(u8)fade_alpha));
}

void sceneMainmenuExit(void) {
    if (text_buf) {
        GFX_TextBufClear(text_buf);
        GFX_TextBufDelete(text_buf);
        text_buf = NULL;
    }
}

