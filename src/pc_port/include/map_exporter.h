#ifndef MAP_EXPORTER_H
#define MAP_EXPORTER_H

/* One JSON mesh and one SVG wireframe for the farm scene that is loaded. */

int map_exporter_self_check(void);
void map_export_args(int argc, char **argv);
void map_export_tick(void);

#endif
