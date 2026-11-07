#ifndef PROCGEN_H
#define PROCGEN_H

typedef struct {
    int squad_x[5];
    int squad_y[5];
    int squad_dir;

    int num_aliens;
    int alien_type[8]; /* 1 = Sectoid, 2 = Floater, 3 = Muton */
    int alien_x[8];
    int alien_y[8];
    int alien_z[8];
    int alien_dir[8];
    const char *alien_name[8];
} ProcSpawnPlan;

/* Procedurally generate a thematic randomized battleground for `biome_id`
 * and populate `out_plan` (if non-NULL) with verified reachable unit spawns. */
void procgen_build_battlefield(int biome_id, ProcSpawnPlan *out_plan);

#endif /* PROCGEN_H */
