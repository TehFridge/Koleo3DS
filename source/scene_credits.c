#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <math.h> 
#include "scene_credits.h"
#include "scene_manager.h"
#include "main.h"
#include "sprites.h"
#include "audio_shit.h"
#include "scene_mainmenu.h"
#include "drawing.h"

#define STATE_FLASH      0
#define STATE_LOGO_FADE  1
#define STATE_WAIT       2
#define STATE_SCROLL     3

static int sceneState = 0;
static float flashAlpha = 255.0f; 
static float logoAlpha = 0.0f;    
static float waveAlpha = 0.0f;    
static float scrollY = 480.0f;    
static u64 stateTimer = 0;        
static u64 sceneStartTime = 0;    
static bool musicStarted = false; 

static GFX_TEXTBUF creditBuf;
static GFX_TEXT creditText[1000];   
static GFX_TEXT exitText;           

static const char* creditLines[] = {
    "Koleo3DS",
    "",
    "",
    "",
    "PROGRAMOWANIE",
    "TehFridge",
    "",
    "",
    "",
    "GRAFIKA",
    "",
    "UI Design",
    "TehFridge",
    "",
    "Koleo Chan/Kun Design",
    "Sansy (@fedora_maniac)",
    "",
    "",
    "",
    "UDŹWIĘKOWIENIE",
    "",
    "Koleo Chan VA",
    "Sansy (@fedora_maniac)",
    "",
    "Koleo Kun VA",
    "Nishi Yuuma",
    "",
    "Archon VA",
    "Arkadiusz \"Dark Archon\" Kamiński",
    "",
    "Main Menu Music",
    "Kirby's Adventure - Orange Ocean",
    "[7-N163 Arrangement]",
    "(Rei8bit)",
    "",
    "Bilety Menu Music",
    "Toolbox [FDS]",
    "(Rei8bit)",
    "", 
    "Kupowanie Biletu Music",
    "Cybermagnetic Floor [7-N163]",
    "(Rei8bit)",
    "",
    "Tutorial Music",
    "Chrono Trigger - Delightful Spekkio",
    "[2xYM2149 Arrangement]",
    "(Rei8bit)",
    "",
    "Credits Music",
    "Wacky Workbench (Re-Imagined)",
    "[YM2151 + SegaPCM]",
    "(Megabaz)",
    "",
    "",
    "DLC VA",
    "",
    "Reichi Shinigami",
    "Vahn Blackwood",
    "akirsonka",
    "Szfed8",
    "NickDS",
    "Dianko Lastrea",
    "",
    "",
    "Additional pozdrowienia dla:",
    "Chiyoko",
    "stary nie zmieniaj się",
    "jesteś sztywny git :)",
    "",
    "BartkaGM",
    "fajna prelka na remconie :D", 
    "",
    "PituDitu",
    "fajnie sie rysowało na",
    "pictochat xDDD",
    "",
    "Ser w Grach",
    "zabrałeś te sery z lodówki",
    "na EC1?",
    "",
    "Kube Czajkowskiego",
    "fajną apke pan zrobił",
    "lubie :D",
    "",
    "oraz wszystkich Pyrkonowiczów",
    "którzy byli na prelekcji", 
    "Dzięki za przyjście :D", 
    "(AMBEREXPO odśnieżcie dach plz)",
    "(a wait zły konwent, anyways)" 
    "",
    "",
    "Serdeczne podziękowania dla",
    "wszystkich którzy wspierali",
    "mnie podczas tworzenia tego",
    "projektu i w życiu prywatnym.",
    ":D",
    "",
    "Projekt korzysta z API Aplikacji",
    "KOLEO",
    "",
    "KOLEO nie jest w żaden sposób",
    "powiązane z tym projektem.",
    "",
    "Wszystkie znaki towarowe",
    "należą do ich właścicieli.",
    "",
    ""
};
#define CREDIT_LINE_COUNT (sizeof(creditLines)/sizeof(creditLines[0]))

void drawCaptureWaveform(float alphaVal) {
    if (!captureBuf) return;
    if (alphaVal <= 0.0f) return; 

    GSPGPU_InvalidateDataCache(captureBuf, CAPTURE_SIZE);

    s16* pcmData = (s16*)captureBuf;
    
    bool hasSound = false;
    for(int i=0; i<100; i++) {
        if(pcmData[i] > 100 || pcmData[i] < -100) { 
            hasSound = true;
            break;
        }
    }

    float prevX = 0.0f;
    float prevY = 120.0f;
    int step = 1;

    bgm_playing = false;
    
    u8 lineAlpha = (u8)((178.0f * alphaVal) / 255.0f);
    u8 shadowAlpha = (u8)alphaVal;

    u32 lineColor = GFX_COLOR_RGBA(192, 255, 0, lineAlpha); 
    u32 shadowColor = GFX_COLOR_RGBA(0, 0, 0, shadowAlpha);

    for (int x = 0; x < 400; x+=2) {
        if (x * step >= (CAPTURE_SIZE / 2)) break;

        s16 sample = pcmData[x * step];
        float normalized = (float)sample / 32768.0f;
        
        float currentY = 120.0f + (normalized * 80);
        float currentX = (float)x;

        if (x > 0) {
            GFX_DrawLine(
                prevX, prevY, shadowColor, 
                currentX, currentY + 3.0f, shadowColor,
                2.0f, 0.5f 
            );
            GFX_DrawLine(
                prevX, prevY, lineColor,   
                currentX, currentY, lineColor,
                2.0f, 0.5f 
            );
        }
        prevX = currentX;
        prevY = currentY;
    }
}

