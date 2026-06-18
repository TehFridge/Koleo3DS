#include "audio_shit.h"
#include "logs.h"
#include <stdlib.h> 
#include <stdio.h> 
#include <string.h>
#include "main.h"

#define FILE_COUNT (sizeof(fileList) / sizeof(fileList[0]))

const char* fileList[] = {
    "romfs:/bgm/title.bcwav",
    "romfs:/bgm/login.bcwav",          
    "romfs:/bgm/mainmenu.bcwav",
    "romfs:/bgm/bilety.bcwav",
    "romfs:/bgm/biletkup.bcwav",
    "romfs:/sfx/change.bcwav",
    "romfs:/bgm/credits.bcwav",
    "romfs:/bgm/tutorial.bcwav"
};

const u8 maxSPlayList[] = {
    2, 2, 2, 2, 2, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1
};

typedef struct {
    const char* filename;
    CWAV* cwav;
    void* buffer;
    bool loaded;
    u32 size; 
} AudioEntry;

static AudioEntry audioList[FILE_COUNT];

const char* cwavGetStatusString(cwavStatus_t status) {
    switch (status) {
        case CWAV_NOT_ALLOCATED:           return "CWAV_NOT_ALLOCATED";
        case CWAV_SUCCESS:                 return "CWAV_SUCCESS";
        case CWAV_INVALID_ARGUMENT:        return "CWAV_INVALID_ARGUMENT";
        case CWAV_FILE_OPEN_FAILED:        return "CWAV_FILE_OPEN_FAILED";
        case CWAV_FILE_READ_FAILED:        return "CWAV_FILE_READ_FAILED";
        case CWAV_UNKNOWN_FILE_FORMAT:     return "CWAV_UNKNOWN_FILE_FORMAT";
        case CWAV_INVAID_INFO_BLOCK:       return "CWAV_INVAID_INFO_BLOCK";
        case CWAV_INVAID_DATA_BLOCK:       return "CWAV_INVAID_DATA_BLOCK";
        case CWAV_UNSUPPORTED_AUDIO_ENCODING: return "CWAV_UNSUPPORTED_AUDIO_ENCODING";
        case CWAV_INVALID_CWAV_CHANNEL:    return "CWAV_INVALID_CWAV_CHANNEL";
        case CWAV_NO_CHANNEL_AVAILABLE:    return "CWAV_NO_CHANNEL_AVAILABLE";
        default:                           return "UNKNOWN_STATUS";
    }
}

bool loadAudioIndex(u32 index) {
    if (index >= FILE_COUNT) return false;

    AudioEntry* entry = &audioList[index];
    if (entry->loaded) {
        log_to_file("[CWAV] Index %d already loaded at PTR: %p\n", index, entry->cwav);
        return true; 
    }

    FILE* file = fopen(entry->filename, "rb");
    if (!file) {
        log_to_file("[WARN] Failed to open %s\n", entry->filename);
        return false;
    }

    fseek(file, 0, SEEK_END);
    u32 fileSize = ftell(file);
    rewind(file);

    void* buffer = linearAlloc(fileSize);
    if (!buffer) {
        fclose(file);
        log_to_file("[WARN] linearAlloc failed for %s (Size: %lu)\n", entry->filename, fileSize);
        return false;
    }
    
    linear_bytes_used += fileSize;
    entry->size = fileSize; 

    fread(buffer, 1, fileSize, file);
    fclose(file);

    CWAV* cwav = (CWAV*)calloc(1, sizeof(CWAV)); 
    if (!cwav) {
        linearFree(buffer);
        log_to_file("[WARN] malloc failed for CWAV struct %s\n", entry->filename);
        return false;
    }

    cwavLoad(cwav, buffer, maxSPlayList[index]);
    
    log_to_file("[Audio] Load '%s' status: %s | Buffer: %p | Struct: %p\n", 
                entry->filename, 
                cwavGetStatusString(cwav->loadStatus),
                buffer,
                cwav);

    if (cwav->loadStatus != CWAV_SUCCESS) {
        log_to_file("[WARN] Load Status Failed for %s\n", entry->filename);
        cwavFree(cwav);
        free(cwav);
        
        linearFree(buffer); 
        linear_bytes_used -= fileSize; 
        
        return false;
    }

    entry->cwav = cwav;
    entry->buffer = buffer;
    entry->loaded = true;

    return true;
}

