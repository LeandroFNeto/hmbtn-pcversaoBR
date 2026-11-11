/* Writes the loaded scene's floor polygons, zone boxes, and model slots.
 * Screen points come from gte_project. The ground probe and RotTransPers are not used. */

#include <gte.h>
#include <inline_c.h>
#include <libgte.h>
#include <map_exporter.h>

#include "farm_data_size.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "field.h"
#include "model.h"
#include "scene.h"

#include <SDL.h>

#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_DIR "debug/maps"
#define NAME_MAX_COPY 31
#define FILTER_MAX 64
#define WRITTEN_MAX 256
#define SELFTEST_SCENE 0x7FE
#define FARM_GRID_SCENE 0x7FD
#define GREENHOUSE_GRID_SCENE 0x7FC

enum { ARM_OFF = 0, ARM_ALL = 1, ARM_LIST = 2 };

typedef struct ExpVert {
    int x;
    int y;
    int z;
    int behind;
    int sx;
    int sy;
} ExpVert;

typedef struct ExpPoly {
    int mesh;
    int header;
    int column;
    int row;
    char id[32];
    int n;
    ExpVert v[4];
} ExpPoly;

typedef struct ExpZone {
    int index;
    int id;
    int flags;
    int model_index;
    int min[3];
    int max[3];
    int entry[3];
    ExpVert foot[4];
} ExpZone;

typedef struct ExpSlot {
    int index;
    int flags;
    char name[32];
    int position[3];
    int angle[3];
    ExpVert at;
} ExpSlot;

typedef struct ExpDoc {
    ExpPoly *polys;
    int poly_n;
    int poly_cap;
    ExpZone zones[FARM_ZONE_COUNT];
    int zone_n;
    ExpSlot slots[SCENE_MODEL_COUNT];
    int slot_n;
    int scene;
    int map;
    int area;
} ExpDoc;

static int arm_mode;
static int args_from_cli;
static int filter_ids[FILTER_MAX];
static int filter_n;
static int written_ids[WRITTEN_MAX];
static int written_n;

