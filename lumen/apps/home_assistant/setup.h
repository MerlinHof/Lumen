/* App-owned module; assembled by apps/host/compose.h. */
static void app_home_assistant(void) {
    ha_dashboard();
}
static void app_ha_setup(void) {
    heading("Home Assistant");
    drawtext(30, 142, "Serveradresse", ui_font(UI_BODY), 540);
    button_h(28, 188, 544, 52, *ha_url ? ha_url : "http://homeassistant.local:8123", 0);
    drawtext(30, 281, "Langlebiger Zugriffstoken", ui_font(UI_BODY), 540);
    button_h(28, 327, 544, 52, *ha_token ? "••••••••  ·  Ändern" : "Token eingeben", 0);
    wrapped(30, 414,
            "Am Handy: Bücher übertragen starten, die Webseite öffnen und Home Assistant wählen. "
            "Adresse und Token dort einfügen. Danach diese App erneut öffnen.",
            ui_font(UI_BODY), 540, 5);
    button(28, 607, 544, "Verbinden", 0);
}
static int wiki_offsets[1024], wiki_pages;
static uint64_t wiki_hash;
