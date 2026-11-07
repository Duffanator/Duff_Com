#ifndef MUSIC_H
#define MUSIC_H

enum {
    MUSIC_TRACK_NONE  = 0,
    MUSIC_TRACK_TITLE = 1,  /* 90s Industrial Moody Metal (Nine Inch Nails style) */
    MUSIC_TRACK_GAME  = 2   /* Slow Moody Atmospheric Sci-Fi (Gameplay / HQ & Battlescape) */
};

void music_init(void);
void music_shutdown(void);
int  music_is_opl_active(void);

void music_play_track(int track_id);
void music_set_volume(int vol_level); /* 0=OFF, 1=LOW, 2=MED, 3=HIGH */

/* Call once per 70Hz VGA vertical retrace (vga_vsync) */
void music_tick(void);

#endif /* MUSIC_H */
