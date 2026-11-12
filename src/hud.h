#ifndef HUD_UI_H
#define HUD_UI_H

void hud_render(void);
int  hud_handle_click(int mx, int my, int right_click);
void hud_update_inventory(void);
void hud_inv_scroll(int delta_rows);

#endif /* HUD_UI_H */
