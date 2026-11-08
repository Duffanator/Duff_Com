#include "pathfind.h"
#include "isomap.h"
#include "gfx_assets.h"
#include <stdlib.h>
#include <string.h>

#define TOTAL_NODES (MAP_LEVS * MAP_H * MAP_W)
#define NODE_IDX(x, y, z) (((z) * MAP_H + (y)) * MAP_W + (x))

/* Check if a wall blocks movement across an orthogonal edge; returns 0=blocked, 1=open, 2=closed door */
static int check_ortho_edge(int x0, int y0, int z, int x1, int y1) {
    uint8_t w = WALL_NONE;
    if (x1 > x0) {
        w = g_map[z][y0][x1].wall_west;
    } else if (x1 < x0) {
        w = g_map[z][y0][x0].wall_west;
    } else if (y1 > y0) {
        w = g_map[z][y1][x0].wall_north;
    } else if (y1 < y0) {
        w = g_map[z][y0][x0].wall_north;
    }
    if (isomap_is_door_closed(w)) return 2;
    if (!isomap_wall_blocks_move(w)) return 1;
    return 0;
}

static int is_walkable_obj(uint8_t obj_id) {
    return (obj_id == OBJ_NONE || isomap_is_lift_obj(obj_id) || obj_id == OBJ_RUBBLE);
}

int pathfind_can_step(int x0, int y0, int z0, int x1, int y1, int z1, int can_fly, int *out_cost) {
    MapCell *dst;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int dz = z1 - z0;
    int cost = 4;

    if ((unsigned)x1 >= MAP_W || (unsigned)y1 >= MAP_H || (unsigned)z1 >= MAP_LEVS) return 0;
    dst = &g_map[z1][y1][x1];

    /* Destination cannot already be occupied by another unit or blocking prop */
    if (dst->unit_idx != -1) return 0;
    if (!is_walkable_obj(dst->obj_id)) {
        return 0;
    }

    /* Vertical transition: check staircases & UFO grav-lifts FIRST so ground & flying units can use them! */
    if (dz != 0) {
        if (dx != 0 || dy != 0 || abs(dz) != 1) return 0;
        if (dz == 1 && isomap_is_lift_obj(g_map[z0][y0][x0].obj_id)) {
            if (out_cost) *out_cost = can_fly ? 4 : 6;
            return 1;
        }
        if (dz == -1 && isomap_is_lift_obj(g_map[z1][y1][x1].obj_id)) {
            if (out_cost) *out_cost = can_fly ? 4 : 6;
            return 1;
        }
        if (can_fly) {
            /* Flying units can ascend/descend in open air if no solid floor/ceiling blocks the transition */
            int upper_z = (z1 > z0) ? z1 : z0;
            if (g_map[upper_z][y0][x0].floor_id != FLOOR_NONE) return 0;
            if (out_cost) *out_cost = 4;
            return 1;
        }
        return 0;
    }

    /* Horizontal move on same level z0 */
    if (!can_fly) {
        int has_stair_below = (z1 > 0 && isomap_is_lift_obj(g_map[z1 - 1][y1][x1].obj_id));
        if (dst->floor_id == FLOOR_NONE && !has_stair_below) return 0;
    }

    /* Check blocking props on destination tile */
    if (!is_walkable_obj(dst->obj_id)) {
        return 0;
    }

    if (dx != 0 && dy != 0) {
        /* Diagonal step: prevent corner-clipping through walls or pinched corner hull props */
        int e1 = check_ortho_edge(x0, y0, z0, x1, y0);
        int e2 = check_ortho_edge(x0, y0, z0, x0, y1);
        int e3 = check_ortho_edge(x1, y0, z0, x1, y1);
        int e4 = check_ortho_edge(x0, y1, z0, x1, y1);
        int a_open = is_walkable_obj(g_map[z0][y0][x1].obj_id);
        int b_open = is_walkable_obj(g_map[z0][y1][x0].obj_id);
        if (!e1 || !e2 || !e3 || !e4) return 0;
        if (!a_open && !b_open) return 0;
        cost = 6;
        if (e1 == 2 || e2 == 2 || e3 == 2 || e4 == 2) cost += 2;
    } else {
        int e = check_ortho_edge(x0, y0, z0, x1, y1);
        if (!e) return 0;
        cost = 4;
        if (e == 2) cost += 2; /* +2 TUs to swing open closed door */
    }

    if (dst->obj_id == OBJ_RUBBLE) cost += 2;
    if (out_cost) *out_cost = cost;
    return 1;
}

