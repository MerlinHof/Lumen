/* Open-Meteo data and cache belong to this package. */
#include "lumen_http.h"
#include <math.h>

typedef struct {char date[64];int code,high,low,rain;} WeatherDay;
typedef struct {
    char city[256],place[256],stamp[64],rise[64],set[64];
    int temp,feel,wind,code,is_day,light;
    WeatherDay days[5];
} Forecast;
static Forecast forecast;
static WireObject weather_json;

static double weather_number(const char *value) {
    if(!value || (*value!='-' && !wire_digit(*value)))return NAN;
    char *end;
    double number=strtod(value,&end);
    return end!=value && isfinite(number) ? number : NAN;
}
static int weather_integer(const char *value) {
    double number=weather_number(value);
    return isfinite(number) && fabs(number)<10000 ? (int)lround(number) : 0;
}
static const char *weather_value(const char *key) {
    const WireField *field=wire_object_get(&weather_json,key);
    return field ? field->value : NULL;
}
static const char *weather_item(const char *array,int index) {
    if(!array || *array!='[')return NULL;
    const char *at=wire_skip_space(array+1);
    for(int i=0;i<index;i++) {
        at=wire_value(at,0);
        if(!at || *(at=wire_skip_space(at))!=',')return NULL;
        at=wire_skip_space(at+1);
    }
    return *at==']' ? NULL : at;
}
static int weather_object(const char *at,char *object,size_t capacity) {
    if(!at || *at!='{')return 0;
    const char *end=wire_value(at,0);
    if(!end || (size_t)(end-at)>=capacity)return 0;
    memcpy(object,at,(size_t)(end-at));
    object[end-at]=0;
    return wire_read_object(&weather_json,object);
}
static int geocoding(const char *json,Forecast *out,double *latitude,double *longitude) {
    char object[8192];
    if(!wire_read_object(&weather_json,json) || !weather_object(weather_item(weather_value("results"),0),object,sizeof(object)))return 0;
    *latitude=weather_number(weather_value("latitude"));
    *longitude=weather_number(weather_value("longitude"));
    wire_field_string(wire_object_get(&weather_json,"name"),out->place,sizeof(out->place));
    return isfinite(*latitude) && fabs(*latitude)<=90 && isfinite(*longitude) && fabs(*longitude)<=180;
}
static int decode_forecast(const char *json,Forecast *out) {
    char object[8192];
    if(!wire_read_object(&weather_json,json))return 0;
    const char *current=weather_value("current"),*daily=weather_value("daily");
    if(!weather_object(current,object,sizeof(object)) || !isfinite(weather_number(weather_value("temperature_2m"))))
        return 0;
    out->temp=weather_integer(weather_value("temperature_2m"));
    out->feel=weather_integer(weather_value("apparent_temperature"));
    out->wind=weather_integer(weather_value("wind_speed_10m"));
    out->code=weather_integer(weather_value("weather_code"));
    out->is_day=weather_integer(weather_value("is_day"));
    wire_field_string(wire_object_get(&weather_json,"time"),out->stamp,sizeof(out->stamp));
    if(!weather_object(daily,object,sizeof(object)))return 0;
    const char *rise=weather_item(weather_value("sunrise"),0),*set=weather_item(weather_value("sunset"),0);
    if(rise)wire_string(rise,out->rise,sizeof(out->rise));
    if(set)wire_string(set,out->set,sizeof(out->set));
    double light=weather_number(weather_item(weather_value("daylight_duration"),0));
    out->light=isfinite(light) && light>=0 && light<=86400 ? (int)light : 0;
    for(int i=0;i<5;i++) {
        WeatherDay *day=&out->days[i];
        const char *date=weather_item(weather_value("time"),i);
        if(date)wire_string(date,day->date,sizeof(day->date));
        day->code=weather_integer(weather_item(weather_value("weather_code"),i));
        day->high=weather_integer(weather_item(weather_value("temperature_2m_max"),i));
        day->low=weather_integer(weather_item(weather_value("temperature_2m_min"),i));
        day->rain=weather_integer(weather_item(weather_value("precipitation_probability_max"),i));
    }
    return *out->days[0].date!=0;
}
static int load_forecast(void) {
    Forecast cached;
    if(app_state_load("forecast-v1.bin",&cached,sizeof(cached)) &&
       memchr(cached.city,0,sizeof(cached.city)) && !strcmp(city,cached.city)) {
        cached.place[sizeof(cached.place)-1]=0;
        cached.stamp[sizeof(cached.stamp)-1]=0;
        cached.rise[sizeof(cached.rise)-1]=0;
        cached.set[sizeof(cached.set)-1]=0;
        for(int i=0;i<5;i++)cached.days[i].date[sizeof(cached.days[i].date)-1]=0;
        forecast=cached;
        return 1;
    }
    static char legacy[100001];
    size_t n;
    if(!lumen_legacy_read("weather.json",legacy,sizeof(legacy)-1,&n))return 0;
    legacy[n]=0;
    Forecast old={0};
    if(!decode_forecast(legacy,&old))return 0;
    if(lumen_legacy_read("weather-place.json",legacy,sizeof(legacy)-1,&n)) {
        double lat,lon;
        legacy[n]=0;
        geocoding(legacy,&old,&lat,&lon);
    }
    forecast=old;
    return 1;
}
static void forecast_operation(const char *operation,const char *data) {
    if(strcmp(operation,"refresh_forecast") || !*data || strlen(data)>=sizeof(forecast.city)) {
        lumen_app_service_result(0,"");return;
    }
    static char body[100001];
    char url[1024],escaped[sizeof(forecast.city)*3];
    char *at=escaped;
    static const char hex[]="0123456789ABCDEF";
    for(const unsigned char *p=(const unsigned char *)data;*p;p++) {
        if((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || wire_digit(*p) || strchr("-._~",*p))
            *at++=(char)*p;
        else {*at++='%';*at++=hex[*p>>4];*at++=hex[*p&15];}
    }
    *at=0;
    const char *language=lumen_language==1 ? "en" : lumen_language==2 ? "fr" : "de";
    snprintf(url,sizeof(url),"https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=%s&format=json",escaped,language);
    double latitude,longitude;
    Forecast result={0};
    snprintf(result.city,sizeof(result.city),"%s",data);
    int ok=lumen_https_get(url,body,sizeof(body)) && geocoding(body,&result,&latitude,&longitude);
    if(ok) {
        snprintf(url,sizeof(url),
            "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
            "&current=temperature_2m,apparent_temperature,weather_code,wind_speed_10m,is_day"
            "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,daylight_duration,precipitation_probability_max"
            "&timezone=auto&forecast_days=5",latitude,longitude);
        ok=lumen_https_get(url,body,sizeof(body)) && decode_forecast(body,&result) &&
           app_state_save("forecast-v1.bin",&result,sizeof(result));
    }
    lumen_app_service_result(ok,"");
}
