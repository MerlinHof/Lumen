#include "lumen_app.h"
#include <math.h>
#include "astronomy.h"
#include "sky_catalog.h"

static int history_page, solar, settings;
enum { UI_SECONDARY=PAINT_SECONDARY, UI_PRIMARY=PAINT_BODY, UI_BODY=PAINT_BODY, UI_TITLE=PAINT_TITLE };
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static const char *tr(const char *label) {
    static const char *labels[][3] = {
        {"Jetzt","Now","Maintenant"},{"Ereignisse","Events","Événements"},
        {"Datum","Date","Date"},{"Himmel & Ort","Sky and location","Ciel et position"},
        {"Ort","Location","Position"},{"Geschichte","History","Histoire"},
        {"Stern","Star","Étoile"},{"Mondbeleuchtung","Lunar illumination","Illumination lunaire"},
        {"Breitengrad, Längengrad","Latitude, longitude","Latitude, longitude"},
        {"Sternbild-Linien","Constellation lines","Lignes des constellations"},
        {"Neumond","New moon","Nouvelle lune"},{"Erstes Viertel","First quarter","Premier quartier"},
        {"Vollmond","Full moon","Pleine lune"},{"Letztes Viertel","Last quarter","Dernier quartier"},
        {"Nächster Sonnenaufgang","Next sunrise","Prochain lever du soleil"},
        {"−1 Tag","−1 day","−1 jour"},{"+1 Tag","+1 day","+1 jour"},
        {"−1 Jahr","−1 year","−1 an"},{"+1 Jahr","+1 year","+1 an"},
        {"Sun","Sun","Soleil"},{"Moon","Moon","Lune"},{"Mercury","Mercury","Mercure"},
        {"Merkur","Mercury","Mercure"},{"Erde","Earth","Terre"},{"Saturn","Saturn","Saturne"},
        {"Neptun","Neptune","Neptune"}
    };
    for(size_t i=0;i<sizeof(labels)/sizeof(*labels);i++)
        if(!strcmp(label,labels[i][0]))return word(labels[i][0],labels[i][1],labels[i][2]);
    return label;
}
static int calendar_days(int year,int month) {
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month-1]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
}
static void drawtext_raw(int x,int y,const char *label,int style,int width) {
    lumen_text(x,y,width,25,label,style,-1,1);
}
static void disc(int x,int y,int radius,int gray){ui_paint_ellipse(x,y,radius,radius,gray);}
static void key_outline(int x,int y,int w,int h){ui_paint_surface(x,y,w,h,10,255,110);}
static void button_h(int x,int y,int w,int h,const char *label,int active) {
    lumen_button(x,y,w,h,tr(label),active);
}
static void map_selector(void) {
    const char *tabs[]={word("Himmel","Sky","Ciel"),word("Sonnensystem","Solar system","Système solaire")};
    lumen_segments("map",259,74,253,44,tabs,2,solar);
}
#define ui_font(style) (style)
#define centered(x,y,w,h,label,style) ui_paint_text(x,y,w,h,tr(label),style)
#define wrapped(x,y,label,style,w,lines) lumen_wrapped(x,y,w,(lines)*36,tr(label),style,lines)
#define drawtext(x,y,label,style,w) drawtext_raw(x,y,tr(label),style,w)
#define heading(label) ui_paint_heading(tr(label),1)
#define icon_button ui_paint_icon_button
#define ring ui_paint_ring
#define aa_disc disc
#define toggle lumen_toggle
#define rect(x,y,w,h,gray) ui_paint_surface(x,y,w,h,0,gray,-1)