void unloadAudioIndex(u32 index) {
    if (index >= FILE_COUNT) return;
    AudioEntry* entry = &audioList[index];
    if (!entry->loaded) return;

    if (entry->cwav) {
        cwavStop(entry->cwav, -1, -1);    
        svcSleepThread(60 * 1000 * 1000); 

        cwavFree(entry->cwav); 
        free(entry->cwav);
    }
    
    if (entry->buffer) {
        linearFree(entry->buffer); 
        linear_bytes_used -= entry->size;
    }

    entry->cwav = NULL;
    entry->buffer = NULL;
    entry->loaded = false;
    entry->size = 0;
}

void initAudioSystem(void) {
    for (u32 i = 0; i < FILE_COUNT; i++) {
        audioList[i].filename = fileList[i];
        audioList[i].cwav = NULL;
        audioList[i].buffer = NULL;
        audioList[i].loaded = false;
        audioList[i].size = 0; 
    }
    log_to_file("[INFO] Audio system initialized (%d entries)\n", FILE_COUNT);
    
}

void freeAllAudios(void) {
    for (u32 i = 0; i < FILE_COUNT; i++) {
        unloadAudioIndex(i);
    }
    log_to_file("[INFO] All Audios unloaded.\n");
}

CWAV* getAudio(u32 index) {
    if (index >= FILE_COUNT) return NULL;
    if (!audioList[index].loaded) loadAudioIndex(index);
    return audioList[index].cwav;
}

void playAudio(u32 index, bool stereo, float vol) {
    CWAV* cwav = getAudio(index);
    
    log_to_file("[Audio] Request Play Index: %d | PTR: %p\n", index, cwav);

    if (cwav) {
        
        if (cwav->loadStatus != CWAV_SUCCESS) {
             log_to_file("[ERR] Attempted to play invalid Audio (Status: %s)\n", cwavGetStatusString(cwav->loadStatus));
             return;
        }

        cwavPlayResult result;
        cwav->volume = vol;
        if (stereo)
            result = cwavPlay(cwav, 0, 1);
        else
            result = cwavPlay(cwav, 0, -1);
        
        log_to_file("[Audio] Play '%s' Result: %s (L:%d R:%d)\n", 
                    fileList[index], 
                    cwavGetStatusString(result.playStatus),
                    result.monoLeftChannel,
                    result.rightChannel);
        
    } else {
        log_to_file("[Audio] Play failed: Index %d returned NULL pointer.\n", index);
    }
}

void stopAudio(u32 index) {
    if (index >= FILE_COUNT) return;
    AudioEntry* entry = &audioList[index];
    
    if (!entry->loaded || !entry->cwav) {
        log_to_file("[Audio] Stop ignored: Index %d not loaded.\n", index);
        return;
    }

    cwavStop(entry->cwav, -1, -1);
    log_to_file("[Audio] Stopped '%s' | PTR: %p\n", entry->filename, entry->cwav);
}

char activeVoicePack[128] = "Default"; 
char activeVoicePackSource[16] = "romfs"; 

static AudioEntry vpAudioList[MAX_VP_FILES];

bool loadVoicePackAudio(const char* vpName, u32 step) {
    if (step >= MAX_VP_FILES) return false;
    
    AudioEntry* entry = &vpAudioList[step];
    if (entry->loaded) return true;

    char filepath[128];
    
    if (strcmp(activeVoicePackSource, "sd") == 0) {
        snprintf(filepath, sizeof(filepath), "/3ds/Koleo3DS/VP/%s/tutorial/tut_%lu.bcwav", vpName, step + 1);
    } else {
        snprintf(filepath, sizeof(filepath), "romfs:/vp/%s/tutorial/tut_%lu.bcwav", vpName, step + 1);
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        log_to_file("[WARN] Failed to open VP file %s\n", filepath);
        return false;
    }

    fseek(file, 0, SEEK_END);
    u32 fileSize = ftell(file);
    rewind(file);

    void* buffer = linearAlloc(fileSize);
    if (!buffer) {
        fclose(file);
        log_to_file("[WARN] VP linearAlloc failed for %s (Size: %lu)\n", filepath, fileSize);
        return false;
    }
    
    linear_bytes_used += fileSize;
    entry->size = fileSize; 

    fread(buffer, 1, fileSize, file);
    fclose(file);

    CWAV* cwav = (CWAV*)calloc(1, sizeof(CWAV)); 
    if (!cwav) {
        linearFree(buffer);
        return false;
    }

    cwavLoad(cwav, buffer, 1); 

    if (cwav->loadStatus != CWAV_SUCCESS) {
        log_to_file("[WARN] VP Load Status Failed for %s\n", filepath);
        cwavFree(cwav);
        free(cwav);
        linearFree(buffer); 
        linear_bytes_used -= fileSize; 
        return false;
    }

    entry->cwav = cwav;
    entry->buffer = buffer;
    entry->loaded = true;

    return true;
}

