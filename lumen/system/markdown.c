#include "markdown.h"
#include <stdlib.h>
#include <string.h>
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "cmark-gfm-core-extensions.h"
void md_escape17(FILE *out,const char *text,int length) {
    for (int i=0;i<length;i++) {
        switch (text[i]) {
        case '&': fputs("&amp;",out);break;
        case '<': fputs("&lt;",out);break;
        case '>': fputs("&gt;",out);break;
        case '"': fputs("&quot;",out);break;
        default: fputc((unsigned char)text[i],out);break;
        }
    }
}
void markdown_render17(FILE *out,const char *text) {
    static int initialized;
    if (!initialized) {
        cmark_gfm_core_extensions_ensure_registered();
        initialized=1;
    }
    int options=CMARK_OPT_VALIDATE_UTF8|CMARK_OPT_FOOTNOTES;
    cmark_parser *parser=cmark_parser_new(options);
    if (!parser) return;
    const char *extensions[]={"table","strikethrough","autolink","tagfilter","tasklist"};
    for (unsigned i=0;i<sizeof(extensions)/sizeof(*extensions);i++) {
        cmark_syntax_extension *extension=cmark_find_syntax_extension(extensions[i]);
        if (extension) cmark_parser_attach_syntax_extension(parser,extension);
    }
    cmark_parser_feed(parser,text,strlen(text));
    cmark_node *document=cmark_parser_finish(parser);
    if (document) {
        char *html=cmark_render_html(document,options,cmark_parser_get_syntax_extensions(parser));
        if (html) { fputs(html,out);free(html); }
        cmark_node_free(document);
    }
    cmark_parser_free(parser);
}