static struct {
    double lat, lon;
    int year, month, day, hour, minute;
} sky_config;
static int sky_panel, sky_step = 0, sky_selected = -1, sky_show_lines = 1;
static int sky_px[sizeof(sky_stars)/sizeof(*sky_stars)], sky_py[sizeof(sky_stars)/sizeof(*sky_stars)];
static astro_time_t sky_time(void) {
    return Astronomy_MakeTime(sky_config.year, sky_config.month, sky_config.day, sky_config.hour,
                              sky_config.minute, 0);
}
static int sky_project(double ra, double dec, astro_time_t *t, astro_rotation_t rotation, int *x,
                       int *y) {
    astro_spherical_t s = {.status = ASTRO_SUCCESS, .lat = dec, .lon = ra * 15, .dist = 1};
    astro_vector_t v = Astronomy_RotateVector(rotation, Astronomy_VectorFromSphere(s, *t));
    astro_equatorial_t eq = Astronomy_EquatorFromVector(v);
    astro_horizon_t h =
        Astronomy_Horizon(t, Astronomy_MakeObserver(sky_config.lat, sky_config.lon, 500), eq.ra,
                          eq.dec, REFRACTION_NORMAL);
    if (h.altitude < 0)
        return 0;
    double r = (90 - h.altitude) / 90 * 251, a = h.azimuth * .01745329252;
    *x = 300 - r * sin(a);
    *y = 425 - r * cos(a);
    return 1;
}
static void sky_now(void) {
    time_t n = time(NULL);
    struct tm t;
    gmtime_r(&n, &t);
    sky_config.year = t.tm_year + 1900;
    sky_config.month = t.tm_mon + 1;
    sky_config.day = t.tm_mday;
    sky_config.hour = t.tm_hour;
    sky_config.minute = t.tm_min;
}
static void sky_init(void) {
    sky_config.lat = 47.856;
    sky_config.lon = 12.128;
    sky_now();
    double p[2];
    if ((app_state_load("sky-location-1.bin", p, sizeof(p)) ||
         lumen_legacy_state("sky-location-1.bin", p, sizeof(p))) && isfinite(p[0]) && isfinite(p[1]) &&
        fabs(p[0]) <= 90 && fabs(p[1]) <= 180) {
        app_state_save("sky-location-1.bin", p, sizeof(p));
        sky_config.lat = p[0];
        sky_config.lon = p[1];
    }
}
static void sky_map(void) {
    astro_time_t t = sky_time();
    astro_rotation_t rot = Astronomy_Rotation_EQJ_EQD(&t);
    astro_observer_t obs = Astronomy_MakeObserver(sky_config.lat, sky_config.lon, 500);
    ui_paint_ring(300, 425, 251, 2);
    if (sky_show_lines) {
        for (unsigned i = 0; i < sizeof(sky_lines) / sizeof(*sky_lines); i++) {
            int x, y, u, v;
            if (sky_project(sky_lines[i][0], sky_lines[i][1], &t, rot, &x, &y) &&
                sky_project(sky_lines[i][2], sky_lines[i][3], &t, rot, &u, &v) &&
                abs(u - x) + abs(v - y) < 300)
                ui_paint_line(x, y, u, v, 1, 168);
        }
    }
    lumen_points_begin(0);
    int points=0;
    for (unsigned i = 0; i < sizeof(sky_stars) / sizeof(*sky_stars); i++) {
        sky_px[i] = sky_py[i] = -1000;
        int x, y;
        if (sky_stars[i].mag > 5.5 ||
            !sky_project(sky_stars[i].ra, sky_stars[i].dec, &t, rot, &x, &y))
            continue;
        sky_px[i] = x;
        sky_py[i] = y;
        int radius = sky_stars[i].mag < 1 ? 3 : sky_stars[i].mag < 3 ? 2 : 1;
        if(points<3000)lumen_point(points++,x,y,radius);
    }
    lumen_points_end();
    for(size_t i=0;i<sizeof(sky_stars)/sizeof(*sky_stars);i++)
        if(sky_px[i]>=0 && sky_stars[i].mag<1.1 && *sky_stars[i].name)
            drawtext_raw(sky_px[i]+6,sky_py[i]-8,sky_stars[i].name,ui_font(UI_SECONDARY),140);
    const astro_body_t planets[] = {BODY_SUN,  BODY_MOON,    BODY_MERCURY, BODY_VENUS,
                                    BODY_MARS, BODY_JUPITER, BODY_SATURN};
    const char *names[] = {"Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn"};
    for (int i = 0; i < 7; i++) {
        astro_equatorial_t eq = Astronomy_Equator(planets[i], &t, obs, EQUATOR_OF_DATE, ABERRATION);
        if (eq.status != ASTRO_SUCCESS)
            continue;
        astro_horizon_t h = Astronomy_Horizon(&t, obs, eq.ra, eq.dec, REFRACTION_NORMAL);
        if (h.altitude >= 0) {
            double r = (90 - h.altitude) / 90 * 251, a = h.azimuth * .01745329252;
            int x = 300 - r * sin(a), y = 425 - r * cos(a);
            disc(x, y, 6, 255);
            ring(x, y, 6, 2);
            drawtext_raw(x + 8, y - 11, tr(names[i]), ui_font(UI_BODY), 130);
        }
    }
    centered(280, 158, 40, 28, "N", ui_font(UI_SECONDARY));
    centered(280, 666, 40, 28, "S", ui_font(UI_SECONDARY));
    drawtext_raw(18, 413, lumen_language == 0 ? "O" : "E", ui_font(UI_SECONDARY), 30);
    drawtext_raw(563, 413, lumen_language == 2 ? "O" : "W", ui_font(UI_SECONDARY), 30);
    if (sky_selected >= 0) {
        rect(45, 588, 510, 80, 255);
        key_outline(45, 588, 510, 80);
        const char *n = sky_stars[sky_selected].name;
        astro_constellation_t c =
            Astronomy_Constellation(sky_stars[sky_selected].ra, sky_stars[sky_selected].dec);
        char b[200];
        snprintf(b, 200, "%s · %s", *n ? n : tr("Stern"), c.status == ASTRO_SUCCESS ? c.name : "");
        drawtext_raw(58, 599, b, ui_font(UI_BODY), 484);
        snprintf(b, 200, "RA %.2fh · Dec %.1f° · %.1f mag", sky_stars[sky_selected].ra,
                 sky_stars[sky_selected].dec, sky_stars[sky_selected].mag);
        drawtext_raw(58, 631, b, ui_font(UI_SECONDARY), 484);
    }
}
static void sky_draw(void) {
    heading("Planetarium");
    map_selector();
    icon_button(528, 74, IC_SETTINGS);
    char b[150];
    snprintf(b, sizeof(b), "%02d.%02d.%04d %02d:%02d UTC · %.2f°, %.2f°", sky_config.day,
             sky_config.month, sky_config.year, sky_config.hour, sky_config.minute, sky_config.lat,
             sky_config.lon);
    centered(20, 124, 560, 31, b, ui_font(UI_SECONDARY));
    sky_map();
    const char *steps[] = {"+1 h", "+1 d", "+1 y", "+1000 y"};
    lumen_segments("step",83, 696, 434, 38, steps, 4, sky_step);
    icon_button(24, 696, IC_PREV);
    icon_button(530, 696, IC_NEXT);
    button_h(28, 747, 162, 42, "Jetzt", 0);
    button_h(210, 747, 170, 42, "Ereignisse", 0);
    button_h(400, 747, 172, 42, "Datum", 0);
}
static void sky_time_label(astro_time_t t, char *out, int cap) {
    astro_utc_t u = Astronomy_UtcFromTime(t);
    snprintf(out, cap, "%02d.%02d.%04d %02d:%02d UTC", u.day, u.month, u.year, u.hour, u.minute);
}
static void sky_settings_draw(void) {
    heading("Himmel & Ort");
    const char *tabs[] = {"Ort", "Ereignisse", "Geschichte"};
    lumen_segments("panel",28, 136, 544, 44, tabs, 3, sky_panel);
    char b[512];
    if (sky_panel == 0) {
        snprintf(b, sizeof(b), "%.4f, %.4f", sky_config.lat, sky_config.lon);
        button_h(28, 228, 544, 56, b, 0);
        drawtext(30, 194, "Breitengrad, Längengrad", ui_font(UI_SECONDARY), 540);
        drawtext(30, 330, "Sternbild-Linien", ui_font(UI_PRIMARY), 450);
        toggle(492, 330, sky_show_lines);
        wrapped(30, 430,
                "Tippe einen Stern an, um Namen, Sternbild und Koordinaten zu sehen. Datum und "
                "Uhrzeit sind UTC. Die Karte wird vollständig lokal berechnet.",
                ui_font(UI_BODY), 540, 5);
        wrapped(30, 610,
                "Zeitreise: 1 bis 12026. In ferner Zukunft ist die Sternkarte eine Näherung ohne "
                "Eigenbewegung der Sterne.",
                ui_font(UI_SECONDARY), 540, 4);
    } else if (sky_panel == 1) {
        astro_time_t t = sky_time();
        astro_angle_result_t phase = Astronomy_MoonPhase(t);
        snprintf(b, sizeof(b), "%s: %.0f%%", tr("Mondbeleuchtung"),
                 phase.status == ASTRO_SUCCESS ? 50 * (1 - cos(phase.angle * .01745329252)) : 0);
        drawtext_raw(30, 219, b, ui_font(UI_PRIMARY), 540);
        for (int i = 0; i < 4; i++) {
            double angle = i * 90;
            astro_search_result_t r = Astronomy_SearchMoonPhase(angle, t, 40);
            const char *name[] = {"Neumond", "Erstes Viertel", "Vollmond", "Letztes Viertel"};
            drawtext(30, 290 + i * 87, name[i], ui_font(UI_PRIMARY), 540);
            if (r.status == ASTRO_SUCCESS) {
                sky_time_label(r.time, b, sizeof(b));
                drawtext_raw(30, 326 + i * 87, b, ui_font(UI_BODY), 540);
            }
        }
        astro_search_result_t rise = Astronomy_SearchRiseSet(
            BODY_SUN, Astronomy_MakeObserver(sky_config.lat, sky_config.lon, 500), DIRECTION_RISE,
            t, 2);
        if (rise.status == ASTRO_SUCCESS) {
            sky_time_label(rise.time, b, sizeof(b));
            drawtext(30, 650, "Nächster Sonnenaufgang", ui_font(UI_SECONDARY), 540);
            drawtext_raw(30, 682, b, ui_font(UI_BODY), 540);
        }
    } else {
        static const char *history[] = {
            "1543 · Copernicus\nDe revolutionibus describes a heliocentric model: Earth is a "
            "planet orbiting the Sun.",
            "1609 · Galileo\nA telescope reveals lunar mountains and resolves the Milky Way into "
            "countless stars. In 1610 he observes Jupiter's four large moons.",
            "1687 · Newton\nUniversal gravitation connects falling objects on Earth with the "
            "motion of planets.",
            "1915 · Einstein\nGeneral relativity explains gravity as spacetime curvature, "
            "including Mercury's orbital precession.",
            "1995 · 51 Pegasi b\nMayor and Queloz announce a planet orbiting a Sun-like star. "
            "Thousands of exoplanets follow."};
        wrapped(30, 230, history[history_page % 5], ui_font(UI_TITLE), 540, 9);
        lumen_pager(708, history_page, 5);
    }
}
static void sky_shift(int sign) {
    if (sky_step == 3 || sky_step == 2) {
        sky_config.year += sign * (sky_step == 3 ? 1000 : 1);
        if (sky_config.year < 1)
            sky_config.year = 1;
        if (sky_config.year > 12026)
            sky_config.year = 12026;
        if (sky_config.day > calendar_days(sky_config.year, sky_config.month))
            sky_config.day = calendar_days(sky_config.year, sky_config.month);
    } else {
        astro_time_t t = Astronomy_AddDays(sky_time(), sign * (sky_step == 0 ? 1. / 24 : 1));
        astro_utc_t u = Astronomy_UtcFromTime(t);
        if (u.year >= 1 && u.year <= 12026) {
            sky_config.year = u.year;
            sky_config.month = u.month;
            sky_config.day = u.day;
            sky_config.hour = u.hour;
            sky_config.minute = u.minute;
        }
    }
    sky_selected = -1;
}
static void solar16(void) {
    if (sky_config.year < 1)
        sky_now();
    heading("Planetarium");
    map_selector();
    icon_button(528, 74, IC_SETTINGS);
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
        ui_paint_ring(300, 407, r, 1);
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

static void draw(void) {
    if(settings)sky_settings_draw();
    else if(solar)solar16();
    else sky_draw();
}
static void prompt_date(void) {
    char value[32];
    snprintf(value,sizeof(value),"%04d-%02d-%02d %02d:%02d",sky_config.year,sky_config.month,
        sky_config.day,sky_config.hour,sky_config.minute);
    lumen_request("input","YYYY-MM-DD HH:MM UTC",value,2);
}
static void accept_text(int action) {
    char value[128],extra;
    if(!lumen_string("text",value,sizeof(value)) || !*value)return;
    if(action==1) {
        double lat,lon;
        if(sscanf(value,"%lf , %lf %c",&lat,&lon,&extra)==2 && isfinite(lat) &&
           isfinite(lon) && fabs(lat)<=90 && fabs(lon)<=180) {
            const double location[]={lat,lon};
            sky_config.lat=lat;sky_config.lon=lon;
            app_state_save("sky-location-1.bin",location,sizeof(location));
        }
    } else if(action==2) {
        int y,m,d,h,min;
        if(sscanf(value,"%d-%d-%d %d:%d %c",&y,&m,&d,&h,&min,&extra)==5 &&
           y>=1 && y<=12026 && m>=1 && m<=12 && d>=1 && d<=calendar_days(y,m) &&
           h>=0 && h<24 && min>=0 && min<60) {
            sky_config.year=y;sky_config.month=m;sky_config.day=d;
            sky_config.hour=h;sky_config.minute=min;
        }
    }
    sky_selected=-1;
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){sky_init();return 1;}
    if(in->kind==INPUT_BACK && settings){settings=lumen_back=0;return 1;}
    if(in->kind==INPUT_TEXT){accept_text(in->x);return 1;}
    if(in->kind==INPUT_PAGE && settings && sky_panel==2){history_page=in->x;return 1;}
    if(lumen_is("component")) {
        char id[32];lumen_string("id",id,sizeof(id));
        char value[8];
        if(!lumen_string("text",value,sizeof(value)) || strlen(value)!=1 ||
           value[0]<'0' || value[0]>'3')return 0;
        int choice=value[0]-'0';
        if(!strcmp(id,"map"))solar=choice==1;
        if(!strcmp(id,"step") && choice>=0 && choice<4)sky_step=choice;
        if(!strcmp(id,"panel") && choice>=0 && choice<3)sky_panel=choice;
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(settings) {
        if(!sky_panel && y>=228 && y<284) {
            char value[48];snprintf(value,sizeof(value),"%.4f, %.4f",sky_config.lat,sky_config.lon);
            lumen_request("input",tr("Breitengrad, Längengrad"),value,1);
        } else if(!sky_panel && y>=325 && y<375 && x>=460)sky_show_lines=!sky_show_lines;
    } else if(x>=528 && y>=74 && y<118) {
        settings=lumen_back=1;sky_panel=0;
    } else if(solar && y>=714 && y<762) {
        int saved=sky_step;
        sky_step=x<297 ? 1 : 2;
        sky_shift(x<160 || (x>=297 && x<434) ? -1 : 1);
        sky_step=saved;
    } else if(!solar && y>=696 && y<740) {
        if(x<80)sky_shift(-1);
        else if(x>=520)sky_shift(1);
    } else if(!solar && y>=747) {
        if(x<200){sky_now();sky_selected=-1;}
        else if(x<390){settings=lumen_back=1;sky_panel=1;}
        else prompt_date();
    } else if(!solar && y>=158 && y<693) {
        int best=625;sky_selected=-1;
        for(size_t i=0;i<sizeof(sky_px)/sizeof(*sky_px);i++) {
            int dx=x-sky_px[i],dy=y-sky_py[i],dist=dx*dx+dy*dy;
            if(dist<best){best=dist;sky_selected=(int)i;}
        }
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
