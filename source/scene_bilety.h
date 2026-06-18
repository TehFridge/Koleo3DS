#ifndef SCENE_BILETY_H
#define SCENE_BILETY_H
#include <stdint.h>

void sceneBiletyInit(void);
void sceneBiletyUpdate(uint32_t kDown, uint32_t kHeld);
void sceneBiletyRender(void);
void sceneBiletyExit(void);

#endif
