/* App-owned module; assembled by apps/host/compose.h. */
static void document_surface(void) {
    if (selected < 0)
        return;
    if (!engine_render(page, ui_pixels + 12 * WIDTH + 28, WIDTH, 544, 728, zoom, panx, pany))
        ui_message("Dieses Buch konnte nicht geöffnet oder dargestellt werden.");
    annotations_draw();
    if (rounding_profile() && engine_reflowable()) {
        TextRect images[64];
        int n = engine_image_rects(images, 64);
        for (int i=0; i<n; i++) {
            TextRect r = images[i];
            if (r.x0 < 0 || r.y0 < 0 || r.x1 > 544 || r.y1 > 728) continue;
            int x = (int)ceilf(r.x0), y = (int)ceilf(r.y0);
            rounded_clip_apply(28+x,12+y,(int)floorf(r.x1)-x,(int)floorf(r.y1)-y,255);
        }
    }
    rect(0, 744, 600, 56, 255);
}
