#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include "scene_login.h"
#include "scene_manager.h"
#include "drawing.h"
#include "koleo_api.h"
#include "audio_shit.h"

static char username[64];
static char password[64];

static SwkbdState swkbd;
static bool result;


static int login_state = 0;
static float fade_alpha = 255.0f;
static float text_y = 260.0f; 
static int wait_frames = 0;

static GFX_TEXTBUF textBuf;
static GFX_TEXT infoText;
static char current_msg[64] = "";

void sceneLoginInit(void) {
    login_state = 0;
    fade_alpha = 255.0f;
    text_y = 260.0f;
    wait_frames = 0;
    
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));

    
    textBuf = GFX_TextBufNew(1024);
    GFX_TextParse(&infoText, textBuf, ""); 
    playAudio(1, true, 0.25f);
}

void sceneLoginUpdate(uint32_t kDown, uint32_t kHeld) {
    switch (login_state) {
        case 0: 
            fade_alpha -= 5.0f;
            if (fade_alpha <= 0.0f) {
                fade_alpha = 0.0f;
                
                
                strcpy(current_msg, "Wprowadz adres E-Mail");
                GFX_TextBufClear(textBuf);
                GFX_TextParse(&infoText, textBuf, current_msg);
                GFX_TextOptimize(&infoText);
                
                login_state = 1; 
            }
            break;
            
        case 1: 
            text_y += (110.0f - text_y) * 0.1f; 
            if ((text_y - 110.0f) < 1.0f) { 
                text_y = 110.0f;
                wait_frames = 15; 
                playNavAudio(2, true);
                login_state = 2;
            }
            break;

        case 2: 
            wait_frames--;
            if (wait_frames <= 0) {
                login_state = 3;
            }
            break;

        case 3: 
            swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 1, -1);
            swkbdSetHintText(&swkbd, "Wprowadź adres E-Mail");
            result = swkbdInputText(&swkbd, username, sizeof(username));
            
            if (!result) {
                
            }

            
            strcpy(current_msg, "Wprowadź hasło");
            GFX_TextBufClear(textBuf);
            GFX_TextParse(&infoText, textBuf, current_msg);
            GFX_TextOptimize(&infoText);
            
            text_y = 260.0f; 
            login_state = 4; 
            break;

        case 4: 
            text_y += (110.0f - text_y) * 0.1f;
            if ((text_y - 110.0f) < 1.0f) {
                text_y = 110.0f;
                wait_frames = 60; 
                playNavAudio(3, true);
                login_state = 5;
            }
            break;

        case 5: 
            wait_frames--;
            if (wait_frames <= 0) {
                login_state = 6;
            }
            break;

        case 6: 
            swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 1, -1);
            swkbdSetHintText(&swkbd, "Wprowadz haslo");
            swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);
            result = swkbdInputText(&swkbd, password, sizeof(password));

            if (!result) {
               
            }

            login_state = 7; 
            break;

        case 7: 
            kontoLogin(username, password);
            login_state = 8; 
            break;

        case 8: 
            if (konto_login.done) {
                parse_AuthResponse(konto_login.data); 
                sceneManagerSwitchTo(SCENE_TUTORIAL);
            }
            break;
    }
}

void sceneLoginRender(void) {
    
    GFX_BeginSceneTop(0, true); 
    GFX_DrawRectSolid(0, 0, 0.1f, 400, 240, GFX_COLOR_RGBA(48, 74, 130, 255)); 
    narysujpociagi(40);
    
    
    if (login_state >= 1 && login_state <= 6) {
        GFX_DrawShadowedText(&infoText, 200.0f, text_y, 0.5f, 0.8f, 0.8f, 
                             GFX_ALIGN_CENTER, 
                             GFX_COLOR_RGBA(255, 255, 255, 255), 
                             GFX_COLOR_RGBA(0, 0, 0, 128));
    }

    
    if (fade_alpha > 0.0f) {
        GFX_DrawRectSolid(0, 0, 1.0f, 400, 240, GFX_COLOR_RGBA(0, 0, 0, (u8)fade_alpha));
    }

    
    
    GFX_BeginSceneBottom(); 
    GFX_DrawRectSolid(0, 0, 0.1f, 320, 240, GFX_COLOR_RGBA(48, 74, 130, 255)); 
    narysujpociagi(40);    
    GFX_DrawRectSolid(0, 0, 0.6f, 320, 240, GFX_COLOR_RGBA(0, 0, 0, 120));
    
    if (fade_alpha > 0.0f) {
        GFX_DrawRectSolid(0, 0, 1.0f, 320, 240, GFX_COLOR_RGBA(0, 0, 0, (u8)fade_alpha));
    }
}

void sceneLoginExit(void) {
    GFX_TextBufDelete(textBuf);
    stopAudio(1);
}