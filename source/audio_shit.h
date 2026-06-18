#ifndef CWAV_SHIT_H
#define CWAV_SHIT_H

#include <cwav.h>
#include "main.h"

extern u8* captureBuf;
extern ndspWaveBuf captureWaveBuf; 
extern const u32 CAPTURE_SIZE;

extern size_t linear_bytes_used;
void initAudioSystem(void);
void freeAllAudios(void);

CWAV* getAudio(u32 index);
void playAudio(u32 index, bool stereo, float vol);
void stopAudio(u32 index);

bool loadAudioIndex(u32 index);
void unloadAudioIndex(u32 index);

#define MAX_VP_FILES 31 

extern char activeVoicePack[128]; 

bool loadVoicePackAudio(const char* vpName, u32 step);
void unloadAllVoicePackAudio(void);
void playVoicePackAudio(u32 step, bool stereo);
void stopVoicePackAudio(u32 step);

#define MAX_NAV_FILES 14

bool loadNavVoiceLines(const char* vpName);
void playNavAudio(u32 index, bool stereo);
void unloadAllNavAudio(void);

#endif