static int ensure_dir(void) {
    if (_mkdir("debug") != 0 && errno != EEXIST) {
        return -1;
    }
    if (_mkdir(MAP_DIR) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void project_vert(ExpVert *v, int x, int y, int z) {
    short sx;
    short sy;

    if (v == NULL) {
        return;
    }
    v->x = x;
    v->y = y;
    v->z = z;
    if (gte_project(x, y, z, &sx, &sy) != 0) {
        v->behind = 1;
        v->sx = 0;
        v->sy = 0;
        return;
    }
    v->behind = 0;
    v->sx = sx;
    v->sy = sy;
}

static int any_behind(const ExpVert *v, int n) {
    int i;

    if (v == NULL || n <= 0) {
        return 1;
    }
    for (i = 0; i < n; i++) {
        if (v[i].behind) {
            return 1;
        }
    }
    return 0;
}

static int add_poly(ExpDoc *doc, const ExpPoly *poly) {
    ExpPoly *grown;

    if (doc == NULL || poly == NULL) {
        return -1;
    }
    if (doc->poly_n == doc->poly_cap) {
        int cap;

        cap = doc->poly_cap == 0 ? 64 : doc->poly_cap * 2;
        grown = (ExpPoly *)realloc(doc->polys, (size_t)cap * sizeof(ExpPoly));
        if (grown == NULL) {
            return -1;
        }
        doc->polys = grown;
        doc->poly_cap = cap;
    }
    doc->polys[doc->poly_n++] = *poly;
    return 0;
}

static int prim_size(unsigned mode) {
    switch (mode & 0xFDFFu) {
    case 0x2400:
    case 0x2004:
    case 0x3101:
    case 0x3800:
        return 0x18;
    case 0x2800:
    case 0x3000:
        return 0x14;
    case 0x2501:
    case 0x3400:
    case 0x3004:
    case 0x3901:
        return 0x1C;
    case 0x3501:
    case 0x3C00:
    case 0x3804:
        return 0x24;
    case 0x2C00:
    case 0x2D01:
    case 0x2804:
        return 0x20;
    case 0x3D01:
        return 0x2C;
    case 0x4001:
        return 0xC;
    case 0x2000:
    case 0x2101:
    case 0x2901:
    case 0x5001:
        return 0x10;
    default:
        return 0;
    }
}

/* Vertex index of the matching raw packet when it fits, otherwise the packed
 * indices at 0x08. Lines and a packet that cannot hold the indices are not emitted. */
static int prim_vertices(unsigned mode, int size, int *nvert, int *index_at) {
    int cmd;
    int at;
    int count;

    cmd = ((mode >> 8) & 0xFD) & ~1;
    if (cmd == 0x20 || cmd == 0x24 || cmd == 0x30 || cmd == 0x34) {
        count = 3;
    } else if (cmd == 0x28 || cmd == 0x2C || cmd == 0x38 || cmd == 0x3C) {
        count = 4;
    } else {
        return 0;
    }
    if (cmd == 0x20 || cmd == 0x28) {
        at = 0x08;
    } else if (cmd == 0x30) {
        at = 0x10;
    } else if (cmd == 0x24) {
        at = 0x14;
    } else if (cmd == 0x34) {
        at = 0x1C;
    } else if (cmd == 0x38) {
        at = 0x14;
    } else if (cmd == 0x2C) {
        at = 0x18;
    } else {
        at = 0x24;
    }
    if (at + 2 * count > size) {
        at = 0x08;
    }
    if (at + 2 * count > size) {
        return 0;
    }
    *nvert = count;
    *index_at = at;
    return 1;
}

/* `ptr` for `bytes` stays inside [base, base + size). A short or missing span is incomplete data. */
static int span_ok(const void *ptr, size_t bytes, const unsigned char *base, int size) {
    const unsigned char *start;
    size_t offset;

    if (ptr == NULL || base == NULL || size <= 0 || bytes == 0) {
        return 0;
    }
    start = (const unsigned char *)ptr;
    if (start < base) {
        return 0;
    }
    offset = (size_t)(start - base);
    if (offset > (size_t)size || bytes > (size_t)size - offset) {
        return 0;
    }
    return 1;
}

/* Walk the TMD that fits in `bound`. An unmapped primitive or a short packet stops that
 * object and keeps the polygons already accepted. Returns -1 only when the polygon list
 * cannot grow. */
static int emit_mesh(ExpDoc *doc, const Model *model, const unsigned char *bound, int bound_size) {
    TmdHeader *tmd;
    int object_i;

    if (doc == NULL || model == NULL || model->tmd == NULL) {
        return 0;
    }
    if (!span_ok(model->tmd, sizeof(TmdHeader), bound, bound_size) || model->tmd->n_obj <= 0) {
        return 0;
    }
    tmd = model->tmd;
    for (object_i = 0; object_i < tmd->n_obj; object_i++) {
        TmdObject *object;
        SVECTOR *verts;
        unsigned char *prim;
        int prim_i;
        size_t vert_bytes;

        object = &tmd->objects[object_i];
        if (!span_ok(object, sizeof *object, bound, bound_size)) {
            return 0;
        }
        if (object->scale == 0 || object->n_vert == 0 || object->n_vert > (1u << 20)) {
            continue;
        }
        vert_bytes = (size_t)object->n_vert * sizeof(SVECTOR);
        verts = (SVECTOR *)((unsigned char *)tmd->objects + object->vert_top);
        prim = (unsigned char *)tmd->objects + object->primitive_top;
        if (!span_ok(verts, vert_bytes, bound, bound_size)) {
            continue;
        }
        {
            int prim_limit;

            prim_limit = object->n_primitive > 8192u ? 8192 : (int)object->n_primitive;
            for (prim_i = 0; prim_i < prim_limit; prim_i++) {
            unsigned mode;
            int size;
            int nvert;
            int index_at;
            int v;
            int ok;
            ExpPoly poly;

            if (!span_ok(prim, 4, bound, bound_size)) {
                break;
            }
            mode = *(unsigned short *)(prim + 2);
            size = prim_size(mode);
            if (size == 0 || !span_ok(prim, (size_t)size, bound, bound_size)) {
                break;
            }
            if (prim_vertices(mode, size, &nvert, &index_at)) {
                memset(&poly, 0, sizeof poly);
                poly.mesh = 1;
                poly.header = (int)*(unsigned int *)prim;
                poly.n = nvert;
                snprintf(poly.id, sizeof poly.id, "%d:%d", object_i, prim_i);
                ok = 1;
                for (v = 0; v < nvert; v++) {
                    unsigned index;

                    index = *(unsigned short *)(prim + index_at + v * 2);
                    if (index >= object->n_vert) {
                        ok = 0;
                        break;
                    }
                    project_vert(&poly.v[v], verts[index].vx, verts[index].vy, verts[index].vz);
                }
                if (ok && add_poly(doc, &poly) != 0) {
                    return -1;
                }
            }
            prim += size;
            }
        }
    }
    return 0;
}

static int emit_grid(ExpDoc *doc, int which, int cols, int rows) {
    VECTOR origin;
    VECTOR next;
    int step;
    int half;
    int col;
    int row;

    memset(&origin, 0, sizeof origin);
    memset(&next, 0, sizeof next);
    field_result_coords(0, 0, &origin, (s16)which);
    field_result_coords(1, 0, &next, (s16)which);
    step = next.vx - origin.vx;
    half = step / 2;
    if (half == 0) {
        return 0;
    }
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            VECTOR centre;
            ExpPoly poly;
            int x;
            int z;

            memset(&centre, 0, sizeof centre);
            field_result_coords((s16)col, (s16)row, &centre, (s16)which);
            x = centre.vx;
            z = centre.vz;
            memset(&poly, 0, sizeof poly);
            poly.column = col;
            poly.row = row;
            poly.n = 4;
            snprintf(poly.id, sizeof poly.id, "%d,%d", col, row);
            project_vert(&poly.v[0], x - half, 0, z - half);
            project_vert(&poly.v[1], x + half, 0, z - half);
            project_vert(&poly.v[2], x + half, 0, z + half);
            project_vert(&poly.v[3], x - half, 0, z + half);
            if (add_poly(doc, &poly) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static void copy_name(char *dst, const unsigned char *section, s32 name) {
    const char *text;
    int i;
    int start;

    if (dst == NULL) {
        return;
    }
    dst[0] = '\0';
    if (section == NULL || name < 0 || farm_section_size <= 0) {
        return;
    }
    start = SECTION_STRINGS_OFFSET + (int)name;
    if (start < 0 || start >= farm_section_size) {
        return;
    }
    text = (const char *)section + start;
    for (i = 0; i < NAME_MAX_COPY; i++) {
        if (start + i >= farm_section_size) {
            dst[0] = '\0';
            return;
        }
        if (text[i] == '\0') {
            dst[i] = '\0';
            return;
        }
        dst[i] = text[i];
    }
    dst[0] = '\0';
}

static int emit_slots(ExpDoc *doc, const FarmModeScript *script, const unsigned char *section) {
    int i;

    if (doc == NULL || script == NULL) {
        return 0;
    }
    for (i = 0; i < SCENE_MODEL_COUNT; i++) {
        const FarmZoneModel *model;
        ExpSlot *slot;

        model = &script->models[i];
        if (model->flags & SCENE_ENTRY_ABSENT) {
            continue;
        }
        if (doc->slot_n >= SCENE_MODEL_COUNT) {
            break;
        }
        slot = &doc->slots[doc->slot_n++];
        memset(slot, 0, sizeof *slot);
        slot->index = i;
        slot->flags = (int)model->flags;
        copy_name(slot->name, section, model->name);
        slot->position[0] = model->position.vx;
        slot->position[1] = model->position.vy;
        slot->position[2] = model->position.vz;
        slot->angle[0] = model->angle.vx;
        slot->angle[1] = model->angle.vy;
        slot->angle[2] = model->angle.vz;
        project_vert(&slot->at, slot->position[0], slot->position[1], slot->position[2]);
    }
    return 0;
}

static int emit_zones(ExpDoc *doc, const FarmModeScript *script) {
    int i;

    if (doc == NULL || script == NULL) {
        return 0;
    }
    for (i = 0; i < FARM_ZONE_COUNT; i++) {
        const FarmZone *zone;
        ExpZone *out;

        zone = &script->zones[i];
        if (zone->id == ZONE_ID_EMPTY || (zone->flags & ZONE_FLAG_DEAD)) {
            continue;
        }
        out = &doc->zones[doc->zone_n++];
        memset(out, 0, sizeof *out);
        out->index = i;
        out->id = zone->id;
        out->flags = zone->flags;
        out->model_index = zone->modelIndex;
        out->min[0] = zone->min.x;
        out->min[1] = zone->min.y;
        out->min[2] = zone->min.z;
        out->max[0] = zone->max.x;
        out->max[1] = zone->max.y;
        out->max[2] = zone->max.z;
        out->entry[0] = zone->entry.x;
        out->entry[1] = zone->entry.y;
        out->entry[2] = zone->entry.z;
        project_vert(&out->foot[0], out->min[0], out->min[1], out->min[2]);
        project_vert(&out->foot[1], out->max[0], out->min[1], out->min[2]);
        project_vert(&out->foot[2], out->max[0], out->min[1], out->max[2]);
        project_vert(&out->foot[3], out->min[0], out->min[1], out->max[2]);
    }
    return 0;
}

static void json_string(FILE *out, const char *text) {
    fputc('"', out);
    for (; text != NULL && *text != '\0'; text++) {
        if (*text == '"' || *text == '\\') {
            fputc('\\', out);
        }
        fputc(*text, out);
    }
    fputc('"', out);
}

static void json_screen(FILE *out, const ExpVert *v) {
    if (v->behind) {
        fputs("{\"behind\": true}", out);
        return;
    }
    fprintf(out, "{\"x\": %d, \"y\": %d}", v->sx, v->sy);
}

static void json_xyz(FILE *out, int x, int y, int z) {
    fprintf(out, "[%d, %d, %d]", x, y, z);
}

static int write_json(FILE *out, const ExpDoc *doc) {
    int i;
    int v;

    if (out == NULL || doc == NULL || (doc->poly_n > 0 && doc->polys == NULL)) {
        return -1;
    }
    fprintf(out, "{\n  \"scene\": %d,\n  \"place\": {\"map\": %d, \"area\": %d},\n  \"polygons\": [", doc->scene,
            doc->map, doc->area);
    for (i = 0; i < doc->poly_n; i++) {
        const ExpPoly *poly;

        poly = &doc->polys[i];
        fprintf(out, "%s\n    {\"kind\": \"%s\", \"id\": \"%s\"", i == 0 ? "" : ",", poly->mesh ? "mesh" : "grid",
                poly->id);
        if (poly->mesh) {
            fprintf(out, ", \"header\": %u", (unsigned)poly->header);
        } else {
            fprintf(out, ", \"column\": %d, \"row\": %d", poly->column, poly->row);
        }
        fputs(", \"world\": [", out);
        for (v = 0; v < poly->n; v++) {
            if (v != 0) {
                fputs(", ", out);
            }
            json_xyz(out, poly->v[v].x, poly->v[v].y, poly->v[v].z);
        }
        fputs("], \"screen\": [", out);
        for (v = 0; v < poly->n; v++) {
            if (v != 0) {
                fputs(", ", out);
            }
            json_screen(out, &poly->v[v]);
        }
        fputs("]}", out);
    }
    fputs("\n  ],\n  \"zones\": [", out);
    for (i = 0; i < doc->zone_n; i++) {
        const ExpZone *zone;

        zone = &doc->zones[i];
        fprintf(out, "%s\n    {\"index\": %d, \"id\": %d, \"flags\": %d, \"model_index\": %d, \"min\": ",
                i == 0 ? "" : ",", zone->index, zone->id, zone->flags, zone->model_index);
        json_xyz(out, zone->min[0], zone->min[1], zone->min[2]);
        fputs(", \"max\": ", out);
        json_xyz(out, zone->max[0], zone->max[1], zone->max[2]);
        fputs(", \"entry\": ", out);
        json_xyz(out, zone->entry[0], zone->entry[1], zone->entry[2]);
        fputc('}', out);
    }
    fputs("\n  ],\n  \"slots\": [", out);
    for (i = 0; i < doc->slot_n; i++) {
        const ExpSlot *slot;

        slot = &doc->slots[i];
        fprintf(out, "%s\n    {\"index\": %d, \"slot\": %d, \"flags\": %d, \"name\": ", i == 0 ? "" : ",",
                slot->index, slot->index, slot->flags);
        json_string(out, slot->name);
        fputs(", \"position\": ", out);
        json_xyz(out, slot->position[0], slot->position[1], slot->position[2]);
        fputs(", \"angle\": ", out);
        json_xyz(out, slot->angle[0], slot->angle[1], slot->angle[2]);
        fputs(", \"screen\": ", out);
        json_screen(out, &slot->at);
        fputc('}', out);
    }
    fputs("\n  ]\n}\n", out);
    return ferror(out) ? -1 : 0;
}

static void svg_poly(FILE *out, const char *id, const ExpVert *v, int n) {
    int i;
    int sx;
    int sy;

    if (out == NULL || id == NULL || n <= 0 || any_behind(v, n)) {
        return;
    }
    fputs("<polygon points=\"", out);
    for (i = 0; i < n; i++) {
        if (i != 0) {
            fputc(' ', out);
        }
        fprintf(out, "%d,%d", v[i].sx, v[i].sy);
    }
    fputs("\" fill=\"none\" stroke=\"black\"/>\n", out);
    sx = 0;
    sy = 0;
    for (i = 0; i < n; i++) {
        sx += v[i].sx;
        sy += v[i].sy;
    }
    fprintf(out, "<text x=\"%d\" y=\"%d\">%s</text>\n", sx / n, sy / n, id);
}

static int write_svg(FILE *out, const ExpDoc *doc) {
    int i;

    if (out == NULL || doc == NULL || (doc->poly_n > 0 && doc->polys == NULL)) {
        return -1;
    }
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\">\n", out);
    for (i = 0; i < doc->poly_n; i++) {
        svg_poly(out, doc->polys[i].id, doc->polys[i].v, doc->polys[i].n);
    }
    for (i = 0; i < doc->zone_n; i++) {
        char id[16];

        snprintf(id, sizeof id, "%x", doc->zones[i].id);
        svg_poly(out, id, doc->zones[i].foot, 4);
    }
    fputs("</svg>\n", out);
    return ferror(out) ? -1 : 0;
}

static int map_export_scene(const FarmModeScript *script, const Model *model, LimitPair place,
                            const unsigned char *section, const unsigned char *bound, int bound_size) {
    ExpDoc doc;
    char mesh_path[64];
    char svg_path[64];
    FILE *mesh;
    FILE *svg;
    int failed;

    if (script == NULL) {
        return -1;
    }
    memset(&doc, 0, sizeof doc);
    doc.scene = script->head.scene;
    doc.map = place.value;
    doc.area = place.location;
    if (emit_mesh(&doc, model, bound, bound_size) != 0) {
        free(doc.polys);
        return -1;
    }
    if (place.location == PLACE_FARM) {
        if (emit_grid(&doc, 0, FIELD_COLS, FIELD_ROWS) != 0) {
            free(doc.polys);
            return -1;
        }
    } else if (place.location == PLACE_GREENHOUSE) {
        if (emit_grid(&doc, 1, GREENHOUSE_SIZE, GREENHOUSE_SIZE) != 0) {
            free(doc.polys);
            return -1;
        }
    }
    emit_zones(&doc, script);
    emit_slots(&doc, script, section);
    if (ensure_dir() != 0) {
        free(doc.polys);
        return -1;
    }
    snprintf(mesh_path, sizeof mesh_path, MAP_DIR "/scene_%x_mesh.json", (unsigned)script->head.scene);
    snprintf(svg_path, sizeof svg_path, MAP_DIR "/scene_%x_wireframe.svg", (unsigned)script->head.scene);
    mesh = fopen(mesh_path, "wb");
    svg = fopen(svg_path, "wb");
    failed = mesh == NULL || svg == NULL || write_json(mesh, &doc) != 0 || write_svg(svg, &doc) != 0;
    if (mesh != NULL) {
        failed |= fclose(mesh) != 0;
    }
    if (svg != NULL) {
        failed |= fclose(svg) != 0;
    }
    free(doc.polys);
    return failed ? -1 : 0;
}

static void save_gte(MATRIX *matrix, GteSettings *settings) {
    gte_ReadRotMatrix(matrix);
    gte_debug_settings(settings);
}

static void restore_gte(const MATRIX *matrix, const GteSettings *settings) {
    MATRIX color;
    MATRIX light;
    int row;
    int col;

    SetRotMatrix((MATRIX *)matrix);
    SetTransMatrix((MATRIX *)matrix);
    SetGeomOffset(settings->off_x, settings->off_y);
    SetGeomScreen(settings->screen_h);
    gte_set_depth_cue(settings->dqa, settings->dqb);
    gte_set_far_color(settings->far_color[0], settings->far_color[1], settings->far_color[2]);
    gte_SetBackColor(settings->back_color[0], settings->back_color[1], settings->back_color[2]);
    memset(&color, 0, sizeof color);
    memset(&light, 0, sizeof light);
    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            color.m[row][col] = settings->color_matrix[row][col];
            light.m[row][col] = settings->light_matrix[row][col];
        }
    }
    gte_SetColorMatrix(&color);
    gte_SetLightMatrix(&light);
}

static int gte_same(const MATRIX *a, const MATRIX *b, const GteSettings *sa, const GteSettings *sb) {
    int row;
    int col;

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            if (a->m[row][col] != b->m[row][col]) {
                return 0;
            }
        }
        if (a->t[row] != b->t[row]) {
            return 0;
        }
    }
    if (sa->screen_h != sb->screen_h || sa->off_x != sb->off_x || sa->off_y != sb->off_y) {
        return 0;
    }
    if (sa->dqa != sb->dqa || sa->dqb != sb->dqb) {
        return 0;
    }
    for (row = 0; row < 3; row++) {
        if (sa->far_color[row] != sb->far_color[row] || sa->back_color[row] != sb->back_color[row]) {
            return 0;
        }
        for (col = 0; col < 3; col++) {
            if (sa->color_matrix[row][col] != sb->color_matrix[row][col] ||
                sa->light_matrix[row][col] != sb->light_matrix[row][col]) {
                return 0;
            }
        }
    }
    return 1;
}

