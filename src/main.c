#include "dos_compat.h"
#include "vga.h"
#include "input.h"
#include "sound.h"
#include "music.h"
#include "settings.h"
#include "gfx_assets.h"
#include "isomap.h"
#include "hud.h"
#include "campaign.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __DJGPP__
#define GDB_IMPLEMENTATION
#include "gdbstub.h"
#else
#define gdb_start()
#define gdb_checkpoint()
#endif

static void draw_title_banner(void) {
    if (gfx_title_bg) {
        /* Render full-screen 320x200 1994 VGA pixel-art Title Screen! */
        memcpy(vga_backbuf, gfx_title_bg, SCREEN_W * SCREEN_H);
        vga_rect_bevel(26, 6, 268, 28, 14, 96, 79);
        vga_print_shadow(56, 11, "DUFF-COM: COOPERSBURG PA", 96, 15);
        vga_print_shadow(62, 21, "LEHIGH VALLEY DEFENSE FORCE", 80, 15);

        vga_rect_bevel(44, 176, 232, 18, 14, 96, 79);
        vga_print_shadow(58, 182, "CLICK OR PRESS ANY BUTTON!", 96, 15);
        vga_flip();
        return;
    }

    vga_clear(15);
    vga_rect_bevel(16, 14, 288, 172, 76, 80, 79);
    vga_rect_fill(22, 20, 276, 160, 14);

    vga_print_shadow(56, 30, "DUFF-COM: COOPERSBURG PA", 96, 15);
    vga_print_shadow(62, 42, "LEHIGH VALLEY DEFENSE FORCE", 80, 15);

    vga_hline(32, 288, 54, 66);

    vga_print(32, 62,  "4 LOCAL PENNSYLVANIA COMBAT SECTORS:", 96);
    vga_print(38, 73,  "- COOPERSBURG RTE 309 DINER & ARCADE", 18);
    vga_print(38, 83,  "- QUAKERTOWN FARMLAND & RED BARN", 18);
    vga_print(38, 93,  "- SUBURBAN HOUSE & ARCADE BASEMENT", 18);
    vga_print(38, 103, "- BETHLEHEM STEEL INDUSTRIAL RUINS", 18);

    vga_hline(32, 288, 115, 66);

    vga_print(32, 122, "TRACKBALL: MOVE CURSOR  L-CLICK: ACT/WALK", 2);
    vga_print(32, 132, "R-CLICK: FACE / OPEN DOOR   1-5: WEAPONS", 2);
    vga_print(32, 142, "ESC / OPT: OPTIONS MENU   M: RETURN TO HQ", 2);

    vga_print_shadow(58, 160, "CLICK OR PRESS ANY BUTTON!", 96, 15);
    vga_flip();
}

