typedef struct {
    ReaderPosition position;
    char text[2048];
} Annotation;

typedef struct {
    int reader_document_active, reader_progress_mode, reader_chrome;
    int chapterpage, chapter_count, chaptertab;
    Chapter chapters[256];
    Annotation annotations[128];
    int annotation_count, annotation_page, selecting, selection_x, selection_y;
    int annotation_delete, selection_last_x, selection_last_y;
    TextSelection selection, saved_shading;
    int shading_page, shading_notes, layout_epoch, shading_epoch;
    float shading_zoom, shading_panx, shading_pany;
} ReaderUi;
static ReaderUi reader_ui = {
    .shading_page = -1, .shading_notes = -1, .shading_epoch = -1
};