static void set_identity_view(void) {
    gte_set_rotation_identity();
    gte_set_translation(0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);
}

static int parse_id(const char *text, int *id) {
    char *end;
    long value;

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        value = strtol(text, &end, 16);
    } else {
        value = strtol(text, &end, 10);
    }
    if (end == text || *end != '\0') {
        return -1;
    }
    *id = (int)value;
    return 0;
}

static void apply_token(const char *token) {
    char buf[128];
    char *cursor;
    char *comma;

    filter_n = 0;
    if (token == NULL || token[0] == '\0') {
        arm_mode = ARM_OFF;
        return;
    }
    if (strcmp(token, "1") == 0) {
        arm_mode = ARM_ALL;
        return;
    }
    if (strlen(token) >= sizeof buf) {
        arm_mode = ARM_OFF;
        return;
    }
    memcpy(buf, token, strlen(token) + 1);
    arm_mode = ARM_LIST;
    cursor = buf;
    while (cursor != NULL && cursor[0] != '\0') {
        int id;

        comma = strchr(cursor, ',');
        if (comma != NULL) {
            *comma = '\0';
        }
        if (parse_id(cursor, &id) != 0 || filter_n >= FILTER_MAX) {
            arm_mode = ARM_OFF;
            filter_n = 0;
            return;
        }
        filter_ids[filter_n++] = id;
        cursor = comma == NULL ? NULL : comma + 1;
    }
    if (filter_n == 0) {
        arm_mode = ARM_OFF;
    }
}

