/* Page-bound ink stays compatible with notes saved by older LUMEN versions. */
#include "ink_raster.h"

enum { INK_HEIGHT=688 };
static unsigned char ink_pixels[WIDTH*INK_HEIGHT],ink_undo[WIDTH*INK_HEIGHT];
static int ink_dirty,ink_drag,ink_visible,ink_present,ink_revision;
static int64_t ink_refresh_at;
static void ink_path(char path[64]) {
    snprintf(path,64,"notes/%d-page-%d.ink",editor_note,editor_page);
}

static int ink_save(void) {
    if(!ink_dirty)return 1;
    char path[64],leaf[256];
    ink_path(path);
    const char *root=getenv("LUMEN_LEGACY_DATA");
    int ok=1;
    if(ink_visible)ok=lumen_storage_write_at(root,path,ink_pixels,sizeof(ink_pixels));
    else {
        int dir=lumen_storage_parent_at(root,path,0,leaf);
        if(dir>=0) {
            ok=!unlinkat(dir,leaf,0) || errno==ENOENT;
            if(ok)ok=!fsync(dir);
            close(dir);
        } else ok=0;
    }
    if(ok)ink_dirty=0;
    return ok;
}

/* The display buffer is transient. The original ink file changes only at
 * the end of a stroke, or when the app leaves the current page. */
static void ink_publish(void) {
    ink_visible=0;
    for(size_t i=0;i<sizeof(ink_pixels) && !ink_visible;i++)ink_visible=ink_pixels[i]!=255;
    if(!ink_visible){ink_present=0;return;}
    char leaf[256];
    int dir=lumen_storage_parent("ink-surface.raw",0,leaf);
    if(dir<0)return;
    int fd=openat(dir,".ink-surface.new",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);
    ink_present=0;
    if(fd>=0) {
        FILE *out=fdopen(fd,"wb");
        if(out) {
            size_t size=sizeof(ink_pixels)-2*WIDTH;
            int ok=fwrite(ink_pixels+2*WIDTH,1,size,out)==size;
            if(fclose(out))ok=0;
            if(ok)ink_present=!renameat(dir,".ink-surface.new",dir,leaf);
        } else close(fd);
    }
    if(ink_present)ink_revision=(ink_revision+1)%100000;
    if(!ink_present)unlinkat(dir,".ink-surface.new",0);
    close(dir);
}

static void ink_load(void) {
    char path[64];size_t size;
    ink_path(path);
    if(editor_note<=0 || !lumen_storage_read_at(getenv("LUMEN_LEGACY_DATA"),path,ink_pixels,sizeof(ink_pixels),&size) || size!=sizeof(ink_pixels))
        memset(ink_pixels,255,sizeof(ink_pixels));
    memcpy(ink_undo,ink_pixels,sizeof(ink_undo));
    ink_drag=ink_dirty=0;
    ink_publish();
}
static NoteInkBrush brush;

static int ink_pointer(const Input *in) {
    if(in->kind==INPUT_DOWN && (unsigned)in->x<WIDTH && in->y>=112 && in->y<742) {
        memcpy(ink_undo,ink_pixels,sizeof(ink_undo));
        brush=(NoteInkBrush){.pixels=ink_pixels,.width=width,.erasing=eraser};
        ink_drag=ink_dirty=1;
        note_ink_begin(&brush,in->x,in->y);
    } else if(ink_drag && (in->kind==INPUT_MOVE || in->kind==INPUT_TAP ||
                            in->kind==INPUT_SWIPE || in->kind==INPUT_CANCEL)) {
        if(in->kind!=INPUT_CANCEL && (unsigned)in->x<WIDTH && in->y>=112 && in->y<742)
            note_ink_extend(&brush,in->x,in->y);
        if(in->kind!=INPUT_MOVE)ink_drag=0;
    } else return 0;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    int64_t ms=(int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
    if(in->kind==INPUT_MOVE && ms<ink_refresh_at)return 0;
    ink_refresh_at=ms+33;
    ink_publish();
    if(!ink_drag && !ink_save())return -1;
    return 1;
}
