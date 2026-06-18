#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "cJSON.h"
#include "scene_tutorial.h"
#include "scene_manager.h"
#include "main.h"
#include "sprites.h"
#include "audio_shit.h" 
#include "scene_init.h"
#include "drawing.h"
#include "logs.h"
#include "scene_mainmenu.h"

static u64 lastTick = 0;
static float dt = 0.0f;

static float tut_bg_x = 0.0f;
static float tut_scroll_speed = 0.5f;

float easeOutCubicTut(float t) {
    float f = 1.0f - t;
    return 1.0f - (f * f * f);
}

float easeOutBackImg(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float f = t - 1.0f;
    return 1.0f + c3 * (f * f * f) + c1 * (f * f);
}

typedef struct {
    char text[256];
    int speakingImageId;   
    int idleImageId;       
    float speechDuration;  
    GFX_IMAGE* topImage;    
    GFX_IMAGE* bottomImage; 
} TutorialStep;

#define MAX_TUTORIAL_STEPS 50
static TutorialStep dynamicScript[MAX_TUTORIAL_STEPS];
static int totalSteps = 0;
static int currentStep = 0;

static float speechTimer = 0.0f; 
static int currentDisplayedCharId = 0; 

static GFX_TEXTBUF tutorialTextBuf;
static GFX_TEXT currentTextObj;

static GFX_TEXTBUF promptBuf;
static GFX_TEXT promptText;

static float flashTimer = 0.0f;
static float FLASH_DURATION = 0.5f;

static float entranceTimer = 0.0f;
static float ENTRANCE_DURATION = 1.2f;

static float charBounceTimer = 0.0f;
static bool isBouncing = false;
static float BOUNCE_SPEED = 12.0f; 

static float topImgTimer = 0.0f;
static float botImgTimer = 0.0f;
static float IMG_ANIM_DURATION = 0.5f; 
int offset = 0;

static char previousVoicePack[64];
bool Tutorial_audio_loaded = false;
static inline GFX_IMAGE* getTopImageById(int id) {
    switch(id) {
        case 1: return top1;
        case 2: return top2;
        case 3: return top3;
        case 4: return top4;
        case 5: return top5;
        case 6: return top6;
        default: return NULL;
    }
}

static inline GFX_IMAGE* getBotImageById(int id) {
    switch(id) {
        case 1: return bot1;
        case 2: return bot2;
        case 3: return bot3;
        case 4: return bot4;
        case 5: return bot5;
        case 6: return bot6;
        case 7: return bot7;
        case 8: return bot8;
        case 9: return bot9;
        case 10: return bot10;
        case 11: return bot11;
        case 12: return bot12;
        default: return NULL;
    }
}
extern char activeVoicePackSource[16];
static bool loadTutorialDialogue(const char* vpName) {
    char filepath[128];
    if (strcmp(activeVoicePackSource, "sd") == 0) {
        snprintf(filepath, sizeof(filepath), "/3ds/Koleo3DS/vp/%s/dialogue.json", vpName);
    } else {
        if (strcmp(vpName, "Nikt") == 0) {
            snprintf(filepath, sizeof(filepath), "romfs:/vp/Nishi/dialogue.json", vpName);
        } else {
            snprintf(filepath, sizeof(filepath), "romfs:/vp/%s/dialogue.json", vpName);
        }
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        log_to_file("[ERR] Failed to open dialogue: %s\n", filepath);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);

    char* jsonBuffer = (char*)linearAlloc(fileSize + 1);
    fread(jsonBuffer, 1, fileSize, file);
    fclose(file);
    jsonBuffer[fileSize] = '\0';

    cJSON* root = cJSON_Parse(jsonBuffer);
    linearFree(jsonBuffer); 

    if (!root) {
        log_to_file("[ERR] Invalid JSON in %s\n", filepath);
        return false;
    }

    totalSteps = cJSON_GetArraySize(root);
    if (totalSteps > MAX_TUTORIAL_STEPS) totalSteps = MAX_TUTORIAL_STEPS;

    for (int i = 0; i < totalSteps; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        cJSON* textNode = cJSON_GetObjectItem(item, "text");
        cJSON* lenNode = cJSON_GetObjectItem(item, "speech_len");
        
        cJSON* speakImgNode = cJSON_GetObjectItem(item, "speakingImageId");
        cJSON* idleImgNode = cJSON_GetObjectItem(item, "idleImageId");
        cJSON* topImgNode = cJSON_GetObjectItem(item, "topImageId");
        cJSON* botImgNode = cJSON_GetObjectItem(item, "botImageId");

        if (textNode && cJSON_IsString(textNode)) {
            strncpy(dynamicScript[i].text, textNode->valuestring, sizeof(dynamicScript[i].text) - 1);
            dynamicScript[i].text[sizeof(dynamicScript[i].text) - 1] = '\0'; 
        } else {
            strcpy(dynamicScript[i].text, "Missing text");
        }

        if (lenNode && cJSON_IsNumber(lenNode)) {
            dynamicScript[i].speechDuration = (float)lenNode->valuedouble;
        } else {
            dynamicScript[i].speechDuration = 2.0f; 
        }

        dynamicScript[i].speakingImageId = (speakImgNode && cJSON_IsNumber(speakImgNode)) ? speakImgNode->valueint : 1; 
        dynamicScript[i].idleImageId = (idleImgNode && cJSON_IsNumber(idleImgNode)) ? idleImgNode->valueint : 2;
        int topId = (topImgNode && cJSON_IsNumber(topImgNode)) ? topImgNode->valueint : 0;
        int botId = (botImgNode && cJSON_IsNumber(botImgNode)) ? botImgNode->valueint : 0;

        dynamicScript[i].topImage = getTopImageById(topId);
        dynamicScript[i].bottomImage = getBotImageById(botId);
    }

    cJSON_Delete(root); 
    return true;
}

