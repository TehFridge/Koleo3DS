#ifndef SCENE_DEBUGMAINMENU_H
#define SCENE_DEBUGMAINMENU_H
#include <stdint.h>

void sceneDebugmainmenuInit(void);
void sceneDebugmainmenuUpdate(uint32_t kDown, uint32_t kHeld);
void sceneDebugmainmenuRender(void);
void sceneDebugmainmenuExit(void);

#endif
