static void solar16(void) {
    if (sky_config.year < 1)
        sky_now();
    heading_actions("Planetarium", 1);
    icon_button(528, 74, IC_CLOCK);
    astro_time_t t = sky_time();
    char label[120];
    sky_time_label(t, label, sizeof(label));
    centered(28, 133, 544, 40, label, ui_font(UI_SECONDARY));
    const astro_body_t ids[] = {BODY_MERCURY, BODY_VENUS,  BODY_EARTH,  BODY_MARS,
                                BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE};
    const char *names[] = {"Merkur",  "Venus",  "Erde",   "Mars",
                           "Jupiter", "Saturn", "Uranus", "Neptun"};
    aa_disc(300, 407, 10, 0);
    for (int i = 0; i < 8; i++) {
        int r = 40 + i * 26;
        aa_arc_color(300, 407, r, 0, 360, 1, 176);
        astro_vector_t v = Astronomy_HelioVector(ids[i], t);
        if (v.status != ASTRO_SUCCESS)
            continue;
        double a = atan2(v.y, v.x);
        int x = 300 + r * cos(a), y = 407 - r * sin(a);
        aa_disc(x, y, i < 4 ? 6 : 8, 0);
        char shortname[4] = {names[i][0], names[i][1], 0};
        drawtext_raw(x + 11, y - 10, shortname, ui_font(UI_SECONDARY), 40);
    }
    for (int i = 0; i < 8; i++)
        drawtext(28 + (i % 4) * 140, 650 + (i / 4) * 25, names[i], ui_font(UI_SECONDARY), 135);
    button_h(28, 714, 125, 48, "−1 Tag", 0);
    button_h(165, 714, 125, 48, "+1 Tag", 0);
    button_h(302, 714, 125, 48, "−1 Jahr", 0);
    button_h(439, 714, 133, 48, "+1 Jahr", 0);
}
