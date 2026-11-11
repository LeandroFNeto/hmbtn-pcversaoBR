#ifndef PC_PORT_ITEM_ART_TRACE_H
#define PC_PORT_ITEM_ART_TRACE_H

/* Called with the pointer item_art_address just returned, and returns that same pointer.
 * A null result for the shipping bin, or for any id while the mode scene is a house,
 * writes one `itemart:` line. */
void *item_art_note(int id, void *found);

/* Load the shipping bin's picture when `place` is the outdoor farm. Other maps leave it alone. */
void item_art_keep_for_place(int place);

int item_art_self_check(void);

#endif