void advanceTutorialStep() {
    int nextStep = currentStep + 1;
    
    if (nextStep >= totalSteps) {
        sceneManagerSwitchTo(SCENE_MAINMENU);
    } else {
        GFX_IMAGE* oldTop = dynamicScript[currentStep].topImage;
        GFX_IMAGE* newTop = dynamicScript[nextStep].topImage;
        GFX_IMAGE* oldBot = dynamicScript[currentStep].bottomImage;
        GFX_IMAGE* newBot = dynamicScript[nextStep].bottomImage;

        if (newTop != oldTop && newTop != NULL) topImgTimer = 0.0f;
        if (newBot != oldBot && newBot != NULL) botImgTimer = 0.0f;

        if (VOICEACT > 0) stopVoicePackAudio(currentStep);
        
        currentStep = nextStep;
        speechTimer = 0.0f;
        currentDisplayedCharId = dynamicScript[currentStep].speakingImageId;

        if (VOICEACT > 0) playVoicePackAudio(currentStep, true);

        GFX_TextBufClear(tutorialTextBuf);
        GFX_TextParse(&currentTextObj, tutorialTextBuf, dynamicScript[currentStep].text);
        GFX_TextOptimize(&currentTextObj);
        
        isBouncing = true;
        charBounceTimer = 0.0f;
    }
}

void sceneTutorialInit(void) {
    stopAudio(2);
    bgm_playing = false;
    lastTick = svcGetSystemTick();
    dt = 0.0f;
    currentStep = 0;
    
    if (!loadTutorialDialogue(activeVoicePack)) {
        
        totalSteps = 1;
        strcpy(dynamicScript[0].text, "Error loading dialogue.json!");
        dynamicScript[0].speechDuration = 5.0f;
        dynamicScript[0].speakingImageId = 1;
        dynamicScript[0].idleImageId = 2;
        dynamicScript[0].topImage = NULL;
        dynamicScript[0].bottomImage = NULL;
    }
    
    speechTimer = 0.0f;
    currentDisplayedCharId = dynamicScript[currentStep].speakingImageId;
    
    flashTimer = 0.0f;
    entranceTimer = 0.0f;
    charBounceTimer = 0.0f;
    isBouncing = false;
    topImgTimer = 0.0f;
    botImgTimer = 0.0f;
    tut_bg_x = 0.0f;

    tutorialTextBuf = GFX_TextBufNew(4096);
    GFX_TextBufClear(tutorialTextBuf);
    
    GFX_TextParse(&currentTextObj, tutorialTextBuf, dynamicScript[currentStep].text);
    GFX_TextOptimize(&currentTextObj);
    
    promptBuf = GFX_TextBufNew(256);
    GFX_TextParse(&promptText, promptBuf, "Naciśnij (A), aby kontynuować");
    GFX_TextOptimize(&promptText);
    if (!Tutorial_audio_loaded) {
        unloadAllVoicePackAudio();
        
    }
    if (VOICEACT > 0 && !Tutorial_audio_loaded) {
        for(int i = 0; i < totalSteps; i++) {
            loadVoicePackAudio(activeVoicePack, i);
        }
        Tutorial_audio_loaded = true;
    }
    playAudio(7, true, 0.25f);
    playVoicePackAudio(currentStep, true);
    
}

