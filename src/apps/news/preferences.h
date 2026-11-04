/* App-owned module; assembled by apps/host/compose.h. */
static const char *news_category(int c) {
    const char *labels[] = {"Welt",    "Wissenschaft",  "Technologie",
                            "Energie", "An diesem Tag", "Tagesrätsel"};
    return tr(labels[c >= 0 && c < 6 ? c : 0]);
}

static void app_newspaper(void) {
    polish_news_draw();
}
static void app_news_settings(void) {
    heading("Zeitung · Themen");
    for (int i = 0; i < 6; i++) {
        drawtext_raw(30, 150 + i * 74, news_category(i), ui_font(UI_PRIMARY), 425);
        toggle(492, 150 + i * 74, (news_topics >> i) & 1);
    }
    wrapped(
        30, 620,
        "Die Themen gelten für die nächste Ausgabe. Meldungen bleiben in der Sprache der Quelle.",
        ui_font(UI_SECONDARY), 540, 3);
    button_h(28, 706, 544, 52, "Fertig", 0);
}
static void app_news_source(void) {
    heading("Quelle");
    if (news_source_page < 0 || news_source_page >= newspaper.count)
        return;
    NewsArticle *a = &newspaper.article[news_source_page];
    drawtext_raw(28, 143, a->publisher, ui_font(UI_TITLE), 544);
    wrapped_raw(28, 210, a->title, ui_font(UI_PRIMARY), 544, 4);
    wrapped_raw(28, 350, a->published, ui_font(UI_SECONDARY), 544, 2);
    wrapped_raw(28, 412, a->url, ui_font(UI_BODY), 544, 7);
    if (*a->url) {
        uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
        if (qrcodegen_encodeText(a->url, temp, qr, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, 12,
                                 qrcodegen_Mask_AUTO, true)) {
            int n = qrcodegen_getSize(qr), scale = 150 / (n + 8);
            if (scale < 1)
                scale = 1;
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++)
                    if (qrcodegen_getModule(qr, x, y))
                        rect(420 + x * scale, 620 + y * scale, scale, scale, 0);
        }
    }
}
