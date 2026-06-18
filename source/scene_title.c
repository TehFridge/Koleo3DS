#include "scene_title.h"
#include "drawing.h"
#include "data.h"
#include "sprites.h"
#include "audio_shit.h"
#include "koleo_api.h"
#include "koleo_utils.h"
#include "scene_tutorial.h"
#include "scene_mainmenu.h"
#include "logs.h"

static u64 startTime = 0;

#define TIME_SCALE 0.75f

static bool exiting = false;
static u64 exitStartTime = 0;

static float easeOutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    float p = 0.3f;
    return powf(2.0f, -10.0f * t) * sinf((t - p / 4.0f) * (2.0f * M_PI) / p) + 1.0f;
}

static void drawCenteredScaledTint(GFX_IMAGE* img, float cx, float cy, float scale, float depth, C2D_ImageTint* tint) {
    if (!img) return;
    float draw_x = cx - (img->width * scale) / 2.0f;
    float draw_y = cy - (img->height * scale) / 2.0f;
    GFX_DrawImageAt(img, draw_x, draw_y, depth, tint, scale, scale);
}

GFX_TEXTBUF buf;
GFX_TEXT text;

void sceneTitleInit(void) {
    startTime = osGetTime();
    playAudio(0, true, 0.4f);
    buf = GFX_TextBufNew(128);
    GFX_TextParse(&text, buf, "Wciśnij A");
    GFX_TextOptimize(&text);
    Tutorial_audio_loaded = false;

    exiting = false;
}

void sceneTitleUpdate(uint32_t kDown, uint32_t kHeld) {
    if ((kDown & KEY_A) && !exiting) {
        exiting = true;
        exitStartTime = osGetTime();
    }
    if ((kDown & KEY_SELECT) && !logplz) {
        playAudio(5, true, 1.0f);
        logplz = true;
    }
}

