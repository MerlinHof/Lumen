/* Notifications own their floating surface; input is captured before app dispatch. */
static int notification_visible(void) {
    return *notification_text() && !editor_ui.keyboard && !sleeping && !extra_ringing() && !power_menu;
}
static void notification_draw(void) {
    if (!notification_visible())
        return;
    raised_surface(19, 57, 558, 91);
    wrapped_raw(37, 76, notification_text(), ui_font(UI_BODY), 470, 2);
    icon_button(525, 75, IC_CLOSE);
}
