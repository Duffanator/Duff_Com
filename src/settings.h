#ifndef SETTINGS_H
#define SETTINGS_H

enum {
    ANIM_SLOW   = 0,
    ANIM_MEDIUM = 1,  /* Default out-of-the-box animation speed */
    ANIM_FAST   = 2
};

typedef struct {
    int anim_speed;    /* 0 = SLOW, 1 = MEDIUM (default), 2 = FAST */
    int music_vol;     /* 0 = OFF,  1 = LOW,    2 = MEDIUM, 3 = HIGH */
    int sfx_enabled;   /* 0 = OFF,  1 = ON */
    int scroll_speed;  /* 0 = SLOW, 1 = NORMAL, 2 = FAST */
    int menu_open;     /* 1 = In-game Options overlay active */
} GameSettings;

extern GameSettings g_settings;

void settings_init(void);
void settings_save(void);

/* Render & handle input for the modal In-Game Settings Menu.
 * Returns: 0 = normal/continue, 1 = Abort Mission to HQ, 2 = Quit Game to DOS */
void settings_render_modal(int in_battlescape);
int  settings_handle_input(int in_battlescape);

/* Wait `frames` 70Hz VGA retraces while keeping background OPL2 music ticking smoothly */
void game_wait_frames(int frames);

/* Animation & camera timing accessors scaled by g_settings.anim_speed */
int settings_walk_frames(void);
int settings_bolt_frames(void);
int settings_muzzle_frames(void);
int settings_impact_step_frames(void);
int settings_burst_gap_frames(void);
int settings_grenade_step_frames(void);
int settings_explode_step_frames(void);
int settings_death_step_frames(void);
int settings_alien_step_frames(void);
int settings_alien_post_shot_frames(void);
int settings_hidden_banner_frames(void);
int settings_turn_splash_frames(void);
int settings_scroll_px(void);

#endif /* SETTINGS_H */