void sceneTitleRender(void) {
    u64 currentTime = osGetTime();

    float elapsedMs = (float)(currentTime - startTime) * TIME_SCALE;

    float fadeStart = 1600.0f; 
    float fadeDuration = 250.0f;
    float fadeAlpha = (elapsedMs < fadeStart) ? 1.0f : 1.0f - ((elapsedMs - fadeStart) / fadeDuration);
    if (fadeAlpha < 0.0f) fadeAlpha = 0.0f;
    
    C2D_ImageTint logoTint;
    C2D_AlphaImageTint(&logoTint, fadeAlpha);

    GFX_BeginSceneTop(0, true); 
    
    GFX_DrawRectSolid(0, 0, 0.1f, 400, 240, GFX_COLOR_RGBA(48, 74, 130, 255)); 
    narysujpociagi(40);

    if (fadeAlpha > 0) {
        GFX_DrawRectSolid(0, 0, 0.5f, 400, 240, GFX_COLOR_RGBA(0, 0, 0, (u8)(fadeAlpha * 255))); 
        
        if (elapsedMs >= 30.0f) {
            float t1 = (elapsedMs - 130.0f) / 1000.0f;
            float scale1 = easeOutElastic(fminf(t1 * 1.2f, 1.0f));
            drawCenteredScaledTint(logo_1, 200.0f, 120.0f, scale1, 0.5f, &logoTint);
        }

        if (elapsedMs >= 70.0f) {
            float t2 = (elapsedMs - 340.0f) / 1000.0f;
            float scale2 = easeOutElastic(fminf(t2 * 1.2f, 1.0f));
            drawCenteredScaledTint(logo_2, 200.0f, 120.0f, scale2, 0.51f, &logoTint);
        }
    }

    if (elapsedMs >= 1700.0f) {
        float t = (elapsedMs - 1740.0f) / 1000.0f;
        float duration = 0.85f; 
        
        if (t < duration) {
            float trainScale = (p_poczatek) ? (240.0f / p_poczatek->height) : 1.0f;

            float startX = -8000.0f; 
            float endX = 6000.0f;
            float baseCurrentX = startX + (t / duration) * (endX - startX);

            float alphaBoost = fminf(t * 3.0f, 1.0f);

            for (int j = 0; j < 2; j++) {
                float blurOffset = (float)j * -30.0f; 
                float currentX = baseCurrentX + blurOffset;

                C2D_ImageTint blurTint;
                float layerAlpha = (j == 0) ? alphaBoost : alphaBoost * 0.6f; 
                C2D_AlphaImageTint(&blurTint, layerAlpha);

                if (p_koniec) {
                    GFX_DrawImageAt(p_koniec, currentX, 0, 0.6f, &blurTint, trainScale, trainScale);
                    currentX += (p_koniec->width * trainScale);
                }

                for(int i = 0; i < 20; i++) {
                    if (p_srodek) {
                        GFX_DrawImageAt(p_srodek, currentX, 0, 0.6f, &blurTint, trainScale, trainScale);
                        currentX += (p_srodek->width * trainScale);
                    }
                }

                if (p_poczatek) {
                    GFX_DrawImageAt(p_poczatek, currentX, 0, 0.6f, &blurTint, trainScale, trainScale);
                }
            }
        }
    }

    if (elapsedMs >= 2760.0f && !exiting) {
        float t = (elapsedMs - 2760.0f) / 1000.0f;
        float scale = easeOutElastic(fminf(t * 1.5f, 1.0f));
        drawCenteredScaledTint(koleo_logo, 200.0f, 120.0f, scale, 0.8f, NULL);
    }

    if (exiting) {
        float t = (float)(osGetTime() - exitStartTime) / 600.0f;
        if (t > 1.0f) t = 1.0f;

        float eased = 1.0f - powf(1.0f - t, 3.0f);
        float zoom = 1.0f + eased * 6.5f;

        drawCenteredScaledTint(koleo_logo, 200.0f, 120.0f, zoom, 0.95f, NULL);

        if (t >= 1.0f) {
            if (!loadAuth()) {
                sceneManagerSwitchTo(SCENE_LOGIN);
            } else {
                if (osGetWifiStrength() > 0) {
                    bgm_playing = false;
                    sceneManagerSwitchTo(SCENE_MAINMENU);
                } else {
                    sceneManagerSwitchTo(SCENE_BILETY);
                }
            }
        }
    }
    if (exiting) {
        float t = (float)(osGetTime() - exitStartTime) / 600.0f;
        if (t > 1.0f) t = 1.0f;

        GFX_DrawRectSolid(0, 0, 1.0f, 400, 240,
            GFX_COLOR_RGBA(0, 0, 0, (u8)(t * 255)));
    }
    
    GFX_BeginSceneBottom(); 
    GFX_DrawRectSolid(0, 0, 0.5f, 320, 240, GFX_COLOR_RGBA(48, 74, 130, 255));
    narysujpociagi(40);
    GFX_DrawRectSolid(0, 0, 0.6f, 320, 240, GFX_COLOR_RGBA(0, 0, 0, 120)); 
    GFX_DrawRectSolid(0, 0, 0.8f, 400, 240, GFX_COLOR_RGBA(0, 0, 0, (u8)(fadeAlpha * 255))); 
    if (exiting) {
        float t = (float)(osGetTime() - exitStartTime) / 600.0f;
        if (t > 1.0f) t = 1.0f;

        GFX_DrawRectSolid(0, 0, 1.0f, 320, 240,
            GFX_COLOR_RGBA(0, 0, 0, (u8)(t * 255)));
    }
    float blink = fmodf(elapsedMs / 500.0f, 2.0f); 
    if (blink < 1.0f && !exiting) { 
        GFX_DrawShadowedText(&text, 160.0f, 110.0f, 0.9f, 1.0f, 1.0f,
            GFX_ALIGN_CENTER,
            GFX_COLOR_RGBA(255, 255, 255, 255),
            GFX_COLOR_RGBA(0, 0, 0, 255));
    }
}

void sceneTitleExit(void) { 
    GFX_TextBufDelete(buf);
    stopAudio(0);
}