static void arm_from_env(void) {
    const char *env;

    env = getenv("HMBTN_EXPORT_MAPS");
    if (env == NULL || env[0] == '\0') {
        arm_mode = ARM_OFF;
        filter_n = 0;
        return;
    }
    apply_token(env);
}

static int scene_wanted(int scene) {
    int i;

    if (arm_mode == ARM_ALL) {
        return 1;
    }
    if (arm_mode != ARM_LIST) {
        return 0;
    }
    for (i = 0; i < filter_n; i++) {
        if (filter_ids[i] == scene) {
            return 1;
        }
    }
    return 0;
}

static int scene_written(int scene) {
    int i;

    for (i = 0; i < written_n; i++) {
        if (written_ids[i] == scene) {
            return 1;
        }
    }
    return 0;
}

static void mark_written(int scene) {
    if (written_n < WRITTEN_MAX) {
        written_ids[written_n++] = scene;
    }
}

void map_export_args(int argc, char **argv) {
    int i;

    args_from_cli = 0;
    arm_mode = ARM_OFF;
    filter_n = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--export-maps") != 0) {
            continue;
        }
        args_from_cli = 1;
        if (i + 1 < argc && argv[i + 1][0] != '-') {
            apply_token(argv[i + 1]);
            i++;
        } else {
            arm_mode = ARM_ALL;
            filter_n = 0;
        }
    }
}

