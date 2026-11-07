#ifndef PATHFIND_H
#define PATHFIND_H

#include <stdint.h>

#define MAX_PATH_LEN 64

typedef struct {
    int8_t x, y, z;
    uint8_t tu_cost;    /* Cost of this individual step */
    uint16_t total_tu;  /* Cumulative TU cost from start */
} PathStep;

typedef struct {
    int valid;
    int length;         /* Number of steps (0 if start == goal or unreachable) */
    PathStep steps[MAX_PATH_LEN];
} PathResult;

/* Check if a single step from (x0,y0,z0) to adjacent (x1,y1,z1) is legal; outputs TU cost */
int pathfind_can_step(int x0, int y0, int z0, int x1, int y1, int z1, int can_fly, int *out_cost);

/* Compute shortest 3D A* path from (sx,sy,sz) to (gx,gy,gz) */
int pathfind_compute(int sx, int sy, int sz, int gx, int gy, int gz, int can_fly, PathResult *out_path);

#endif /* PATHFIND_H */
