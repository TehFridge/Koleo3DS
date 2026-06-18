#include "sprites.h"
#include "main.h"


extern size_t linear_bytes_used;



GFX_IMAGE* test;
GFX_IMAGE* kafelek1; 
GFX_IMAGE* kafelek2; 
GFX_IMAGE* kafelek1_alt;

GFX_IMAGE* logo_1; 
GFX_IMAGE* logo_2;

GFX_IMAGE* koleo_logo;

GFX_IMAGE* p_poczatek; 
GFX_IMAGE* p_srodek; 
GFX_IMAGE* p_koniec;

GFX_IMAGE* chan_excited;
GFX_IMAGE* chan_idle_speak;
GFX_IMAGE* chan_idle;
GFX_IMAGE* chan_diss_speak;
GFX_IMAGE* chan_diss;


GFX_IMAGE* kun_excited;
GFX_IMAGE* kun_idle_speak;
GFX_IMAGE* kun_idle;
GFX_IMAGE* kun_diss_speak;
GFX_IMAGE* kun_diss;

GFX_IMAGE* siedzenie;
GFX_IMAGE* siedzenie_zajete;
GFX_IMAGE* siedzenie_puste;

GFX_IMAGE* siedzenie_wozek;
GFX_IMAGE* siedzenie_wozek_zajete;
GFX_IMAGE* siedzenie_wozek_puste;

GFX_IMAGE* siedzenie_caretaker;
GFX_IMAGE* siedzenie_caretaker_zajete;
GFX_IMAGE* siedzenie_caretaker_puste;

GFX_IMAGE* top1;
GFX_IMAGE* top2;
GFX_IMAGE* top3;
GFX_IMAGE* top4;
GFX_IMAGE* top5;
GFX_IMAGE* top6;

GFX_IMAGE* bot1;
GFX_IMAGE* bot2;
GFX_IMAGE* bot3;
GFX_IMAGE* bot4;
GFX_IMAGE* bot5;
GFX_IMAGE* bot6;
GFX_IMAGE* bot7;
GFX_IMAGE* bot8;
GFX_IMAGE* bot9;
GFX_IMAGE* bot10;
GFX_IMAGE* bot11;
GFX_IMAGE* bot12;

u64 lastFrameTime = 0;
int currentFrame = 0;


void spritesInit() {
    kafelek1 = GFX_LoadTexture("romfs:/gfx/kafelek.t3x", 0);
    kafelek2 = GFX_LoadTexture("romfs:/gfx/kafelek.t3x", 2);
    kafelek1_alt = GFX_LoadTexture("romfs:/gfx/kafelek.t3x", 1);

    logo_1 = GFX_LoadTexture("romfs:/gfx/logo.t3x", 0);
    logo_2 = GFX_LoadTexture("romfs:/gfx/logo.t3x", 1);

    koleo_logo = GFX_LoadTexture("romfs:/gfx/logo_koleo.t3x", 0);

    p_poczatek = GFX_LoadTexture("romfs:/gfx/pociag.t3x", 0);
    p_srodek = GFX_LoadTexture("romfs:/gfx/pociag.t3x", 1);
    p_koniec = GFX_LoadTexture("romfs:/gfx/pociag.t3x", 2);

    siedzenie_puste = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 0);
    siedzenie_zajete = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 1);
    siedzenie = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 2);

    siedzenie_wozek_puste = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 3);
    siedzenie_wozek_zajete = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 4);
    siedzenie_wozek = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 5);

    siedzenie_caretaker_puste = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 6);
    siedzenie_caretaker_zajete = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 7);
    siedzenie_caretaker = GFX_LoadTexture("romfs:/gfx/siedzenia.t3x", 8);
    
    chan_excited = GFX_LoadTexture("romfs:/gfx/chan.t3x", 0);
    chan_idle_speak = GFX_LoadTexture("romfs:/gfx/chan.t3x", 1);
    chan_idle = GFX_LoadTexture("romfs:/gfx/chan.t3x", 2);
    chan_diss_speak = GFX_LoadTexture("romfs:/gfx/chan.t3x", 3);
    chan_diss = GFX_LoadTexture("romfs:/gfx/chan.t3x", 4);

    kun_excited = GFX_LoadTexture("romfs:/gfx/kun.t3x", 0);
    kun_idle_speak = GFX_LoadTexture("romfs:/gfx/kun.t3x", 1);
    kun_idle = GFX_LoadTexture("romfs:/gfx/kun.t3x", 2);
    kun_diss_speak = GFX_LoadTexture("romfs:/gfx/kun.t3x", 3);
    kun_diss = GFX_LoadTexture("romfs:/gfx/kun.t3x", 4);

    top1 = GFX_LoadTexture("romfs:/gfx/tutorial_top.t3x", 0);
    top2 = GFX_LoadTexture("romfs:/gfx/tutorial_top.t3x", 1);
    top3 = GFX_LoadTexture("romfs:/gfx/tutorial_top.t3x", 2);
    top4 = GFX_LoadTexture("romfs:/gfx/tutorial_top.t3x", 3);
    top5 = GFX_LoadTexture("romfs:/gfx/tutorial_top.t3x", 4);
    top6 = GFX_LoadTexture("romfs:/gfx/tutorial_top.t3x", 5);

    bot1 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 0);
    bot2 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 1);
    bot3 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 2);
    bot4 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 3);
    bot5 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 4);
    bot6 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 5);
    bot7 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 6);
    bot8 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 7);
    bot9 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 8);
    bot10 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 9);
    bot11 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 10);
    bot12 = GFX_LoadTexture("romfs:/gfx/tutorial_bottom.t3x", 11);
}