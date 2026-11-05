static int annotations_write(void) {
    if (current < 0)
        return 0;
    char path[2048];
    snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
             (unsigned long long)library[current].id);
    return atomic_write(path, reader_ui.annotations, sizeof(Annotation) * (size_t)reader_ui.annotation_count);
}