void sceneTutorialUpdate(uint32_t kDown, uint32_t kHeld) {
    u64 currentTick = svcGetSystemTick();
    dt = (float)(currentTick - lastTick) / CPU_TICKS_PER_MSEC / 1000.0f;
    lastTick = currentTick;
    if (dt > 0.1f) dt = 0.1f;
    
    speechTimer += dt;
    if (currentStep < totalSteps) {
        if (speechTimer >= dynamicScript[currentStep].speechDuration) {
            currentDisplayedCharId = dynamicScript[currentStep].idleImageId;
        }
        if (currentStep == 27 && speechTimer >= dynamicScript[currentStep].speechDuration) {
            advanceTutorialStep();
            return; 
        }
    }

    tut_bg_x -= tut_scroll_speed * (dt * 60.0f);
    if (tut_bg_x <= -80.0f) tut_bg_x += 80.0f;

    if (flashTimer < FLASH_DURATION) {
        flashTimer += dt;
        if (flashTimer > FLASH_DURATION) flashTimer = FLASH_DURATION;
    }
    if (entranceTimer < ENTRANCE_DURATION) {
        entranceTimer += dt;
        if (entranceTimer > ENTRANCE_DURATION) entranceTimer = ENTRANCE_DURATION;
    }
    if (isBouncing) {
        charBounceTimer += BOUNCE_SPEED * dt;
        if (charBounceTimer >= M_PI) { 
            charBounceTimer = 0.0f;
            isBouncing = false;
        }
    }
    if (topImgTimer < IMG_ANIM_DURATION) {
        topImgTimer += dt;
        if (topImgTimer > IMG_ANIM_DURATION) topImgTimer = IMG_ANIM_DURATION;
    }
    if (botImgTimer < IMG_ANIM_DURATION) {
        botImgTimer += dt;
        if (botImgTimer > IMG_ANIM_DURATION) botImgTimer = IMG_ANIM_DURATION;
    }

    if (flashTimer >= FLASH_DURATION * 0.5f) {
        if (kDown & KEY_A) {
            advanceTutorialStep();
        }
        
        if (kDown & KEY_SELECT) {
             sceneManagerSwitchTo(SCENE_MAINMENU);
        }
    }
}

