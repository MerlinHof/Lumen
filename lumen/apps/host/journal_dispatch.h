/* App-owned module; assembled by apps/host/compose.h. */
static void u17_more_choice(int target, int choice) {
    if (target == 935 && choice >= 0 && choice < model_count17) {
        snprintf(llm_model16, sizeof(llm_model16), "%s", model_ids17[choice]);
        extra_write_text("llm-model.txt", llm_model16);
    }
}
static int u17_more_commit(int target, const char *text) {
    (void)text;
    if (target == 176)
        models_loaded17 = 0;
    return 0;
}
static int u17_more_draw(void) {
    if (ui_route() == 128) {
        chats_draw();
        return 1;
    }
    if (ui_route() == 122) {
        chat_draw();
        return 1;
    }
    if (ui_route() == 123) {
        heading_actions("OpenAI", 1);
        icon_button(528, 74, IC_RESTART);
        if (!models_loaded17) {
            models_parse17();
            models_loaded17 = 1;
            char key[2049] = "";
            config16("llm-key.txt", key, sizeof(key));
            if (*key && job_pid16 < 0)
                job_start16(3, "");
        }
        select_row17(155, "Modell", llm_model16);
        drawtext(30, 280, "API-Schlüssel", ui_font(UI_PRIMARY), 440);
        icon_button(528, 272, IC_EDIT);
        char key[2049] = "";
        config16("llm-key.txt", key, sizeof(key));
        wrapped_raw(30, 335, *key ? key : tr("Noch kein Schlüssel hinterlegt."), ui_font(UI_SECONDARY), 540, 10);
        wrapped(30, 620, "Ein Textmodell wählen. Die Liste stammt aus deinem API-Konto.", ui_font(UI_SECONDARY),
                540, 3);
        if (job_pid16 > 0)
            centered(28, 705, 544, 40, "Modelle werden geladen …", ui_font(UI_SECONDARY));
        return 1;
    }
    return 0;
}
static int u17_more_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return -1;
    int x = in->x, y = in->y;
    if (ui_route() == 128)
        return chats_input(in);
    if (ui_route() == 122)
        return chat_input(in);
    if (ui_route() == 123) {
        if (y >= 74 && y < 118 && x >= 516) {
            job_start16(3, "");
            return 1;
        }
        if (y >= 155 && y < 203 && x >= 300) {
            if (model_count17) {
                int chosen = 0;
                for (int i = 0; i < model_count17; i++)
                    if (!strcmp(model_ids17[i], llm_model16))
                        chosen = i;
                open_dropdown(935, model_labels17, model_count17, chosen, 240, 209, 332);
            } else
                job_start16(3, "");
            return 1;
        }
        if (y >= 272 && y < 316) {
            char key[2049] = "";
            config16("llm-key.txt", key, sizeof(key));
            open_keyboard(176, "API-Schlüssel", key);
            return 1;
        }
        return y < 48 ? -1 : 1;
    }
    return -1;
}
