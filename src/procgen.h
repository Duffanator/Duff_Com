#ifndef PROCGEN_H
#define PROCGEN_H

typedef struct {
    int squad_x[8];
    int squad_y[8];
    int squad_dir;
    int bus_x0, bus_y0, bus_x1, bus_y1;
    int bus_turret_tx, bus_turret_ty;
    int bus_drone_pad_x, bus_drone_pad_y;

    int num_aliens;

    int alien_type[16]; /* 1 = Sectoid, 2 = Floater, 3 = Muton, 4 = Cryptid */
    int alien_rank[16]; /* 0 = Soldier, 1 = Warrior, 2 = Leader, 3 = Sectoid Commander */
    int alien_x[16];
    int alien_y[16];
    int alien_z[16];
    int alien_dir[16];
    const char *alien_name[16];

    int num_civilians;
    int civ_gender[8]; /* 0..2 = Male Civilian, 3..5 = Female Civilian */
    int civ_x[8];
    int civ_y[8];
    int civ_z[8];
    int civ_dir[8];
    const char *civ_name[8];
} ProcSpawnPlan;

/* Procedurally generate a thematic randomized battleground for `biome_id`
 * and populate `out_plan` (if non-NULL) with verified reachable unit spawns. */
void procgen_build_battlefield(int biome_id, ProcSpawnPlan *out_plan);

#endif /* PROCGEN_H */
