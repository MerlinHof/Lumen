void ui_loading(const char *message) {
    widget_count = 0;
    memset(ui_pixels, 255, sizeof(ui_pixels));
    int w = textwidth("LUMEN", ui_font(UI_DISPLAY));
    drawtext((600 - w) / 2, 285, "LUMEN", ui_font(UI_DISPLAY), 560);
    w = textwidth(message, ui_font(UI_BODY));
    drawtext(w < 560 ? (600 - w) / 2 : 20, 370, message, ui_font(UI_BODY), 560);
}
