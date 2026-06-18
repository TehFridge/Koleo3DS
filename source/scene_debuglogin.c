#include "cJSON.h"
#include "scene_debuglogin.h"
#include "scene_manager.h"
#include "main.h"
#include "sprites.h"
#include "utils.h"
#include "koleo_api.h"
#include "request.h"
#include "drawing.h"
#include "data.h"
#include "koleo_utils.h"

#define DEBUG_MENU_ITEMS 5   
#define MAX_PASSENGERS 16    

static char username[64];
static char password[64];

static bool auth_parsed = false;
static bool pasazer_parsed = false;
static bool user_parsed = false;
static bool stacje_parsed = false;
static bool stacje_saved = false;

static int debugMenuSelected = 0;

static int currentView = 0; 

static const float menuStartY = 80.0f;
static const float menuLineHeight = 18.0f;

static GFX_TEXTBUF DebugMenu;

static GFX_TEXT debugmenu_Text[60];  

static bool polaczenia_parsed = false;

static void RebuildDebugMenuStaticText(void) {
    GFX_TextParse(&debugmenu_Text[0], DebugMenu, "KOLEO3DS Debug Menu");
    GFX_TextParse(&debugmenu_Text[1], DebugMenu, "Pasazerowie");
    GFX_TextParse(&debugmenu_Text[2], DebugMenu, "Dane Uzytkownika");
    GFX_TextParse(&debugmenu_Text[3], DebugMenu, "Wszystkie Stacje (ZCache'uj)");
    GFX_TextParse(&debugmenu_Text[4], DebugMenu, "Filtruj Stacje");
    GFX_TextParse(&debugmenu_Text[5], DebugMenu, "Wyszukaj Połączenia");
    GFX_TextParse(&debugmenu_Text[10], DebugMenu, ">"); 

    for (int i = 0; i <= 5; i++)
        GFX_TextOptimize(&debugmenu_Text[i]);
    GFX_TextOptimize(&debugmenu_Text[10]);
}

void sceneDebugloginInit(void) {
    DebugMenu = GFX_TextBufNew(4096); 
    GFX_TextBufClear(DebugMenu);
    RebuildDebugMenuStaticText();

    if (loadAuth()) {
        auth_parsed = true; 
        return; 
    }

    static SwkbdState swkbd;
    bool result;

    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 1, 0);
    swkbdSetHintText(&swkbd, "Enter your username");
    result = swkbdInputText(&swkbd, username, sizeof(username));
    if (!result) return;

    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 1, 0);
    swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);
    swkbdSetHintText(&swkbd, "Enter your password");
    result = swkbdInputText(&swkbd, password, sizeof(password));
    if (!result) return;

    kontoLogin(username, password);
}

