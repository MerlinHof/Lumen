/* App-owned module; assembled by apps/host/compose.h. */
static void extra_wiki_restore(void) {
    char path[2048];
    local_path(path, sizeof(path), "wiki-last.xml");
    if (!access(path, R_OK))
        discover_parse_lookup(path);
}
