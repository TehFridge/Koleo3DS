#ifndef SCENE_BILETKUP_H
#define SCENE_BILETKUP_H
#include <stdint.h>

void sceneBiletkupInit(void);
void sceneBiletkupUpdate(uint32_t kDown, uint32_t kHeld);
void sceneBiletkupRender(void);
void sceneBiletkupExit(void);

#endif