int pathfind_compute(int sx, int sy, int sz, int gx, int gy, int gz, int can_fly, PathResult *out_path) {
    static uint16_t g_score[TOTAL_NODES];
    static uint16_t f_score[TOTAL_NODES];
    static int16_t  came_from[TOTAL_NODES];
    static uint8_t  step_cost_arr[TOTAL_NODES];
    static uint8_t  state[TOTAL_NODES]; /* 0=unvisited, 1=open, 2=closed */
    static int16_t  open_list[TOTAL_NODES];
    int open_count = 0;
    int start_idx, goal_idx;

    memset(out_path, 0, sizeof(*out_path));
    if ((unsigned)sx >= MAP_W || (unsigned)sy >= MAP_H || (unsigned)sz >= MAP_LEVS) return 0;
    if ((unsigned)gx >= MAP_W || (unsigned)gy >= MAP_H || (unsigned)gz >= MAP_LEVS) return 0;
    if (sx == gx && sy == gy && sz == gz) return 0;

    memset(state, 0, sizeof(state));
    start_idx = NODE_IDX(sx, sy, sz);
    goal_idx  = NODE_IDX(gx, gy, gz);

    g_score[start_idx]   = 0;
    f_score[start_idx]   = (uint16_t)((abs(gx - sx) + abs(gy - sy)) * 4 + abs(gz - sz) * 6);
    came_from[start_idx] = -1;
    state[start_idx]     = 1;
    open_list[open_count++] = (int16_t)start_idx;

    while (open_count > 0) {
        /* Pop node in open_list with lowest f_score */
        int best_i = 0;
        int cur = open_list[0];
        int cx, cy, cz, i, dir;

        for (i = 1; i < open_count; i++) {
            int n = open_list[i];
            if (f_score[n] < f_score[cur]) {
                cur = n;
                best_i = i;
            }
        }
        open_list[best_i] = open_list[--open_count];
        state[cur] = 2;

        if (cur == goal_idx) {
            /* Reconstruct path back-to-front */
            int temp_nodes[MAX_PATH_LEN];
            int count = 0;
            int trace = goal_idx;
            while (trace != start_idx && trace >= 0 && count < MAX_PATH_LEN) {
                temp_nodes[count++] = trace;
                trace = came_from[trace];
            }
            if (trace != start_idx) return 0;

            out_path->valid = 1;
            out_path->length = count;
            for (i = 0; i < count; i++) {
                int node = temp_nodes[count - 1 - i];
                int nx = node % MAP_W;
                int ny = (node / MAP_W) % MAP_H;
                int nz = node / (MAP_W * MAP_H);
                out_path->steps[i].x = (int8_t)nx;
                out_path->steps[i].y = (int8_t)ny;
                out_path->steps[i].z = (int8_t)nz;
                out_path->steps[i].tu_cost = step_cost_arr[node];
                out_path->steps[i].total_tu = g_score[node];
            }
            return 1;
        }

        cx = cur % MAP_W;
        cy = (cur / MAP_W) % MAP_H;
        cz = cur / (MAP_W * MAP_H);

        /* Explore 8 horizontal neighbors + 2 vertical (up/down) neighbors */
        for (dir = 0; dir < 10; dir++) {
            static const int dx_tab[10] = { 0,  1,  1,  1,  0, -1, -1, -1,  0,  0};
            static const int dy_tab[10] = {-1, -1,  0,  1,  1,  1,  0, -1,  0,  0};
            static const int dz_tab[10] = { 0,  0,  0,  0,  0,  0,  0,  0,  1, -1};
            int nx = cx + dx_tab[dir];
            int ny = cy + dy_tab[dir];
            int nz = cz + dz_tab[dir];
            int scost = 0;
            int nidx;
            uint16_t tentative_g;

            if (!pathfind_can_step(cx, cy, cz, nx, ny, nz, can_fly, &scost)) continue;
            nidx = NODE_IDX(nx, ny, nz);
            if (state[nidx] == 2) continue;

            tentative_g = (uint16_t)(g_score[cur] + scost);
            if (state[nidx] == 0 || tentative_g < g_score[nidx]) {
                int h = (abs(gx - nx) + abs(gy - ny)) * 4 + abs(gz - nz) * 6;
                came_from[nidx]     = (int16_t)cur;
                step_cost_arr[nidx] = (uint8_t)scost;
                g_score[nidx]       = tentative_g;
                f_score[nidx]       = (uint16_t)(tentative_g + h);
                if (state[nidx] == 0) {
                    state[nidx] = 1;
                    open_list[open_count++] = (int16_t)nidx;
                }
            }
        }
    }

    return 0;
}