void sceneCreditsInit(void) {
    
    sceneState = STATE_FLASH;
    flashAlpha = 255.0f;
    logoAlpha = 0.0f;
    waveAlpha = 0.0f; 
    scrollY = 480.0f; 
    
    stateTimer = 0;
    sceneStartTime = osGetTime(); 
    musicStarted = false;

    creditBuf = GFX_TextBufNew(4096);
    for (int i = 0; i < CREDIT_LINE_COUNT; i++) {
        GFX_TextParse(&creditText[i], creditBuf, creditLines[i]);
        GFX_TextOptimize(&creditText[i]);
    }

    GFX_TextParse(&exitText, creditBuf, "Wciśnij (B) by wyjść");
    GFX_TextOptimize(&exitText);
}

void sceneCreditsUpdate(uint32_t kDown, uint32_t kHeld) {
    
    if (kDown & (KEY_B | KEY_START)) {
        sceneManagerSwitchTo(SCENE_MAINMENU); 
        return; 
    }

    if (!musicStarted) {
        if (osGetTime() - sceneStartTime >= 1500) {
            playAudio(6, true, 0.9f);
            musicStarted = true;
        }
    }

    if (sceneState == STATE_FLASH) {
        if (flashAlpha > 0.0f) {
            flashAlpha -= 5.0f; 
            if (flashAlpha < 0.0f) flashAlpha = 0.0f;
        } else {
            sceneState = STATE_LOGO_FADE;
        }
    }

    if (sceneState == STATE_LOGO_FADE) {
        if (logoAlpha < 255.0f) {
            logoAlpha += 2.0f; 
            if (logoAlpha > 255.0f) logoAlpha = 255.0f;
        } else {
            sceneState = STATE_WAIT;
            stateTimer = osGetTime();
        }
    }

    if (sceneState == STATE_WAIT) {
        if (osGetTime() - stateTimer >= 1500) { 
            sceneState = STATE_SCROLL;
        }
    }

    if (sceneState == STATE_SCROLL) {
        scrollY -= 0.5f; 
        
        if (waveAlpha < 255.0f) {
            waveAlpha += 2.0f; 
            if (waveAlpha > 255.0f) waveAlpha = 255.0f;
        }

        if (logoAlpha > 50.0f) logoAlpha -= 0.5f; 
    }
}

void drawTopContent(float offset, float alphaFade) {
    
    if (koleo_logo->width) {

        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, alphaFade); 
        
        GFX_DrawImageAt(koleo_logo, 0, 0, 0.5f, &tint, 1.0f, 1.0f);
    }
    drawCaptureWaveform(waveAlpha);
    
    if (sceneState == STATE_SCROLL) {
        float currentY = scrollY; 
        
        for (int i = 0; i < CREDIT_LINE_COUNT; i++) {
            float width, height;
            GFX_TextGetDimensions(&creditText[i], 0.7f, 0.7f, &width, &height);
            
            float x = (400 - width) / 2;
            
            if (currentY + height > 0 && currentY < 240) {
                GFX_DrawText(&creditText[i], x + offset, currentY, 0.6f, 0.7f, 0.7f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255, 255, 255, 255));
            }
            currentY += height + 10.0f;
        }
    }
}

void drawBottomContent(void) {
    if (sceneState == STATE_SCROLL) {
        
        float currentY = scrollY - 240.0f; 
        
        for (int i = 0; i < CREDIT_LINE_COUNT; i++) {
            float width, height;
            GFX_TextGetDimensions(&creditText[i], 0.7f, 0.7f, &width, &height);
            
            float x = (320 - width) / 2;
            
            if (currentY + height > 0 && currentY < 240) {
                GFX_DrawText(&creditText[i], x, currentY, 0.6f, 0.7f, 0.7f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255, 255, 255, 255));
            }
            currentY += height + 10.0f;
        }
    }

    if (sceneState != STATE_FLASH) {
        
        u8 pulseAlpha = (u8)(fabs(sin(osGetTime() / 300.0)) * 255.0f);

        if (pulseAlpha < 50) pulseAlpha = 50;

        u32 exitColor = GFX_COLOR_RGBA(255, 255, 255, pulseAlpha);
        u32 exitShadow = GFX_COLOR_RGBA(235, 163, 7, pulseAlpha);

        GFX_DrawShadowedText(&exitText, 160.0f, 220.0f, 0.9f, 0.5f, 0.5f, GFX_ALIGN_CENTER, exitColor, exitShadow);
    }
}

void sceneCreditsRender(void) {
    float logoAlphaVal = logoAlpha / 255.0f;

    GFX_BeginSceneTop(0, true); 
    
    drawTopContent(0.0f, logoAlphaVal);
    
    if (flashAlpha > 0.0f) {
        GFX_DrawRectSolid(0, 0, 1.0f, 400, 240, GFX_COLOR_RGBA(255, 255, 255, (u8)flashAlpha));
    }

    if (slider > 0.0f) {
        GFX_BeginSceneTop(1, true);
        
        drawTopContent(slider * -5.0f, logoAlphaVal); 
        
        if (flashAlpha > 0.0f) {
            GFX_DrawRectSolid(0, 0, 1.0f, 400, 240, GFX_COLOR_RGBA(255, 255, 255, (u8)flashAlpha));
        }
    }

    GFX_BeginSceneBottom(); 
     
    drawBottomContent();
    
    if (flashAlpha > 0.0f) {
        GFX_DrawRectSolid(0, 0, 0, 320, 240, GFX_COLOR_RGBA(255, 255, 255, (u8)flashAlpha));
    }
}

void sceneCreditsExit(void) {
    if (creditBuf) GFX_TextBufDelete(creditBuf);
    bgm_playing = false;
    stopAudio(6);
}

