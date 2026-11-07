#include "../../lumen/system/library.h"
#include "../../lumen/system/defaults.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
    data_root="build/tests/fresh-data";mkdir(data_root,0700);
    unlink("build/tests/fresh-data/options-v3.txt");unlink("build/tests/fresh-data/settings.txt");
    library_settings_load();
    assert(settings.size==24&&settings.grid==1&&settings.sort==2);
    assert(options[OPT_AUTO_TIME]&&options[OPT_AUTOSTART]);
    assert(options[OPT_COVER_CROP]&&options[OPT_ROUNDING]&&!options[OPT_BATTERY_PERCENT]);
    assert(!options[OPT_KEYBOARD_LANGUAGE]);
    assert(options[OPT_APP_ICON_STYLE]);
    assert(options[OPT_CLOCK] && options[OPT_WIFI] && options[OPT_BATTERY]);
    assert(!options[OPT_DATE] && !options[OPT_BATTERY_PERCENT]);
    assert(access("build/tests/fresh-data/settings.txt",F_OK)==0);
    options[OPT_KEYBOARD_LANGUAGE]=2;options[OPT_LANGUAGE]=1;settings.size=29;library_settings_save();
    library_settings_load();assert(options[OPT_KEYBOARD_LANGUAGE]==2&&options[OPT_LANGUAGE]==1&&settings.size==29);
    FILE *f=fopen("build/tests/fresh-data/options-v3.txt","w");fprintf(f,"%d 65000\n%d 65000\n%d 65000\n%d 1\n",OPT_LANGUAGE,OPT_FONT,OPT_KEYBOARD_LANGUAGE,OPT_UI_REV);fclose(f);
    library_settings_load();assert(options[OPT_LANGUAGE]==0&&options[OPT_FONT]==0&&options[OPT_KEYBOARD_LANGUAGE]==0);
    library_defaults();assert(!memcmp(options,factory_options,sizeof(options))&&!memcmp(&settings,&factory_settings,sizeof(settings)));
    puts("PASS fresh settings: consistent defaults, persisted setup, independent keyboard language, invalid values rejected, reset parity.");
}