int main(int argc, char **argv) {
    int running = 1;
    int snap_mode = 0;
    int scroll_timer = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-snap") == 0 || strcmp(argv[i], "--snap") == 0) {
            snap_mode = 1;
        }
    }

    if (!snap_mode) {
        srand((unsigned)time(NULL));
    }

    gdb_start();

    vga_init();
    settings_init();
    gfx_init_assets();
    sound_init();
    music_init();
    music_set_volume(g_settings.music_vol);
    input_init();
    isomap_init();
    campaign_init();

    if (snap_mode) {
        isomap_render();
        hud_render();
        input_draw_cursor();
        vga_flip();
        vga_save_bmp("SNAP.BMP");
        music_shutdown();
        sound_shutdown();
        vga_shutdown();
        return 0;
    }

    /* Show arcade attract/title briefing with 90s Industrial Moody Metal (NIN style)! */
    music_play_track(MUSIC_TRACK_TITLE);
    draw_title_banner();
    for (;;) {
        input_poll();
        if (g_input.left_clicked || g_input.right_clicked || g_input.last_key != 0) {
            sfx_click();
            break;
        }
        vga_vsync();
        music_tick();
    }

    /* Transition to Slow Moody Sci-Fi Background Music for HQ & Battlescape! */
    music_play_track(MUSIC_TRACK_GAME);

    while (running) {
        input_poll();

        /* MODE 0: Lehigh Valley Command Center (Radar Map / Bunker Base / Armory) */
        if (g_camp.mode == 0) {
            if (g_settings.menu_open) {
                if (settings_handle_input(0) == 2) {
                    running = 0;
                    break;
                }
            } else if (!g_camp.launch_prompt_open &&
                       (g_input.last_key == KEY_ESC ||
                        g_input.last_key == 'o' || g_input.last_key == 'O')) {
                g_settings.menu_open = 1;
                sfx_click();
            } else {
                campaign_handle_input();
                if (g_camp.mode != 0) {
                    continue;
                }
                campaign_tick();
            }
            campaign_render();
            if (g_settings.menu_open) {
                settings_render_modal(0);
            }
            input_draw_cursor();
            vga_vsync();
            music_tick();
            vga_flip();
            gdb_checkpoint();
            continue;
        }

        /* MODE 1: Tactical Battlescape — Handle Modal Options Menu when Open */
        if (g_settings.menu_open) {
            scroll_timer = 0;
            if (settings_handle_input(1) == 1) {
                g_settings.menu_open = 0;
                campaign_on_mission_complete();
                continue;
            }
            isomap_render();
            hud_render();
            if (g_settings.menu_open) {
                settings_render_modal(1);
            }
            input_draw_cursor();
            vga_vsync();
            music_tick();
            vga_flip();
            gdb_checkpoint();
            continue;
        }

        if (g_battle.mission_ended != 0) {
            if (g_input.left_clicked || g_input.last_key == KEY_ENTER ||
                g_input.last_key == ' ' || g_input.last_key == KEY_ESC) {
                sfx_click();
                campaign_on_mission_complete();
                continue;
            }
        }

        /* Edge-of-screen scrolling with hover delay */
        {
            int edge_dir = 0;
            if (g_input.my < HUD_Y - 4) {
                if (g_input.mx <= 3)            edge_dir |= 1;
                if (g_input.mx >= SCREEN_W - 4) edge_dir |= 2;
                if (g_input.my <= 3)            edge_dir |= 4;
            }
            if (g_input.my >= SCREEN_H - 2 ||
                (g_input.my >= VIEWPORT_H - 4 && g_input.my < HUD_Y)) {
                edge_dir |= 8;
            }

            if (edge_dir != 0) {
                int required_delay = (edge_dir & 8) ? 20 : 10;
                int spx = settings_scroll_px();
                scroll_timer++;
                if (scroll_timer >= required_delay) {
                    if (edge_dir & 1) g_battle.cam_x += spx;
                    if (edge_dir & 2) g_battle.cam_x -= spx;
                    if (edge_dir & 4) g_battle.cam_y += spx;
                    if (edge_dir & 8) g_battle.cam_y -= spx;
                }
            } else {
                scroll_timer = 0;
            }
        }

        if (g_input.my < HUD_Y && !g_battle.weapon_menu_open) {
            isomap_screen_to_tile(g_input.mx, g_input.my, g_battle.view_level,
                                  &g_battle.cur_x, &g_battle.cur_y);
        }

        isomap_update_walk();
        isomap_update_path_preview();

        if (g_battle.mission_ended == 0) {
            if (g_input.left_clicked) {
                if (g_battle.weapon_menu_open || g_input.my >= HUD_Y) {
                    hud_handle_click(g_input.mx, g_input.my, 0);
                } else if (g_battle.fire_mode > 0) {
                    isomap_fire_weapon(g_battle.cur_x, g_battle.cur_y);
                } else {
                    isomap_move_or_interact(g_battle.cur_x, g_battle.cur_y);
                }
            } else if (g_input.right_clicked) {
                if (g_battle.weapon_menu_open) {
                    g_battle.weapon_menu_open = 0;
                    sfx_click();
                } else if (g_input.my >= HUD_Y) {
                    hud_handle_click(g_input.mx, g_input.my, 1);
                } else if (g_battle.fire_mode > 0) {
                    g_battle.fire_mode = 0;
                    sfx_click();
                } else {
                    Unit *u = &g_units[g_battle.selected_unit];
                    isomap_turn_unit_toward(u, g_battle.cur_x, g_battle.cur_y);
                }
            }
        }

        /* Keyboard / Arcade Cabinet Button Hotkeys */
        switch (g_input.last_key) {
            case KEY_ESC:
            case 'o':
            case 'O':
                g_battle.weapon_menu_open = 0;
                g_settings.menu_open = 1;
                sfx_click();
                break;
            case 'm':
            case 'M':
                campaign_on_mission_complete();
                sfx_click();
                break;
            case KEY_LEFT:  g_battle.cam_x += 12; break;
            case KEY_RIGHT: g_battle.cam_x -= 12; break;
            case KEY_UP:    g_battle.cam_y += 12; break;
            case KEY_DOWN:  g_battle.cam_y -= 12; break;
            case KEY_TAB:
            case ' ':
                isomap_select_next_soldier();
                break;
            case 'k':
            case 'K':
                isomap_toggle_kneel();
                break;
            case 'r':
            case 'R':
                g_battle.roof_cutaway = !g_battle.roof_cutaway;
                sfx_click();
                break;
            case 'n':
            case 'N':
                g_battle.night_mode = !g_battle.night_mode;
                isomap_recalc_lighting_and_fov();
                sfx_click();
                break;
            case 'f':
            case 'F':
                g_battle.weapon_menu_open = !g_battle.weapon_menu_open;
                sfx_click();
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
                g_battle.fire_mode = g_input.last_key - '0';
                g_battle.weapon_menu_open = 0;
                sfx_click();
                break;
            case KEY_PGUP:
            case 'u':
            case 'U':
                if (g_battle.view_level < MAP_LEVS - 1) g_battle.view_level++;
                sfx_click();
                break;
            case KEY_PGDN:
            case 'd':
            case 'D':
                if (g_battle.view_level > 0) g_battle.view_level--;
                sfx_click();
                break;
            case 'c':
            case 'C': {
                Unit *u = &g_units[g_battle.selected_unit];
                isomap_center_camera_on(u->x, u->y, u->z);
                break;
            }
            case KEY_ENTER:
                isomap_end_turn();
                break;
            case 'p':
            case 'P':
                vga_save_bmp("SNAP.BMP");
                sfx_click();
                break;
        }

        isomap_render();
        hud_render();
        if (g_settings.menu_open) {
            settings_render_modal(1);
            input_draw_cursor();
        } else {
            input_draw_cursor_ex(isomap_get_cursor_mode());
        }

        vga_vsync();
        music_tick();
        vga_flip();
        gdb_checkpoint();
    }

    music_shutdown();
    sound_shutdown();
    vga_shutdown();
    return 0;
}
