#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

#define SFX_SAMPLE_RATE 11025
#define SFX_DMA_MAX     24576  /* Max 24 KB per sound (~2.23 sec at 11,025 Hz, 64KB-DMA-page safe) */

enum {
    SFX_ID_CLICK = 0,
    SFX_ID_STEP_A,     /* Left tactical boot step */
    SFX_ID_STEP_B,     /* Right tactical boot step */
    SFX_ID_DOOR,       /* Hydraulic / swinging door */
    SFX_ID_RIFLE,      /* Ballistic Assault Rifle gunshot */
    SFX_ID_LASER,      /* Coopersburg Prototype Laser Rifle */
    SFX_ID_PLASMA,     /* Heavy Plasma / Alien Plasma discharge */
    SFX_ID_EXPLODE,    /* HE Grenade / Fuel Drum concussion blast */
    SFX_ID_GLASS,       /* Shattering Diner / Brick window */
    SFX_ID_HIT,         /* Bullet / plasma impact on armor or cover */
    SFX_ID_ALIEN_DIE,   /* Sectoid high-pitched alien death shriek */
    SFX_ID_FLOATER_DIE, /* Floater cybernetic shriek & thruster failure */
    SFX_ID_MUTON_DIE,   /* Muton deep guttural death roar */
    SFX_ID_MAN_DIE,     /* Male DUFF-COM operative death scream */
    SFX_ID_FEM_DIE,     /* Female DUFF-COM operative death scream */
    SFX_ID_ALERT,       /* Alien spotted / Reaction Fire / Low TU alert */
    SFX_COUNT
};

/* Initialize Sound Blaster DSP + ISA DMA & load external assets/*.WAV (with procedural fallbacks) */
void sound_init(void);
void sound_shutdown(void);
int  sound_is_sb_active(void);

/* Play a digital sound effect asynchronously via Sound Blaster DMA (or PC Speaker fallback) */
void sound_play(int sfx_id);
void sound_play_weapon(int weapon_id);
void sound_play_death(int faction, int unit_type, int gender);

/* Convenience tactical audio helpers */
void sfx_beep(int freq_hz, int duration_ms);
void sfx_click(void);
void sfx_step(void);
void sfx_door(void);
void sfx_rifle(void);
void sfx_laser(void);
void sfx_plasma(void);
void sfx_explosion(void);
void sfx_glass(void);
void sfx_hit(void);
void sfx_alien_die(void);
void sfx_man_die(void);
void sfx_fem_die(void);
void sfx_alert(void);

#endif /* SOUND_H */
