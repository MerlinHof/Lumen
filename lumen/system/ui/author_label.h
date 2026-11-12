static const char *author_label(const char *author, char *buf, size_t size) {
    const char *comma = strchr(author, ',');
    if (comma && !strchr(comma + 1, ',')) {
        const char *first = comma + 1;
        while (*first == ' ')
            first++;
        snprintf(buf, size, "%s %.*s", first, (int)(comma - author), author);
        return buf;
    }
    return *author ? author : tr("Autor unbekannt");
}