void playVoicePackAudio(u32 step, bool stereo) {
    if (step >= MAX_VP_FILES || !vpAudioList[step].loaded) return;
    
    CWAV* cwav = vpAudioList[step].cwav;
    if (cwav && cwav->loadStatus == CWAV_SUCCESS) {
        if (stereo) cwavPlay(cwav, 0, 1);
        else        cwavPlay(cwav, 0, -1);
    }
}

void stopVoicePackAudio(u32 step) {
    if (step >= MAX_VP_FILES || !vpAudioList[step].loaded || !vpAudioList[step].cwav) return;
    cwavStop(vpAudioList[step].cwav, -1, -1);
}

void unloadAllVoicePackAudio(void) {
    for (u32 i = 0; i < MAX_VP_FILES; i++) {
        AudioEntry* entry = &vpAudioList[i];
        if (!entry->loaded) continue;

        if (entry->cwav) {
            cwavStop(entry->cwav, -1, -1);    
            svcSleepThread(60 * 1000 * 1000); 
            cwavFree(entry->cwav); 
            free(entry->cwav);
        }
        
        if (entry->buffer) {
            linearFree(entry->buffer); 
            linear_bytes_used -= entry->size;
        }

        entry->cwav = NULL;
        entry->buffer = NULL;
        entry->loaded = false;
        entry->size = 0;
    }
}

static AudioEntry navAudioList[MAX_NAV_FILES];
const char* defaultNavFiles[] = {
    "bilety.bcwav",
    "blik.bcwav",
    "email.bcwav",
    "haslo.bcwav",
    "menu_g.bcwav",
    "miejsca_r.bcwav",
    "opcje.bcwav",
    "polaczenie_b.bcwav",
    "polaczenie_p.bcwav",
    "stacja_k.bcwav",
    "stacja_p.bcwav",
    "stan_konta.bcwav",
    "szukam.bcwav",
    "szybka_r.bcwav"
};

bool loadNavVoiceLines(const char* vpName) {
    
    unloadAllNavAudio();

    bool success = false;
    for (int i = 0; i < MAX_NAV_FILES; i++) {
        char path[256];

        if (strcmp(activeVoicePackSource, "sd") == 0)
            snprintf(path, sizeof(path),
                    "/3ds/Koleo3DS/VP/%s/nav/%s",
                    vpName, defaultNavFiles[i]);
        else
            snprintf(path, sizeof(path),
                    "romfs:/vp/%s/nav/%s",
                    vpName, defaultNavFiles[i]);

        FILE* file = fopen(path, "rb");
        if (!file) {
            log_to_file("[WARN] VP Nav file not found: %s\n", path);
            continue; 
        }

        fseek(file, 0, SEEK_END);
        u32 fileSize = ftell(file);
        rewind(file);

        void* buffer = linearAlloc(fileSize);
        if (!buffer) {
            fclose(file);
            continue;
        }

        linear_bytes_used += fileSize;
        fread(buffer, 1, fileSize, file);
        fclose(file);

        CWAV* cwav = (CWAV*)calloc(1, sizeof(CWAV));
        if (cwav) {
            cwavLoad(cwav, buffer, 1);
            if (cwav->loadStatus == CWAV_SUCCESS) {
                navAudioList[i].cwav = cwav;
                navAudioList[i].buffer = buffer;
                navAudioList[i].size = fileSize;
                navAudioList[i].loaded = true;
                success = true;
            } else {
                cwavFree(cwav);
                free(cwav);
                linearFree(buffer);
                linear_bytes_used -= fileSize;
            }
        }
    }
    return success;
}

void playNavAudio(u32 index, bool stereo) {
    if (index >= MAX_NAV_FILES || !navAudioList[index].loaded) return;
    
    CWAV* cwav = navAudioList[index].cwav;
    if (cwav && cwav->loadStatus == CWAV_SUCCESS) {
        if (stereo) cwavPlay(cwav, 0, 1);
        else        cwavPlay(cwav, 0, -1);
    }
}

void unloadAllNavAudio(void) {
    for (u32 i = 0; i < MAX_NAV_FILES; i++) {
        AudioEntry* entry = &navAudioList[i];
        if (!entry->loaded) continue;

        if (entry->cwav) {
            cwavStop(entry->cwav, -1, -1);    
            svcSleepThread(60 * 1000 * 1000); 
            cwavFree(entry->cwav); 
            free(entry->cwav);
        }
        
        if (entry->buffer) {
            linearFree(entry->buffer); 
            linear_bytes_used -= entry->size;
        }

        entry->cwav = NULL;
        entry->buffer = NULL;
        entry->loaded = false;
        entry->size = 0;
    }
}