void drawTutorialTop(float offset) {
    GFX_DrawRectSolid(0, 0, 0.1f, 400, 240, GFX_COLOR_RGBA(48,74,130,255));
    narysujpociagi(40);

    float entranceT = entranceTimer / ENTRANCE_DURATION;
    if (entranceT > 1.0f) entranceT = 1.0f;
    
    float dialogEase = easeOutCubicTut(entranceT);
    
    float charStart = 0.3f;
    float charT = (entranceT - charStart) / (1.0f - charStart);
    if (charT < 0.0f) charT = 0.0f;
    if (charT > 1.0f) charT = 1.0f;
    float charEase = easeOutCubicTut(charT);

    if (dynamicScript[currentStep].topImage != NULL) {
        GFX_IMAGE* img = dynamicScript[currentStep].topImage;
        
        float animT = topImgTimer / IMG_ANIM_DURATION;
        if (animT > 1.0f) animT = 1.0f;
        float popScale = easeOutBackImg(animT);
        
        if (popScale < 0.0f) popScale = 0.0f;

        float drawW = img->width * popScale;
        float drawH = img->height * popScale;

        float x = 200.0f - (drawW / 2.0f);
        float y = 120.0f - (drawH / 2.0f);
        
        GFX_DrawImageAt(img, x + offset, y, 0.5f, NULL, popScale, popScale);
    }

    float charBaseX = -180.0f + (200.0f * charEase); 
    float charBaseY = 40.0f;
    
    float bounceY = 0.0f;
    if (isBouncing) {
        bounceY = -10.0f * sinf(charBounceTimer); 
    }
    float idleBob = sinf(osGetTime() / 500.0f) * 2.0f;
    float finalCharY = charBaseY + bounceY + idleBob;
    
    float charW = 100.0f;
    
    float boxH = 80.0f;
    float targetBoxY = 240.0f - boxH;
    float startBoxY = 240.0f; 
    
    float currentBoxY = startBoxY - ((startBoxY - targetBoxY) * dialogEase);
    
    u32 colorTop = GFX_COLOR_RGBA(0, 0, 0, 220);
    u32 colorBot = GFX_COLOR_RGBA(50, 50, 80, 240);
    
    float uiOffset = offset * 0.5f; 
    
    GFX_DrawRectangle(-20 + uiOffset, currentBoxY, 0.8f, 450.0f, boxH, colorTop, colorTop, colorBot, colorBot);
    GFX_DrawRectSolid(-20 + uiOffset, currentBoxY, 0.85f, 450.0f, 2.0f, GFX_COLOR_RGBA(208, 198, 255, 255));

    float textX = 160.0f; 
    float textY = currentBoxY + 15.0f;
    
    if (currentBoxY < 240.0f) {
        GFX_DrawText(&currentTextObj, textX + uiOffset, textY, 0.9f, 0.4f, 0.4f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255, 255, 255, 255));
    }

    if (charBaseX + charW > -200.0f) {
        float x = charBaseX + offset; 
        switch(VOICEACT) {
            case 1:        
                switch(currentDisplayedCharId) {
                    case 0:
                        GFX_DrawImageAt(kun_excited, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                    case 1:
                        GFX_DrawImageAt(kun_idle_speak, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break; 
                    case 2:
                        GFX_DrawImageAt(kun_idle, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                    case 3:
                        GFX_DrawImageAt(kun_diss_speak, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break; 
                    case 4:
                        GFX_DrawImageAt(kun_diss, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                }
                break;
            case 2:
                
                switch(currentDisplayedCharId) {
                    case 0:
                        GFX_DrawImageAt(chan_excited, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                    case 1:
                        GFX_DrawImageAt(chan_idle_speak, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break; 
                    case 2:
                        GFX_DrawImageAt(chan_idle, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                    case 3:
                        GFX_DrawImageAt(chan_diss_speak, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break; 
                    case 4:
                        GFX_DrawImageAt(chan_diss, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                }
                break;
            case 3:
                
                switch(currentDisplayedCharId) {
                    
                }
                break;
            default:
                switch(currentDisplayedCharId) {
                    case 0:
                        GFX_DrawImageAt(chan_excited, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                    case 1:
                        GFX_DrawImageAt(chan_idle_speak, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break; 
                    case 2:
                        GFX_DrawImageAt(chan_idle, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                    case 3:
                        GFX_DrawImageAt(chan_diss_speak, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break; 
                    case 4:
                        GFX_DrawImageAt(chan_diss, x - 15.0f, finalCharY - 30.0f, 1.0f, NULL, 1.0f, 1.0f);
                        break;
                }
        }
    }
    if (flashTimer < FLASH_DURATION) {
        float alpha = 1.0f - (flashTimer / FLASH_DURATION);
        u32 white = GFX_COLOR_RGBA(255, 255, 255, (u8)(255 * alpha));
        GFX_DrawRectSolid(0, 0, 1.0f, 400, 240, white);
    }
}

void sceneTutorialRender(void) {
    GFX_BeginSceneTop(0, true);
     
    drawTutorialTop(0.0f);

    if (slider > 0.0f) {
        GFX_BeginSceneTop(1, true);
        
        drawTutorialTop(slider * 5.0f);
    }

    GFX_BeginSceneBottom();
    
    GFX_DrawRectSolid(0, 0, 0.1f, 400, 240, GFX_COLOR_RGBA(48,74,130,255));
    narysujpociagi(40);
    
    if (dynamicScript[currentStep].bottomImage != NULL) {
        GFX_IMAGE* img = dynamicScript[currentStep].bottomImage;

        float animT = botImgTimer / IMG_ANIM_DURATION;
        if (animT > 1.0f) animT = 1.0f;
        float popScale = easeOutBackImg(animT);

        if (popScale < 0.0f) popScale = 0.0f;

        float drawW = img->width * popScale;
        float drawH = img->height * popScale;

        float x = 160.0f - (drawW / 2.0f);
        float y = 120.0f - (drawH / 2.0f);

        GFX_DrawImageAt(img, x, y, 0.5f, NULL, popScale, popScale);
    }

    GFX_DrawShadowedText(&promptText, 160.0f, 200.0f, 0.5f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(0, 0, 0, 255), GFX_COLOR_RGBA(255, 255, 255, 200));
    
    if (flashTimer < FLASH_DURATION) {
        float alpha = 1.0f - (flashTimer / FLASH_DURATION);
        u32 white = GFX_COLOR_RGBA(255, 255, 255, (u8)(255 * alpha));
        GFX_DrawRectSolid(0, 0, 1.0f, 320, 240, white);
    }
}

void sceneTutorialExit(void) {
    GFX_TextBufDelete(tutorialTextBuf);
    GFX_TextBufDelete(promptBuf);
    stopAudio(7);
    if (VOICEACT > 0) {
        stopVoicePackAudio(currentStep); 
    }
}

