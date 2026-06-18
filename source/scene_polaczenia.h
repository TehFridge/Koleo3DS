#ifndef SCENE_POLACZENIA_H
#define SCENE_POLACZENIA_H
#include <stdint.h>

void scenePolaczeniaInit(void);
void scenePolaczeniaUpdate(uint32_t kDown, uint32_t kHeld);
void scenePolaczeniaRender(void);
void scenePolaczeniaExit(void);

#endif
