#ifndef SCENE_TUTORIAL_H
#define SCENE_TUTORIAL_H
#include <stdint.h>

extern bool Tutorial_audio_loaded;

void sceneTutorialInit(void);
void sceneTutorialUpdate(uint32_t kDown, uint32_t kHeld);
void sceneTutorialRender(void);
void sceneTutorialExit(void);

#endif