void map_export_tick(void) {
    MATRIX saved;
    GteSettings settings;
    int scene;
    int wrote;

    if (!args_from_cli) {
        arm_from_env();
    }
    if (arm_mode == ARM_OFF || farm_model.tmd == NULL || farm_area.mode.data != &farm_area.mode.script) {
        return;
    }
    scene = farm_area.mode.script.head.scene;
    if (!scene_wanted(scene) || scene_written(scene)) {
        return;
    }
    {
        const unsigned char *tmd_bytes;
        const unsigned char *bound;
        int bound_size;

        bound = NULL;
        bound_size = 0;
        tmd_bytes = (const unsigned char *)farm_model.tmd;
        if (farm_area.sectionBuffer != NULL && farm_section_size > 0 && tmd_bytes >= farm_area.sectionBuffer &&
            tmd_bytes < farm_area.sectionBuffer + farm_section_size) {
            bound = farm_area.sectionBuffer;
            bound_size = farm_section_size;
        }
        save_gte(&saved, &settings);
        model_load_lighting();
        wrote = map_export_scene(&farm_area.mode.script, &farm_model, farm_area.mode.place, farm_area.sectionBuffer,
                                 bound, bound_size);
    }
    restore_gte(&saved, &settings);
    if (wrote == 0) {
        mark_written(scene);
    }
}

static char *read_all(const char *path) {
    FILE *file;
    long length;
    char *text;

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    text = (char *)malloc((size_t)length + 1);
    if (text == NULL) {
        fclose(file);
        return NULL;
    }
    if (fread(text, 1, (size_t)length, file) != (size_t)length) {
        free(text);
        fclose(file);
        return NULL;
    }
    fclose(file);
    text[length] = '\0';
    return text;
}

static int file_contains(const char *path, const char *needle) {
    char *text;
    int found;

    text = read_all(path);
    if (text == NULL) {
        return 0;
    }
    found = strstr(text, needle) != NULL;
    free(text);
    return found;
}

static void fill_f3(unsigned char *packet, int a, int b, int c) {
    *(unsigned int *)packet = (unsigned int)TMD_PRIM_F3_RAW << 16;
    *(unsigned short *)(packet + 0x08) = (unsigned short)a;
    *(unsigned short *)(packet + 0x0A) = (unsigned short)b;
    *(unsigned short *)(packet + 0x0C) = (unsigned short)c;
}

static void build_self_scene(FarmModeScript *script, unsigned char *section, size_t section_bytes) {
    int i;

    memset(script, 0, sizeof *script);
    memset(section, 0, section_bytes);
    script->head.scene = SELFTEST_SCENE;
    for (i = 0; i < FARM_ZONE_COUNT; i++) {
        script->zones[i].id = ZONE_ID_EMPTY;
    }
    for (i = 0; i < SCENE_MODEL_COUNT; i++) {
        script->models[i].flags = SCENE_ENTRY_ABSENT;
    }
    script->zones[0].id = 34;
    script->zones[0].flags = 0;
    script->zones[0].modelIndex = 1;
    script->zones[0].min.x = 10;
    script->zones[0].min.y = 0;
    script->zones[0].min.z = 400;
    script->zones[0].max.x = 40;
    script->zones[0].max.y = 20;
    script->zones[0].max.z = 500;
    script->zones[0].entry.x = 20;
    script->zones[0].entry.y = 0;
    script->zones[0].entry.z = 450;
    script->models[0].flags = 0;
    script->models[0].name = 0;
    script->models[0].position.vx = 10;
    script->models[0].position.vy = 0;
    script->models[0].position.vz = 20;
    memcpy(section + SECTION_STRINGS_OFFSET, "SELFTEST.TMD", 13);
}

