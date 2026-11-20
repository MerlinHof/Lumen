#include "lumen_app.h"
#include "lumen_http.h"
#include "lumen_worker.h"
#include "lumen_markup.h"
#include <ctype.h>
#include <strings.h>
#include "markup.h"

enum { WEB_X=8, WEB_Y=56, WEB_W=WEB_VIEW_WIDTH, WEB_H=WEB_VIEW_HEIGHT, WEB_LINKS=64, WEB_HISTORY=32, WEB_ACTIONS=96 };
typedef struct {
    int x,y,w,h;
    char url[WEB_URL];
} WebLink;
typedef struct {
    int page,pages,links;
    char title[128],url[WEB_URL];
    WebLink link[WEB_LINKS];
    int field_count;
    WebField fields[WEB_FIELDS];
    char next_url[WEB_URL];
} WebPage;
static WebPage displayed;
static char history[WEB_HISTORY][WEB_URL],requested[WEB_URL];
static int history_count,history_at=-1,history_target;
static int document_slot=-1,raster_slot=1,work_document,work_raster,work_page;
enum {WEB_PAGE, WEB_NAVIGATE, WEB_SCRIPT};
static int operation,error,source_slot=-1,work_source;
static WebAction actions[WEB_ACTIONS],work_action;
static int action_count,editing,edit_submit;
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
static int compare_link(const void *left,const void *right) {
    const WebLink *a=left,*b=right;
    return a->y!=b->y ? a->y-b->y : a->x-b->x;
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
        if(*target!='#' && strncmp(target,"lumen-control:",14) &&
           strncmp(target,"lumen-field:",12) && strncmp(target,"lumen-submit:",13) && !url_valid(target))continue;
        WebLink *link=&result->link[result->links++];
        *link=(WebLink){x,y,right-x,bottom-y,""};
        snprintf(link->url,sizeof(link->url),"%s",target);
    }
    fclose(metadata);
    qsort(result->link,(size_t)result->links,sizeof(result->link[0]),compare_link);
    return wait_success(child) && valid;
}
static int download_page(const char *url,const char *name,WebPage *result) {
    char *body=malloc(WEB_BODY+WEB_URL+48);
    if(!body)return 0;
    int ok=lumen_http_page((LumenHttpRequest){.url=url,.redirects=5,.timeout=18,.compressed=1,.user_agent=mobile_agent,
        .allow_http=!strncmp(url,"http://",7)},body,WEB_BODY+WEB_URL+48,result->url,sizeof(result->url))==LUMEN_HTTP_OK &&
        url_valid(result->url) && *body && strlen(body)<=WEB_BODY &&
        lumen_storage_write(name,body,strlen(body));
    free(body);
    return ok;
}
static int begin(int kind,int page,const char *url,int target) {
    if(worker>0)return 0;
    if(kind==WEB_NAVIGATE ? !url_valid(url) : document_slot<0 || page<0 || page>=displayed.pages)return 0;
    if(kind==WEB_SCRIPT && (work_action.id<1 || work_action.id>=SCRIPT_NODES || action_count==WEB_ACTIONS))return 0;
    work_document=kind==WEB_PAGE ? document_slot : document_slot==0 ? 1 : 0;
    work_source=kind==WEB_NAVIGATE ? (source_slot==0 ? 1 : 0) : source_slot;
    work_raster=1-raster_slot;work_page=page;operation=kind;error=0;
    history_target=target;
    if(kind==WEB_NAVIGATE)snprintf(requested,sizeof(requested),"%s",url);
    worker=lumen_worker_start();
    if(worker<0){worker=0;error=1;return 0;}
    if(!worker) {
        char html_name[24],raster_name[24],source_name[24],html[4096],raster[4096],source[4096];
        snprintf(html_name,sizeof(html_name),"browser-%d.html",work_document);
        cache_name(raster_name,"raster",work_raster);
        cache_name(source_name,"source",work_source);
        int ok=cache_path(raster,raster_name) && cache_path(html,html_name) && cache_path(source,source_name);
        WebPage *result=calloc(1,sizeof(*result));
        if(!result)_exit(1);
        result->page=work_page;
        if(kind==WEB_NAVIGATE)ok=ok && download_page(requested,source_name,result);
        else {
            snprintf(result->title,sizeof(result->title),"%s",displayed.title);
            snprintf(result->url,sizeof(result->url),"%s",displayed.url);
        }
        if(kind!=WEB_PAGE) {
            int replay=0;
            if(kind==WEB_SCRIPT){actions[action_count]=work_action;replay=action_count+1;}
            *result->title=0;
            ok=ok && web_document(source,html,result->url,result->title,actions,replay,
                                  result->fields,&result->field_count,result->next_url);
            if(!*result->title)snprintf(result->title,sizeof(result->title),"%.127s",result->url);
        }
        if(kind==WEB_PAGE) {
            result->field_count=displayed.field_count;
            memcpy(result->fields,displayed.fields,sizeof(result->fields));
        }
        ok=ok && (*result->next_url || render(html,raster,work_page,result)) &&
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
        next->page==work_page && (*next->next_url || next->pages>work_page);
    if(ok && *next->next_url) {
        char url[WEB_URL];snprintf(url,sizeof(url),"%s",next->next_url);
        lumen_storage_delete("browser-result.bin");free(next);
        if(!begin(WEB_NAVIGATE,0,url,-1))error=1;
        return;
    }
    if(ok) {
        displayed=*next;
        document_slot=work_document;raster_slot=work_raster;
        if(operation==WEB_NAVIGATE) {
            source_slot=work_source;action_count=0;
            commit_navigation();
        } else if(operation==WEB_SCRIPT)actions[action_count++]=work_action;
    } else error=1;
    free(next);
    lumen_storage_delete("browser-result.bin");
}
static void draw(void) {
    if(*displayed.title)lumen_view_title(displayed.title);
    if(history_at>0)lumen_icon_action("previous",8,750,IC_PREV);
    if(history_at+1<history_count)lumen_icon_action("forward",55,750,IC_NEXT);
    const char *address=*displayed.url ? displayed.url : requested;
    const char *separator=strstr(address,"://");
    if(separator)address=separator+3;
    lumen_button_action("address",103,750,344,44,*address ? address :
        word("Adresse oder Suche","Address or search","Adresse ou recherche"),0);
    if(displayed.page>0)lumen_icon_action("page-prev",451,750,IC_PREV);
    if(displayed.page+1<displayed.pages)lumen_icon_action("page-next",499,750,IC_NEXT);
    if(worker>0)lumen_icon_action("stop",548,750,IC_CLOSE);
    else if(document_slot>=0)lumen_icon_action("reload",548,750,IC_RESTART);
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
    if(!*value)return;
    const char *slash=strchr(value,'/');
    const char *dot=strchr(value,'.');
    int size;
    if(!strstr(value,"://") && ((!dot && strcmp(value,"localhost")) ||
       (slash && dot && dot>slash) || strpbrk(value," \t"))) {
        char query[WEB_URL-45];
        if(!lumen_url_encode(value,query,sizeof(query))){error=1;return;}
        size=snprintf(url,sizeof(url),"https://www.google.com/search?q=%s",query);
    } else size=snprintf(url,sizeof(url),"%s%s",strstr(value,"://") ? "" : "https://",value);
    if(size<0 || size>=sizeof(url) || !begin(WEB_NAVIGATE,0,url,-1))error=1;
}
static int input(const Input *in) {
    if(in->kind==INPUT_TICK || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH) {
        poll_worker();return 1;
    }
    if(in->kind==INPUT_TEXT && in->x==1){address_input();return 1;}
    if(in->kind==INPUT_TEXT && in->x==2 && editing) {
        work_action=(WebAction){.id=editing,.kind=edit_submit ? WEB_SEARCH : WEB_EDIT};
        editing=0;
        if(lumen_string("text",work_action.value,sizeof(work_action.value)))
            begin(WEB_SCRIPT,displayed.page,NULL,-1);
        return 1;
    }
    if(in->kind==INPUT_SWIPE && (abs(in->dy)>50 || abs(in->dx)>50)) {
        int delta=(abs(in->dx)>abs(in->dy) ? in->dx : in->dy)<0 ? 1 : -1;
        begin(WEB_PAGE,displayed.page+delta,NULL,-1);return 1;
    }
    if(in->kind==INPUT_PAGE){begin(WEB_PAGE,in->x,NULL,-1);return 1;}
    char action[64];
    if(!lumen_action(in,action,"clicked"))return 0;
    if(!strcmp(action,"stop"))lumen_worker_cancel(&worker);
    else if(!strcmp(action,"address") && !worker)
        lumen_request("input",word("Webadresse","Web address","Adresse web"),*displayed.url ? displayed.url : requested,1);
    else if(!strcmp(action,"page-prev"))begin(WEB_PAGE,displayed.page-1,NULL,-1);
    else if(!strcmp(action,"page-next"))begin(WEB_PAGE,displayed.page+1,NULL,-1);
    else if(!strcmp(action,"reload") && document_slot>=0)begin(WEB_NAVIGATE,0,displayed.url,history_at);
    else if(!strcmp(action,"previous") && history_at>0)begin(WEB_NAVIGATE,0,history[history_at-1],history_at-1);
    else if(!strcmp(action,"forward") && history_at+1<history_count)begin(WEB_NAVIGATE,0,history[history_at+1],history_at+1);
    else {
        int link=lumen_action_index(action,"link",displayed.links);
        if(link>=0) {
            const char *url=displayed.link[link].url;
            if(*url=='#')begin(WEB_PAGE,atoi(url+1),NULL,-1);
            else if(!strncmp(url,"lumen-field:",12)) {
                int id=atoi(url+12);
                for(int i=0;i<displayed.field_count;i++)if(displayed.fields[i].id==id) {
                    editing=id;edit_submit=displayed.fields[i].submit;
                    lumen_request("input",displayed.fields[i].label,displayed.fields[i].value,2);
                    break;
                }
            } else if(!strncmp(url,"lumen-control:",14) || !strncmp(url,"lumen-submit:",13)) {
                int submit=url[6]=='s';
                work_action=(WebAction){.id=atoi(url+(submit ? 13 : 14)),.kind=submit ? WEB_SUBMIT : WEB_CLICK};
                if(!begin(WEB_SCRIPT,displayed.page,NULL,-1))error=1;
            } else begin(WEB_NAVIGATE,0,url,-1);
        }
    }
    return 1;
}
int main(void) {
    lumen_run(draw,input,NULL);
    lumen_worker_cancel(&worker);
    return 0;
}
