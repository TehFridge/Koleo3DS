#include "scene_manager.h"
#include "sprites.h"
#include "scene_init.h"
#include "scene_debuglogin.h"
#include "scene_debugmainmenu.h"
#include "scene_title.h"
#include "scene_login.h"
#include "scene_mainmenu.h"
#include "scene_polaczenia.h"
#include "scene_biletkup.h"
#include "scene_bilety.h"
#include "scene_tutorial.h"
#include "drawing.h"
#include "scene_credits.h"

bool debug;
static SceneType currentScene = SCENE_NONE;

void sceneManagerInit(SceneType initialScene) {
    sceneManagerSwitchTo(initialScene);
}

void sceneManagerUpdate(uint32_t kDown, uint32_t kHeld) {
    updatePociagi();
    switch (currentScene) {
        case SCENE_INIT: sceneInitUpdate(kDown, kHeld); break;
        case SCENE_DEBUGLOGIN: sceneDebugloginUpdate(kDown, kHeld); break;
        case SCENE_DEBUGMAINMENU: sceneDebugmainmenuUpdate(kDown, kHeld); break;
        case SCENE_TITLE: sceneTitleUpdate(kDown, kHeld); break;
        case SCENE_LOGIN: sceneLoginUpdate(kDown, kHeld); break;
        case SCENE_MAINMENU: sceneMainmenuUpdate(kDown, kHeld); break;
        case SCENE_POLACZENIA: scenePolaczeniaUpdate(kDown, kHeld); break;
        case SCENE_BILETKUP: sceneBiletkupUpdate(kDown, kHeld); break;
        case SCENE_BILETY: sceneBiletyUpdate(kDown, kHeld); break;
        case SCENE_TUTORIAL: sceneTutorialUpdate(kDown, kHeld); break;
        case SCENE_CREDITS: sceneCreditsUpdate(kDown, kHeld); break;
        default: break;
    }
}

void sceneManagerRender(void) {
    switch (currentScene) {
        case SCENE_INIT: sceneInitRender(); break;
        case SCENE_DEBUGLOGIN: sceneDebugloginRender(); break;
        case SCENE_DEBUGMAINMENU: sceneDebugmainmenuRender(); break;
        case SCENE_TITLE: sceneTitleRender(); break;
        case SCENE_LOGIN: sceneLoginRender(); break;
        case SCENE_MAINMENU: sceneMainmenuRender(); break;
        case SCENE_POLACZENIA: scenePolaczeniaRender(); break;
        case SCENE_BILETKUP: sceneBiletkupRender(); break;
        case SCENE_BILETY: sceneBiletyRender(); break;
        case SCENE_TUTORIAL: sceneTutorialRender(); break;
        case SCENE_CREDITS: sceneCreditsRender(); break;
        default: break;
    }
}

void sceneManagerSwitchTo(SceneType nextScene) {
    
    switch (currentScene) {
        case SCENE_INIT: sceneInitExit(); break;
        case SCENE_DEBUGLOGIN: sceneDebugloginExit(); break;
        case SCENE_DEBUGMAINMENU: sceneDebugmainmenuExit(); break;
        case SCENE_TITLE: sceneTitleExit(); break;
        case SCENE_LOGIN: sceneLoginExit(); break;
        case SCENE_MAINMENU: sceneMainmenuExit(); break;
        case SCENE_POLACZENIA: scenePolaczeniaExit(); break;
        case SCENE_BILETKUP: sceneBiletkupExit(); break;
        case SCENE_BILETY: sceneBiletyExit(); break;
        case SCENE_TUTORIAL: sceneTutorialExit(); break;
        case SCENE_CREDITS: sceneCreditsExit(); break;
        default: break;
    }


    currentScene = nextScene;
    switch (currentScene) {
        case SCENE_INIT: sceneInitInit(); break;;
        case SCENE_DEBUGLOGIN: sceneDebugloginInit(); break;
        case SCENE_DEBUGMAINMENU: sceneDebugmainmenuInit(); break;
        case SCENE_TITLE: sceneTitleInit(); break;
        case SCENE_LOGIN: sceneLoginInit(); break;
        case SCENE_MAINMENU: sceneMainmenuInit(); break;
        case SCENE_POLACZENIA: scenePolaczeniaInit(); break;
        case SCENE_BILETKUP: sceneBiletkupInit(); break;
        case SCENE_BILETY: sceneBiletyInit(); break;
        case SCENE_TUTORIAL: sceneTutorialInit(); break;
        case SCENE_CREDITS: sceneCreditsInit(); break;
        default: break;
    }
}

void sceneManagerExit(void) {
    sceneManagerSwitchTo(SCENE_NONE);
}
