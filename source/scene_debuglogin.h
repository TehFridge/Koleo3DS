#ifndef SCENE_DEBUGLOGIN_H
#define SCENE_DEBUGLOGIN_H
#include <stdint.h>

void sceneDebugloginInit(void);
void sceneDebugloginUpdate(uint32_t kDown, uint32_t kHeld);
void sceneDebugloginRender(void);
void sceneDebugloginExit(void);

#endif
