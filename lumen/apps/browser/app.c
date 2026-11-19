#include "lumen_app.h"
#include "lumen_http.h"
#include "lumen_worker.h"
#include "lumen_markup.h"
#include <ctype.h>
#include <strings.h>
#include "markup.h"

enum { WEB_X=28, WEB_Y=181, WEB_W=544, WEB_H=560, WEB_LINKS=64, WEB_HISTORY=32 };
typedef struct {
    int x,y,w,h;
    char url[WEB_URL];
} WebLink;
typedef struct {
    int page,pages,links;
    char title[128],url[WEB_URL];
    WebLink link[WEB_LINKS];
} WebPage;
static WebPage displayed;
static char history[WEB_HISTORY][WEB_URL],requested[WEB_URL];
static int history_count,history_at=-1,history_target;
static int document_slot=-1,raster_slot=1,work_document,work_raster,work_page;
static int navigating,error;
static pid_t worker;

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void cache_name(char out[24],const char *purpose,int slot) {
    snprintf(out,24,"browser-%s-%d",purpose,slot);
}
static int cache_path(char out[4096],const char *leaf) {
    const char *root=getenv("LUMEN_APP_DATA");
    if(!root || !*root)return 0;
    int n=snprintf(out,4096,"%s/%s",root,leaf);
    return n>0 && n<4096;
}
static int render(const char *html,const char *raster,int page,WebPage *result) {
    const char *tool=getenv("LUMEN_DOCUMENT_TOOL");
    if(!tool || !*tool)return 0;
    char index[16],width[16],height[16];
    snprintf(index,sizeof(index),"%d",page);
    snprintf(width,sizeof(width),"%d",WEB_W);
    snprintf(height,sizeof(height),"%d",WEB_H);
    int output[2];
    if(pipe(output))return 0;
    pid_t child=lumen_worker_start();
    if(!child) {
        close(output[0]);
        if(dup2(output[1],STDOUT_FILENO)<0)_exit(1);
        close(output[1]);
        execl(tool,tool,"--render-document",html,index,width,height,raster,(char *)NULL);
        _exit(1);
    }
    close(output[1]);
    FILE *metadata=fdopen(output[0],"r");
    if(!metadata){close(output[0]);wait_success(child);return 0;}
    char line[WEB_URL+80];
    int valid=child>0 && fgets(line,sizeof(line),metadata) &&
              sscanf(line,"%d",&result->pages)==1 && result->pages>page && result->pages<=4096;
    while(fgets(line,sizeof(line),metadata)) {
        if(!valid || result->links==WEB_LINKS)continue;
        int x,y,right,bottom,offset=0;
        if(sscanf(line,"%d\t%d\t%d\t%d\t%n",&x,&y,&right,&bottom,&offset)!=4 || !offset ||
           x<0 || y<0 || right>WEB_W || bottom>WEB_H || right<=x || bottom<=y)continue;
        char *target=line+offset;
        target[strcspn(target,"\r\n")]=0;
        if(*target!='#' && !url_valid(target))continue;
        WebLink *link=&result->link[result->links++];
        *link=(WebLink){x,y,right-x,bottom-y,""};
        snprintf(link->url,sizeof(link->url),"%s",target);
    }
    fclose(metadata);
    return wait_success(child) && valid;
}
static int prepare(const char *url,const char *html,WebPage *result) {
    char *body=malloc(WEB_BODY+WEB_URL+48);
    if(!body)return 0;
    int ok=lumen_http_page((LumenHttpRequest){.url=url,.redirects=5,.timeout=18,.compressed=1,
        .allow_http=!strncmp(url,"http://",7)},body,WEB_BODY+WEB_URL+48,result->url,sizeof(result->url))==LUMEN_HTTP_OK &&
        url_valid(result->url) && *body && strlen(body)<=WEB_BODY &&
        lumen_storage_write("browser-source.tmp",body,strlen(body));
    free(body);
    char path[4096];
    if(ok)ok=cache_path(path,"browser-source.tmp") && web_document(path,html,result->url,result->title);
    if(ok && !*result->title)snprintf(result->title,sizeof(result->title),"%.127s",result->url);
    lumen_storage_delete("browser-source.tmp");
    return ok;
}
static int begin(int navigate,int page,const char *url,int target) {
    if(worker>0 || (navigate ? !url_valid(url) : document_slot<0 || page<0 || page>=displayed.pages))return 0;
    work_document=navigate ? (document_slot==0 ? 1 : 0) : document_slot;
    work_raster=1-raster_slot;
    work_page=page;navigating=navigate;error=0;history_target=target;
    if(navigate)snprintf(requested,sizeof(requested),"%s",url);
    worker=lumen_worker_start();
    if(worker<0){worker=0;error=1;return 0;}
    if(!worker) {
        char html_name[24],raster_name[24],html[4096],raster[4096];
        snprintf(html_name,sizeof(html_name),"browser-%d.html",work_document);
        cache_name(raster_name,"raster",work_raster);
        int ok=cache_path(raster,raster_name) && cache_path(html,html_name);
        WebPage *result=calloc(1,sizeof(*result));
        if(!result)_exit(1);
        result->page=work_page;
        if(navigate)ok=ok && prepare(requested,html,result);
        else {
            snprintf(result->title,sizeof(result->title),"%s",displayed.title);
            snprintf(result->url,sizeof(result->url),"%s",displayed.url);
        }
        ok=ok && render(html,raster,work_page,result) &&
            lumen_storage_write("browser-result.bin",result,sizeof(*result));
        free(result);_exit(ok ? 0 : 1);
    }
    return 1;
}
static void commit_navigation(void) {
    if(history_target>=0)history_at=history_target;
    else {
        history_count=history_at+1;
        if(history_count==WEB_HISTORY) {
            memmove(history,history+1,(WEB_HISTORY-1)*sizeof(history[0]));
            history_count--;
        }
        history_at=history_count++;
    }
    snprintf(history[history_at],sizeof(history[0]),"%s",displayed.url);
}
static void poll_worker(void) {
    int result=lumen_worker_poll(&worker);
    if(!result)return;
    size_t size;
    WebPage *next=malloc(sizeof(*next));
    int ok=result>0 && next && lumen_storage_read("browser-result.bin",next,sizeof(*next),&size) &&
        size==sizeof(*next) && next->links>=0 && next->links<=WEB_LINKS &&
        next->page==work_page && next->pages>work_page;
    if(ok) {
        displayed=*next;
        document_slot=work_document;raster_slot=work_raster;
        if(navigating)commit_navigation();
    } else error=1;
    free(next);
    lumen_storage_delete("browser-result.bin");
}
static void draw(void) {
    ui_paint_heading("Browser",3);
    if(*displayed.title)lumen_view_title(displayed.title);
    if(worker>0)lumen_icon_action("stop",528,74,IC_CLOSE);
    else if(document_slot>=0)lumen_icon_action("reload",528,74,IC_RESTART);
    if(history_at>0)lumen_icon_action("previous",408,74,IC_PREV);
    if(history_at+1<history_count)lumen_icon_action("forward",468,74,IC_NEXT);
    lumen_button_action("address",28,127,544,45,*displayed.url ? displayed.url :
        word("Webadresse eingeben","Enter a web address","Saisir une adresse web"),0);
    if(document_slot<0) {
        lumen_wrapped(32,280,536,160,worker>0 ?
            word("Seite wird geladen …","Loading page…","Chargement de la page…") :
            error ? word("Die Seite konnte nicht geladen werden.","Could not load the page.","Impossible de charger la page.") :
            word("Eine Seite zum Lesen öffnen.","Open a page to read.","Ouvrez une page à lire."),PAINT_TITLE,4);
    } else {
        char raster[24];cache_name(raster,"raster",raster_slot);
        int top=WEB_Y;
        lumen_bitmap(raster,WEB_X,top,WEB_W,WEB_H,raster_slot);
        if(!worker)for(int i=0;i<displayed.links;i++) {
            WebLink *link=&displayed.link[i];
            int w=link->w<18 ? 18 : link->w,h=link->h<18 ? 18 : link->h;
            int x=link->x+w>WEB_W ? WEB_W-w : link->x;
            int y=link->y+h>WEB_H ? WEB_H-h : link->y;
            lumen_index_action("link",i,WEB_X+x,top+y,w,h);
        }
        lumen_pager(756,displayed.page,displayed.pages);
        if(worker>0 || error) {
            ui_paint_surface(105,692,390,46,12,255,150);
            ui_paint_text(108,693,384,44,worker>0 ?
                word("Wird geladen …","Loading…","Chargement…") :
                word("Seite nicht geladen","Page could not load","Page indisponible"),PAINT_SECONDARY);
        }
    }
    if(worker>0)lumen_schedule(120);
}
static void address_input(void) {
    char address[WEB_URL],url[WEB_URL];
    if(!lumen_string("text",address,sizeof(address)))return;
    char *value=address+strspn(address," \t");
    size_t n=strlen(value);
    while(n && isspace((unsigned char)value[n-1]))value[--n]=0;
    int size=snprintf(url,sizeof(url),"%s%s",strstr(value,"://") ? "" : "https://",value);
    if(size<0 || size>=sizeof(url) || !begin(1,0,url,-1))error=1;
}
static int input(const Input *in) {
    if(in->kind==INPUT_TICK || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH) {
        poll_worker();return 1;
    }
    if(in->kind==INPUT_TEXT && in->x==1){address_input();return 1;}
    if(in->kind==INPUT_PAGE){begin(0,in->x,NULL,-1);return 1;}
    char action[64];
    if(!lumen_action(in,action,"clicked"))return 0;
    if(!strcmp(action,"stop"))lumen_worker_cancel(&worker);
    else if(!strcmp(action,"address") && !worker)
        lumen_request("input",word("Webadresse","Web address","Adresse web"),*displayed.url ? displayed.url : requested,1);
    else if(!strcmp(action,"reload") && document_slot>=0)begin(1,0,displayed.url,history_at);
    else if(!strcmp(action,"previous") && history_at>0)begin(1,0,history[history_at-1],history_at-1);
    else if(!strcmp(action,"forward") && history_at+1<history_count)begin(1,0,history[history_at+1],history_at+1);
    else {
        int link=lumen_action_index(action,"link",displayed.links);
        if(link>=0) {
            const char *url=displayed.link[link].url;
            if(*url=='#')begin(0,atoi(url+1),NULL,-1);
            else begin(1,0,url,-1);
        }
    }
    return 1;
}
int main(void) {
    lumen_run(draw,input,NULL);
    lumen_worker_cancel(&worker);
    return 0;
}
