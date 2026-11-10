#ifndef BASE_UI_H
#define BASE_UI_H

/* Phase 3: Coopersburg Colonial House Base Overview, Construction,
 * Limited Equipment Stores, ARV Cargo Transfer, Buy/Sell Store,
 * Research Lab, Research Log & Workshop Manufacturing UI */

void base_ui_render(void);
void base_ui_handle_input(void);

/* Phase 4: Single-Operative Squad Armory, ARV Cargo Equip & KIA Memorial Wall */
void armory_ui_render(void);
void armory_ui_handle_input(void);

/* Check whether a given (r, c) lot is orthogonally connected to an existing base tile */
int  base_ui_is_lot_connected(int r, int c);


#endif /* BASE_UI_H */
