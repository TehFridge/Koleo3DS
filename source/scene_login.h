#ifndef SCENE_LOGIN_H
#define SCENE_LOGIN_H
#include <stdint.h>

void sceneLoginInit(void);
void sceneLoginUpdate(uint32_t kDown, uint32_t kHeld);
void sceneLoginRender(void);
void sceneLoginExit(void);

#endif
