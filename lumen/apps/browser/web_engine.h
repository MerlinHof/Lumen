#ifndef LUMEN_WEB_ENGINE_H
#define LUMEN_WEB_ENGINE_H
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
int web_engine_render(const char *html,const char *raster,int page,int width,int height,
                      const char *assets,const char *fallback,FILE *metadata);
int web_engine_css_supports(const char *property,const char *value);
void web_engine_inline_css(FILE *out,const char *css);
int web_engine_media_matches(const char *query,int width,int height);
char *web_engine_stylesheet(const char *url,const char *base,char final[1024]);
unsigned char *web_engine_svg(const unsigned char *bytes,size_t size,size_t *length);
unsigned char *web_engine_image(const char *url,const char *base,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