void sceneDebugloginUpdate(uint32_t kDown, uint32_t kHeld) {
    if (konto_login.done && !auth_parsed) {
        parse_AuthResponse(konto_login.data);
        auth_parsed = true;
        
    }
    
    if (pasazerowie.done && !pasazer_parsed) {
        global_lista = parse_Pasazerowie(pasazerowie.data);
        pasazer_parsed = true;
        currentView = 1;

        GFX_TextBufClear(DebugMenu);
        RebuildDebugMenuStaticText();

        for (int i = 0; i < global_lista.count && i < MAX_PASSENGERS; i++) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%d: %s %s (ID: %d)", i + 1, global_lista.ziomy[i].imie, global_lista.ziomy[i].nazwisko, global_lista.ziomy[i].pasazer_id);
            GFX_TextParse(&debugmenu_Text[11 + i], DebugMenu, buf);
            GFX_TextOptimize(&debugmenu_Text[11 + i]);
        }
    }
    
    if (user_dane.done && !user_parsed) {
        parse_UserDane(user_dane.data);
        user_parsed = true;
        currentView = 2;

        GFX_TextBufClear(DebugMenu);
        RebuildDebugMenuStaticText();

        char buf[128];
        snprintf(buf, sizeof(buf), "Imie/Nazwisko: %s %s", dane_uzytkownika.imie, dane_uzytkownika.nazwisko);
        GFX_TextParse(&debugmenu_Text[30], DebugMenu, buf); GFX_TextOptimize(&debugmenu_Text[30]);
        
        snprintf(buf, sizeof(buf), "Email: %s", dane_uzytkownika.email);
        GFX_TextParse(&debugmenu_Text[31], DebugMenu, buf); GFX_TextOptimize(&debugmenu_Text[31]);
        
        snprintf(buf, sizeof(buf), "Stan Konta: %.2f PLN", dane_uzytkownika.stan_konta);
        GFX_TextParse(&debugmenu_Text[32], DebugMenu, buf); GFX_TextOptimize(&debugmenu_Text[32]);
        
        snprintf(buf, sizeof(buf), "Urodziny: %s", dane_uzytkownika.urodziny);
        GFX_TextParse(&debugmenu_Text[33], DebugMenu, buf); GFX_TextOptimize(&debugmenu_Text[33]);
        
        snprintf(buf, sizeof(buf), "ID Znizki: %d", dane_uzytkownika.id_znizki);
        GFX_TextParse(&debugmenu_Text[34], DebugMenu, buf); GFX_TextOptimize(&debugmenu_Text[34]);
    }
    
    if (dawaj_stacje_for_me.done && !polaczenia_parsed) {
        global_polaczenia_list = parse_Polaczenia(dawaj_stacje_for_me.data);
        polaczenia_parsed = true;
        currentView = 4; 

        GFX_TextBufClear(DebugMenu);
        RebuildDebugMenuStaticText();

        for (int i = 0; i < global_polaczenia_list.count && i < 5; i++) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%d: %s (%d min)", i + 1, 
                     global_polaczenia_list.polaczenia[i].nazwa, 
                     global_polaczenia_list.polaczenia[i].dlugosc_trasy);
            GFX_TextParse(&debugmenu_Text[50 + i], DebugMenu, buf);
            GFX_TextOptimize(&debugmenu_Text[50 + i]);
        }
    }
    if (all_stacjas.done && !stacje_saved) {
        FILE *fp = fopen("/3ds/koleo3ds/all_stacjas.json", "w");
        if (fp) {
            fwrite(all_stacjas.data, 1, strlen(all_stacjas.data), fp);
            fclose(fp);
        }
        stacje_saved = true;
    }
    
    if (kDown & KEY_DOWN) {
        debugMenuSelected = (debugMenuSelected + 1) % DEBUG_MENU_ITEMS;
    }
    if (kDown & KEY_UP) {
        debugMenuSelected = (debugMenuSelected - 1 + DEBUG_MENU_ITEMS) % DEBUG_MENU_ITEMS;
    }

    if (kDown & KEY_A) {
        switch (debugMenuSelected) {
            case 0: 
                pasazer_parsed = false;
                pasazerowie.done = false;
                get_Pasazerowie();
                break;

            case 1: 
                user_parsed = false;
                user_dane.done = false;
                get_UserDane();
                break;

            case 2: 
                stacje_saved = false;
                all_stacjas.done = false;
                getAll_Stacje();
                break;

            case 3: 
                stacje_parsed = false;
                if ((fileExists("/3ds/koleo3ds/all_stacjas.json") && !stacje_parsed)) {
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
                
                            all_stacja_list = search_stations(jsonBuffer, "Warszawa");

                            free(jsonBuffer);
                        } else {
                            fclose(fp);
                            log_to_file("malloc failed while reading file\n");
                        }
                    } else {
                        
                        log_to_file("File not found: /3ds/koleo3ds/all_stacjas.json\n");
                    }
                    stacje_parsed = true;
                    currentView = 3;

                    GFX_TextBufClear(DebugMenu);
                    RebuildDebugMenuStaticText();

                    for (int i = 0; i < all_stacja_list.count && i < 5; i++) {
                        char buf[128];
                        snprintf(buf, sizeof(buf), "%d: %s (ID: %d)", i + 1, all_stacja_list.station[i].nazwa_slug, all_stacja_list.station[i].stacja_id);
                        GFX_TextParse(&debugmenu_Text[40 + i], DebugMenu, buf);
                        GFX_TextOptimize(&debugmenu_Text[40 + i]);
                    }
                    fclose(fp);
                }
                break;
            case 4: 
                polaczenia_parsed = false;
                dawaj_stacje_for_me.done = false;
                getStacje_for_Me(znajdz_stacja_id("Wrocław Główny"), znajdz_stacja_id("Warszawa Centralna"), get_current_datetime_iso(), false);
                break;
        }
    }
}

