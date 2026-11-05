/* App-owned state; stable on-disk schemas are intentionally preserved. */
static int note_ids[1024],note_count,note_selected=-1,note_page;
static char note_text[8192];
