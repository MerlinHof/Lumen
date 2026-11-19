/* Bounded document helper, run in a child by a package. */
#include <mupdf/fitz.h>
#include <ctype.h>
#include <strings.h>

static const char *html_tag17(fz_xml *x) {
    const char *s = fz_xml_tag(x);
    return s ? s : "";
}
static void html_text(FILE *out,const char *text) {
    for(;*text;text++) {
        if(*text=='&')fputs("&amp;",out);
        else if(*text=='<')fputs("&lt;",out);
        else if(*text=='>')fputs("&gt;",out);
        else fputc((unsigned char)*text,out);
    }
}
static void recipe_flat17(fz_xml *x, char *out, int cap, int depth) {
    if (depth > 25)
        return;
    for (; x; x = fz_xml_next(x)) {
        const char *t = fz_xml_text(x);
        if (t) {
            int n = strlen(out);
            snprintf(out + n, cap - n, "%s", t);
        } else
            recipe_flat17(fz_xml_down(x), out, cap, depth + 1);
    }
}
static void clean_fragment(FILE *f,fz_xml *x,int depth,char *image,int cap,const char *title) {
    if (depth > 40)
        return;
    for (; x; x = fz_xml_next(x)) {
        const char *text = fz_xml_text(x);
        if (text) {
            html_text(f,text);
            continue;
        }
        const char *tag = html_tag17(x), *class = fz_xml_att(x, "class");
        if (!strcmp(tag, "h1") || !strcmp(tag, "h2")) {
            char heading[512] = "";
            recipe_flat17(fz_xml_down(x), heading, sizeof(heading), 0);
            size_t len = strlen(heading);
            while (len && isspace((unsigned char)heading[len - 1]))
                heading[--len] = 0;
            const char *start = heading;
            while (*start && isspace((unsigned char)*start))
                start++;
            if (!strcasecmp(start, title))
                continue;
        }

        if (!strcmp(tag, "script") || !strcmp(tag, "style") || !strcmp(tag, "form") ||
            !strcmp(tag, "nav") ||
            (class &&
             (strstr(class, "mw-editsection") || strstr(class, "navbox") ||
              strstr(class, "metadata") || strstr(class, "toc") || strstr(class, "noprint"))))
            continue;
        if (!strcmp(tag, "img") && !*image) {
            const char *src = fz_xml_att(x, "src");
            if(src && (!strncmp(src,"//",2) || !strncmp(src,"https://",8)) &&
               !strpbrk(src,"\r\n\t "))
                snprintf(image, cap, "%s%s", src[0] == '/' ? "https:" : "", src);
        }
        const char *allowed[] = {"p",  "h1",    "h2",    "h3",         "h4",  "ul",   "ol",
                                 "li", "table", "thead", "tbody",      "tr",  "th",   "td",
                                 "dl", "dt",    "dd",    "blockquote", "pre", "code", "strong",
                                 "b",  "em",    "i",     "br",         NULL};
        int keep = 0;
        for (int i = 0; allowed[i]; i++)
            if (!strcmp(tag, allowed[i]))
                keep = 1;
        if (keep)
            fprintf(f, "<%s>", tag);
        clean_fragment(f,fz_xml_down(x),depth+1,image,cap,title);
        if (keep && strcmp(tag, "br"))
            fprintf(f, "</%s>", tag);
    }
}

static int sanitize_html(const char *title) {
    const size_t limit=3000000;
    unsigned char *data=malloc(limit+1);
    if(!data)return 0;
    size_t length=fread(data,1,limit+1,stdin);
    if(length>limit || ferror(stdin)){free(data);return 0;}
    fz_context *c=fz_new_context(NULL,NULL,4*1024*1024);
    if(!c){free(data);return 0;}
    fz_xml *xml=NULL;
    fz_buffer *buffer=NULL;
    char *body=NULL,image[2048]="";
    size_t size=0;
    FILE *fragment=open_memstream(&body,&size);
    int ok=0;
    fz_var(xml);fz_var(buffer);fz_var(ok);
    if(fragment) {
        fz_try(c) {
            buffer=fz_new_buffer_from_copied_data(c,data,length);
            xml=fz_parse_xml_from_html5(c,buffer);
            clean_fragment(fragment,fz_xml_root(xml),0,image,sizeof(image),title);
            ok=!ferror(fragment);
        }
        fz_always(c) {fz_drop_xml(c,xml);fz_drop_buffer(c,buffer);}
        fz_catch(c) {ok=0;}
        if(fclose(fragment))ok=0;
        if(ok && size<=6000000)ok=printf("%s\n",image)>0 && fwrite(body,1,size,stdout)==size;
        else ok=0;
    }
    free(body);free(data);fz_drop_context(c);
    return ok;
}