void sceneDebugloginRender(void) {
    GFX_BeginSceneTop(0, true);

    GFX_DrawShadowedText(
        &debugmenu_Text[0],
        200.0f, 20.0f, 1.0f, 1.0f, 1.0f, GFX_ALIGN_CENTER,
        GFX_COLOR_RGBA(0xB1, 0xA2, 0x2F, 0xff),
        GFX_COLOR_RGBA(0xff, 0xff, 0xff, 0xff)
    );

    for (int i = 0; i < DEBUG_MENU_ITEMS; i++) {
        float y = menuStartY + i * menuLineHeight;
        
        if (i == debugMenuSelected) {
            GFX_DrawText(
                &debugmenu_Text[10],
                15.0f, y, 0.5f, 0.5f, 0.5f, GFX_ALIGN_LEFT,
                GFX_COLOR_RGBA(255, 255, 255, 255)
            );
        }

        u32 color = (i == debugMenuSelected)
            ? GFX_COLOR_RGBA(255, 255, 0, 255)
            : GFX_COLOR_RGBA(200, 200, 200, 255);
            
        GFX_DrawText(
            &debugmenu_Text[i + 1],
            30.0f, y, 0.5f, 0.5f, 0.5f, GFX_ALIGN_LEFT,
            color
        );
    }

    float py = menuStartY + DEBUG_MENU_ITEMS * menuLineHeight + 20.0f;
    
    if (currentView == 1 && pasazer_parsed) {
        for (int i = 0; i < global_lista.count && i < MAX_PASSENGERS; i++) {
            GFX_DrawText(&debugmenu_Text[11 + i], 30.0f, py - 20.0f, 0.5f, 0.35f, 0.35f, GFX_ALIGN_LEFT,
                         GFX_COLOR_RGBA(255, 255, 255, 255));
            py += 8.0f;
        }
    } else if (currentView == 2 && user_parsed) {
        for (int i = 0; i < 5; i++) {
            GFX_DrawText(&debugmenu_Text[30 + i], 30.0f, py - 20.0f, 0.5f, 0.35f, 0.35f, GFX_ALIGN_LEFT,
                         GFX_COLOR_RGBA(255, 255, 255, 255));
            py += 8.0f;
        }
    } else if (currentView == 3 && stacje_parsed) {
        for (int i = 0; i < 5; i++) {
            GFX_DrawText(&debugmenu_Text[40 + i], 30.0f, py - 20.0f, 0.5f, 0.35f, 0.35f, GFX_ALIGN_LEFT,
                         GFX_COLOR_RGBA(255, 255, 255, 255));
            py += 8.0f;
        }
    } else if (currentView == 4 && polaczenia_parsed) {
        for (int i = 0; i < global_polaczenia_list.count && i < 5; i++) {
            GFX_DrawText(&debugmenu_Text[50 + i], 30.0f, py - 20.0f, 0.5f, 0.35f, 0.35f, GFX_ALIGN_LEFT,
                         GFX_COLOR_RGBA(255, 255, 255, 255));
            py += 8.0f;
        }
    } 
}

void sceneDebugloginExit(void) {
    if (currentView == 1 && pasazer_parsed) {
        free(global_lista.ziomy); 
    }
    if (polaczenia_parsed) {
        free(global_polaczenia_list.polaczenia); 
    }
    GFX_TextBufDelete(DebugMenu);
}