typedef struct SelfBlob {
    TmdHeader hdr;
    unsigned char packet[0x20];
    SVECTOR verts[6];
} SelfBlob;

static void build_self_model(SelfBlob *blob, Model *model) {
    TmdObject *object;

    memset(blob, 0, sizeof *blob);
    blob->hdr.n_obj = 1;
    object = &blob->hdr.objects[0];
    object->scale = 1;
    object->n_vert = 6;
    object->n_primitive = 2;
    object->vert_top = (u32)((unsigned char *)&blob->verts - (unsigned char *)&blob->hdr.objects[0]);
    object->primitive_top = (u32)(blob->packet - (unsigned char *)&blob->hdr.objects[0]);
    fill_f3(blob->packet, 0, 1, 2);
    fill_f3(blob->packet + 0x10, 3, 4, 5);
    blob->verts[0].vx = 0;
    blob->verts[0].vy = 0;
    blob->verts[0].vz = 1000;
    blob->verts[1].vx = 100;
    blob->verts[1].vy = 0;
    blob->verts[1].vz = 1000;
    blob->verts[2].vx = 0;
    blob->verts[2].vy = 100;
    blob->verts[2].vz = 1000;
    blob->verts[3].vx = 0;
    blob->verts[3].vy = 0;
    blob->verts[3].vz = -10;
    blob->verts[4].vx = 50;
    blob->verts[4].vy = 0;
    blob->verts[4].vz = 1000;
    blob->verts[5].vx = 0;
    blob->verts[5].vy = 50;
    blob->verts[5].vz = 1000;
    memset(model, 0, sizeof *model);
    model->tmd = &blob->hdr;
}

static int scene_file_ok(const char *json, const char *svg) {
    if (json == NULL || svg == NULL) {
        return 0;
    }
    if (strstr(json, "\"scene\": 2046") == NULL || strstr(json, "\"area\": 2") == NULL) {
        return 0;
    }
    if (strstr(json, "\"world\": [[0, 0, 1000], [100, 0, 1000], [0, 100, 1000]]") == NULL) {
        return 0;
    }
    if (strstr(json, "\"min\": [10, 0, 400]") == NULL || strstr(json, "\"max\": [40, 20, 500]") == NULL ||
        strstr(json, "\"entry\": [20, 0, 450]") == NULL) {
        return 0;
    }
    if (strstr(json, "SELFTEST.TMD") == NULL || strstr(json, "\"x\": 160, \"y\": 120") == NULL) {
        return 0;
    }
    if (strstr(json, "\"behind\": true") == NULL || strstr(json, "\"kind\": \"grid\"") != NULL) {
        return 0;
    }
    if (strstr(svg, ">0:0<") == NULL || strstr(svg, ">0:1<") != NULL) {
        return 0;
    }
    return 1;
}

static int mutated_fails(const char *json, const char *svg, int mutate_svg, const char *from, const char *to) {
    const char *source;
    char *copy;
    char *hit;
    int ok;

    source = mutate_svg ? svg : json;
    copy = (char *)malloc(strlen(source) + strlen(to) + 1);
    if (copy == NULL) {
        return 0;
    }
    hit = strstr(source, from);
    if (hit == NULL) {
        free(copy);
        return 0;
    }
    memcpy(copy, source, (size_t)(hit - source));
    memcpy(copy + (hit - source), to, strlen(to));
    memcpy(copy + (hit - source) + strlen(to), hit + strlen(from), strlen(hit + strlen(from)) + 1);
    ok = scene_file_ok(mutate_svg ? json : copy, mutate_svg ? copy : svg) == 0;
    free(copy);
    return ok;
}

static int check(int ok, const char *what) {
    SDL_Log("map export check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static void put_env(const char *value) {
    static char slot[160];

    snprintf(slot, sizeof slot, "HMBTN_EXPORT_MAPS=%s", value == NULL ? "" : value);
    _putenv(slot);
}

static int read_file_same(const char *path, const char *before) {
    char *after;
    int same;

    after = read_all(path);
    same = after != NULL && strcmp(after, before) == 0;
    free(after);
    return same;
}

int map_exporter_self_check(void) {
    FarmModeScript *script;
    FarmModeScript *saved_script;
    unsigned char *section;
    SelfBlob blob;
    Model model;
    LimitPair place;
    MATRIX found;
    MATRIX after;
    GteSettings found_settings;
    GteSettings after_settings;
    s32 saved_section_size;
    Model saved_model;
    FarmModeScript *saved_data;
    LimitPair saved_place;
    int saved_arm;
    int saved_cli;
    int saved_filter_n;
    int saved_written_n;
    int saved_filter[FILTER_MAX];
    char saved_env[128];
    const char *env;
    char *json;
    char *svg;
    char grid_id[32];
    char grid_world[160];
    VECTOR origin;
    VECTOR next;
    int half;
    int failed;
    int slot_x;
    char *first_write;

    failed = 0;
    script = (FarmModeScript *)malloc(sizeof *script);
    saved_script = (FarmModeScript *)malloc(sizeof *saved_script);
    section = (unsigned char *)malloc(SECTION_STRINGS_OFFSET + 32);
    if (script == NULL || saved_script == NULL || section == NULL) {
        free(script);
        free(saved_script);
        free(section);
        return check(0, "self-check allocation");
    }
    saved_section_size = farm_section_size;
    saved_model = farm_model;
    saved_data = farm_area.mode.data;
    saved_place = farm_area.mode.place;
    memcpy(saved_script, &farm_area.mode.script, sizeof *saved_script);
    saved_arm = arm_mode;
    saved_cli = args_from_cli;
    saved_filter_n = filter_n;
    saved_written_n = written_n;
    memcpy(saved_filter, filter_ids, sizeof saved_filter);
    env = getenv("HMBTN_EXPORT_MAPS");
    snprintf(saved_env, sizeof saved_env, "%s", env == NULL ? "" : env);

    save_gte(&found, &found_settings);
    set_identity_view();
    build_self_scene(script, section, SECTION_STRINGS_OFFSET + 32);
    build_self_model(&blob, &model);
    farm_section_size = SECTION_STRINGS_OFFSET + 32;
    place.value = 0;
    place.location = PLACE_2;
    failed |= check(map_export_scene(script, &model, place, section, (const unsigned char *)&blob, (int)sizeof blob) == 0,
                    "constructed scene writes");
    json = read_all(MAP_DIR "/scene_7fe_mesh.json");
    svg = read_all(MAP_DIR "/scene_7fe_wireframe.svg");
    failed |= check(scene_file_ok(json, svg), "triangle, zone, slot, screen point, and svg label");
    if (json == NULL || svg == NULL) {
        failed |= check(0, "constructed files were read");
    } else {
        failed |= check(mutated_fails(json, svg, 0, "\"world\": [[0, 0, 1000], [100, 0, 1000], [0, 100, 1000]]",
                                       "\"world\": [[1, 0, 1000], [100, 0, 1000], [0, 100, 1000]]"),
                        "a wrong triangle fails");
        failed |= check(mutated_fails(json, svg, 0, "\"min\": [10, 0, 400]", "\"min\": [11, 0, 400]"),
                        "a wrong zone fails");
        failed |= check(mutated_fails(json, svg, 0, "SELFTEST.TMD", "OTHER.TMD"), "a wrong slot name fails");
        failed |= check(mutated_fails(json, svg, 0, "\"x\": 160, \"y\": 120", "\"x\": 161, \"y\": 120"),
                        "a wrong screen point fails");
        failed |= check(mutated_fails(json, svg, 1, ">0:0<", ">0:9<"), "a missing svg label fails");
    }

    failed |= check(map_export_scene(NULL, &model, place, section, (const unsigned char *)&blob, (int)sizeof blob) != 0,
                    "a null script returns an error");
    script->head.scene = 0x7F9;
    failed |= check(map_export_scene(script, NULL, place, section, (const unsigned char *)&blob, (int)sizeof blob) == 0,
                    "a null model still writes");
    failed |= check(file_contains(MAP_DIR "/scene_7f9_mesh.json", "\"scene\": 2041") &&
                        !file_contains(MAP_DIR "/scene_7f9_mesh.json", "\"kind\": \"mesh\""),
                    "a null model omits the mesh");
    {
        unsigned short saved_mode;

        saved_mode = *(unsigned short *)(blob.packet + 0x12);
        *(unsigned short *)(blob.packet + 0x12) = 0xFFFF;
        script->head.scene = 0x7FA;
        failed |= check(map_export_scene(script, &model, place, section, (const unsigned char *)&blob,
                                         (int)sizeof blob) == 0,
                        "an unmapped primitive still writes");
        failed |= check(file_contains(MAP_DIR "/scene_7fa_mesh.json", "\"id\": \"0:0\"") &&
                            !file_contains(MAP_DIR "/scene_7fa_mesh.json", "\"id\": \"0:1\""),
                        "an unmapped primitive keeps the earlier triangle");
        *(unsigned short *)(blob.packet + 0x12) = saved_mode;
    }
    script->head.scene = 0x7FB;
    failed |= check(map_export_scene(script, &model, place, section, (const unsigned char *)&blob,
                                     (int)sizeof(TmdHeader)) == 0,
                    "a short mesh bound still writes");
    failed |= check(file_contains(MAP_DIR "/scene_7fb_mesh.json", "\"scene\": 2043") &&
                        file_contains(MAP_DIR "/scene_7fb_mesh.json", "\"id\": 34") &&
                        !file_contains(MAP_DIR "/scene_7fb_mesh.json", "\"kind\": \"mesh\""),
                    "a short mesh bound omits the mesh and keeps the zone");

    script->head.scene = FARM_GRID_SCENE;
    place.location = PLACE_FARM;
    failed |= check(map_export_scene(script, &model, place, section, (const unsigned char *)&blob, (int)sizeof blob) == 0,
                    "farm grid writes");
    snprintf(grid_id, sizeof grid_id, "\"id\": \"%d,%d\"", FIELD_COLS - 1, FIELD_ROWS - 1);
    memset(&origin, 0, sizeof origin);
    memset(&next, 0, sizeof next);
    field_result_coords(0, 0, &origin, 0);
    field_result_coords(1, 0, &next, 0);
    half = (next.vx - origin.vx) / 2;
    snprintf(grid_world, sizeof grid_world, "\"world\": [[%d, 0, %d], [%d, 0, %d], [%d, 0, %d], [%d, 0, %d]]",
             origin.vx - half, origin.vz - half, origin.vx + half, origin.vz - half, origin.vx + half,
             origin.vz + half, origin.vx - half, origin.vz + half);
    failed |= check(file_contains(MAP_DIR "/scene_7fd_mesh.json", "\"kind\": \"grid\"") &&
                        file_contains(MAP_DIR "/scene_7fd_mesh.json", "\"id\": \"0,0\"") &&
                        file_contains(MAP_DIR "/scene_7fd_mesh.json", grid_id) &&
                        file_contains(MAP_DIR "/scene_7fd_mesh.json", grid_world),
                    "farm grid cells");
    script->head.scene = GREENHOUSE_GRID_SCENE;
    place.location = PLACE_GREENHOUSE;
    failed |= check(map_export_scene(script, &model, place, section, (const unsigned char *)&blob, (int)sizeof blob) == 0,
                    "greenhouse grid writes");
    snprintf(grid_id, sizeof grid_id, "\"id\": \"%d,%d\"", GREENHOUSE_SIZE - 1, GREENHOUSE_SIZE - 1);
    failed |= check(file_contains(MAP_DIR "/scene_7fc_mesh.json", grid_id), "greenhouse grid cell");

    {
        MATRIX dirty;

        memset(&dirty, 0, sizeof dirty);
        dirty.m[1][2] = 0x1357;
        dirty.t[0] = 0x111;
        SetRotMatrix(&dirty);
        SetTransMatrix(&dirty);
        SetGeomScreen(50);
    }
    restore_gte(&found, &found_settings);
    save_gte(&after, &after_settings);
    failed |= check(gte_same(&found, &after, &found_settings, &after_settings), "gte view restored");
    failed |= check(after.m[1][2] != 0x1357 || found.m[1][2] == 0x1357, "dirty matrix was not left loaded");

    written_n = 0;
    remove(MAP_DIR "/scene_52_mesh.json");
    remove(MAP_DIR "/scene_52_wireframe.svg");
    remove(MAP_DIR "/scene_50_mesh.json");
    remove(MAP_DIR "/scene_50_wireframe.svg");
    remove(MAP_DIR "/scene_60_mesh.json");
    remove(MAP_DIR "/scene_60_wireframe.svg");
    remove(MAP_DIR "/scene_51_mesh.json");
    remove(MAP_DIR "/scene_51_wireframe.svg");
    remove(MAP_DIR "/scene_53_mesh.json");
    remove(MAP_DIR "/scene_53_wireframe.svg");
    memcpy(&farm_area.mode.script, script, sizeof *script);
    farm_area.mode.data = &farm_area.mode.script;
    farm_area.sectionBuffer = section;
    farm_model = model;
    farm_area.mode.place = place;

    args_from_cli = 0;
    put_env(NULL);
    farm_area.mode.script.head.scene = 0x52;
    map_export_tick();
    failed |= check(!file_contains(MAP_DIR "/scene_52_mesh.json", "\"scene\""), "no flag writes nothing");

    put_env("1");
    farm_area.mode.script.head.scene = 0x50;
    slot_x = farm_area.mode.script.models[0].position.vx;
    map_export_tick();
    failed |= check(file_contains(MAP_DIR "/scene_50_mesh.json", "\"scene\": 80"), "env 1 writes scene 0x50");
    failed |= check(farm_area.mode.script.models[0].position.vx == slot_x, "slot position unchanged");

    written_n = 0;
    remove(MAP_DIR "/scene_50_mesh.json");
    remove(MAP_DIR "/scene_50_wireframe.svg");
    put_env("0x50");
    farm_area.mode.script.head.scene = 0x60;
    map_export_tick();
    failed |= check(!file_contains(MAP_DIR "/scene_60_mesh.json", "\"scene\""), "filter 0x50 skips 0x60");
    farm_area.mode.script.head.scene = 0x50;
    map_export_tick();
    failed |= check(file_contains(MAP_DIR "/scene_50_mesh.json", "\"scene\": 80"), "filter 0x50 writes 0x50");

    written_n = 0;
    remove(MAP_DIR "/scene_50_mesh.json");
    remove(MAP_DIR "/scene_50_wireframe.svg");
    remove(MAP_DIR "/scene_60_mesh.json");
    remove(MAP_DIR "/scene_60_wireframe.svg");
    {
        char *argv[] = {"hmbtn-host", "--export-maps", "0x60"};

        map_export_args(3, argv);
    }
    put_env("0x50");
    farm_area.mode.script.head.scene = 0x50;
    map_export_tick();
    failed |= check(!file_contains(MAP_DIR "/scene_50_mesh.json", "\"scene\""), "command line skips 0x50");
    farm_area.mode.script.head.scene = 0x60;
    map_export_tick();
    failed |= check(file_contains(MAP_DIR "/scene_60_mesh.json", "\"scene\": 96"), "command line writes 0x60");
    first_write = read_all(MAP_DIR "/scene_60_mesh.json");
    map_export_tick();
    failed |= check(read_file_same(MAP_DIR "/scene_60_mesh.json", first_write == NULL ? "" : first_write),
                    "second tick leaves the files unchanged");
    free(first_write);

    written_n = 0;
    args_from_cli = 0;
    put_env("1");
    farm_model.tmd = NULL;
    farm_area.mode.script.head.scene = 0x51;
    map_export_tick();
    failed |= check(!file_contains(MAP_DIR "/scene_51_mesh.json", "\"scene\""), "null tmd writes nothing");
    farm_model.tmd = &blob.hdr;
    farm_area.mode.data = NULL;
    farm_area.mode.script.head.scene = 0x53;
    map_export_tick();
    failed |= check(!file_contains(MAP_DIR "/scene_53_mesh.json", "\"scene\""), "unready mode writes nothing");

    memcpy(&farm_area.mode.script, saved_script, sizeof *saved_script);
    farm_area.mode.data = saved_data;
    farm_area.mode.place = saved_place;
    farm_model = saved_model;
    farm_section_size = saved_section_size;
    arm_mode = saved_arm;
    args_from_cli = saved_cli;
    filter_n = saved_filter_n;
    written_n = saved_written_n;
    memcpy(filter_ids, saved_filter, sizeof filter_ids);
    put_env(saved_env[0] == '\0' ? NULL : saved_env);
    restore_gte(&found, &found_settings);
    free(json);
    free(svg);
    free(script);
    free(saved_script);
    free(section);
    return failed == 0 ? 0 : 1;
}
