#include "lumen_app.h"
#include "api.h"
#include <ctype.h>
#include <math.h>
#include "state.h"

static int calc_precision16=10,extra_page;
enum {CALCULATOR, HISTORY, SETTINGS, GRAPH, RANGE};
static int page;
static void set_page(int next){page=next;lumen_back=page!=CALCULATOR;}
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static const char *tr(const char *text) {
    static const char *labels[][3]={
        {"Rechner","Calculator","Calculatrice"},{"Rechenverlauf","History","Historique"},
        {"Funktionsgraph","Function plot","Graphe"},{"Bereich","Range","Intervalle"},
        {"Graph-Bereich","Plot range","Intervalle du graphe"},{"Fertig","Done","Terminé"},
        {"Ausdruck oder Wertebereich prüfen.","Check expression or range.","Vérifiez l’expression ou l’intervalle."}
    };
    for(size_t i=0;i<sizeof(labels)/sizeof(*labels);i++)
        if(!strcmp(text,labels[i][0]))return word(labels[i][0],labels[i][1],labels[i][2]);
    return text;
}
static void edit(int action,const char *label,const char *text){lumen_request("input",tr(label),text,action);}
static size_t boundary(const char *text,size_t at) {
    size_t length=strlen(text);
    if(at>length)at=length;
    while(at && ((unsigned char)text[at]&192)==128)at--;
    return at;
}
static int text_insert(char *text,size_t capacity,int *cursor,const char *value) {
    size_t n=strlen(text),add=strlen(value),at=boundary(text,*cursor);
    if(add>=capacity-n)return 0;
    memmove(text+at+add,text+at,n-at+1);
    memcpy(text+at,value,add);
    *cursor=at+add;
    return 1;
}
static void text_backspace(char *text,int *cursor) {
    size_t at=boundary(text,*cursor);
    if(!at)return;
    size_t start=boundary(text,at-1);
    memmove(text+start,text+at,strlen(text+at)+1);
    *cursor=start;
}
enum {UI_SECONDARY=PAINT_SECONDARY,UI_PRIMARY=PAINT_BODY,UI_BODY=PAINT_BODY};
#define ui_font(role) (role)
#define heading_actions(title,count) ui_paint_heading(tr(title),count)
#define heading(title) ui_paint_heading(tr(title),0)
#define key_outline(x,y,w,h) ui_paint_surface(x,y,w,h,10,255,105)
#define centered(x,y,w,h,text,role) ui_paint_text(x,y,w,h,tr(text),role)
#define label_at(x,y,w,h,text,role) lumen_text(x,y,w,h,text,role,0,1)
#define drawtext_raw(x,y,text,role,w) lumen_text(x,y,w,24,text,role,0,1)
#define drawtext(x,y,text,role,w) drawtext_raw(x,y,tr(text),role,w)
#define wrapped(x,y,text,role,w,lines) lumen_wrapped(x,y,w,lines*28,tr(text),role,lines)
#define rect(x,y,w,h,gray) ui_paint_surface(x,y,w,h,0,gray,-1)
#define stroke(x,y,u,v,w) ui_paint_line(x,y,u,v,w,0)
#include "view.h"
#include "plots.h"

static void load(void) {
    char state[17000];
    size_t size;
    if(lumen_storage_read("calculator-state.txt",state,sizeof(state)-1,&size) ||
       lumen_legacy_read("calculator-state.txt",state,sizeof(state)-1,&size)) {
        state[size]=0;
        double answer,memory;
        int degrees;
        if(sscanf(state,"%lf %lf %d",&answer,&memory,&degrees)==3 && isfinite(answer) && isfinite(memory)){
            calc_answer=answer;calc_memory=memory;calc_degrees=!!degrees;
        }
    }
    if(lumen_storage_read("calc-precision16.txt",state,20,&size) ||
       lumen_legacy_read("calc-precision16.txt",state,20,&size)) {
        state[size]=0;int n=atoi(state);if(n>=3 && n<=15)calc_precision16=n;
    }
    if(lumen_storage_read("calculator-history.txt",state,sizeof(state)-1,&size) ||
       lumen_legacy_read("calculator-history.txt",state,sizeof(state)-1,&size)) {
        state[size]=0;
        char *save;
        for(char *row=strtok_r(state,"\n",&save);row && calc_history_count<16;row=strtok_r(NULL,"\n",&save))
            snprintf(calc_history[calc_history_count++],1024,"%s",row);
    }
}
static void persist(void) {
    char state[128];snprintf(state,sizeof(state),"%.17g %.17g %d\n",calc_answer,calc_memory,calc_degrees);
    lumen_storage_write("calculator-state.txt",state,strlen(state));
    snprintf(state,sizeof(state),"%d",calc_precision16);
    lumen_storage_write("calc-precision16.txt",state,strlen(state));
}
static void draw(void) {
    if(page==HISTORY)app_calc_history();
    else if(page==GRAPH)calculator_graph();
    else if(page==RANGE)graph_range_screen();
    else if(page==SETTINGS) {
        char value[16];snprintf(value,sizeof(value),"%d",calc_precision16);
        heading(word("Rechner · Einstellungen","Calculator settings","Réglages de la calculatrice"));
        lumen_select_action("angle",203,word("Winkelmaß","Angle unit","Unité d’angle"),calc_degrees ? "DEG" : "RAD");
        lumen_select_action("precision",271,word("Ergebnisstellen","Significant digits","Chiffres significatifs"),value);
        lumen_button_action("clear-history",28,367,544,48,word("Verlauf löschen","Clear history","Effacer l’historique"),0);
    } else app_calculator();
}
static int action_index(const char *id,const char *prefix,int count) {
    size_t length=strlen(prefix);
    if(strncmp(id,prefix,length) || !isdigit((unsigned char)id[length]))return -1;
    char *end;long index=strtol(id+length,&end,10);
    return *end || index<0 || index>=count ? -1 : (int)index;
}
static void function_key(const char *key) {
    if (!strcmp(key,"M+")) {
        double value;char error[80];
        calc_memory+=calc_evaluate(calc_expr,calc_degrees,calc_answer,calc_memory,&value,error,sizeof(error))
                     ? value : calc_answer;
    } else if (!strcmp(key,"MC"))calc_memory=0;
    else {
        calc_insert(key);
        if(isalpha((unsigned char)key[0]) && strcmp(key,"pi") && strcmp(key,"e") &&
           strcmp(key,"ans") && strcmp(key,"mem") && strcmp(key,"x") && strcmp(key,"y"))
            calc_insert("(");
    }
    persist();
}
static void graph_zoom(double factor) {
    double cx=(plot_bounds[0]+plot_bounds[1])/2,cy=(plot_bounds[2]+plot_bounds[3])/2,
           dx=(plot_bounds[1]-plot_bounds[0])*factor/2,dy=(plot_bounds[3]-plot_bounds[2])*factor/2;
    if(dx>1e-9 && dx<1e12 && dy>1e-9 && dy<1e12) {
        plot_bounds[0]=cx-dx;plot_bounds[1]=cx+dx;
        plot_bounds[2]=cy-dy;plot_bounds[3]=cy+dy;
    }
}
static int component(const char *id) {
    if(page==SETTINGS) {
        if(!strcmp(id,"angle")){const char *choices[]={"DEG","RAD"};lumen_select(choices,2,!calc_degrees,1);}
        else if(!strcmp(id,"precision")) {
            const char *choices[]={"6","10","15"};
            lumen_select(choices,3,calc_precision16==6 ? 0 : calc_precision16==15 ? 2 : 1,2);
        } else if(!strcmp(id,"clear-history")){calc_history_count=0;lumen_storage_write("calculator-history.txt","",0);}
        return 1;
    }
    if(page==CALCULATOR) {
        int function=action_index(id,"function-",16),digit=action_index(id,"digit-",16);
        if(function>=0)function_key((calc_alt ? calc_all1 : calc_all0)[function]);
        else if(digit>=0)calc_insert(calc_digits[digit]);
        else if(!strcmp(id,"graph")) {
            if(!*calc_expr)snprintf(calc_expr,sizeof(calc_expr),"sin(x)");
            set_page(GRAPH);
        } else if(!strcmp(id,"history")){extra_page=0;set_page(HISTORY);}
        else if(!strcmp(id,"settings"))set_page(SETTINGS);
        else if(!strcmp(id,"erase"))calc_backspace();
        else if(!strcmp(id,"functions"))calc_alt^=1;
        else if(!strcmp(id,"equals")){calc_run();persist();}
        else if(!strcmp(id,"clear")){calc_expr[0]=calc_result[0]=calc_error[0]=0;calc_cursor=0;}
        else if(!strcmp(id,"sign")) {
            if(!*calc_expr)calc_insert("-");
            else if(strlen(calc_expr)<sizeof(calc_expr)-4) {
                char opposite[1024];
                snprintf(opposite,sizeof(opposite),"-(%s)",calc_expr);
                snprintf(calc_expr,sizeof(calc_expr),"%s",opposite);
                calc_cursor=strlen(calc_expr);calc_preview();
            }
        }
        return 1;
    }
    if(page==HISTORY) {
        int row=action_index(id,"history-",8);
        int index=calc_history_count-1-(extra_page*8+row);
        if(row>=0 && index>=0) {
            snprintf(calc_expr,sizeof(calc_expr),"%s",calc_history[index]);
            calc_cursor=strlen(calc_expr);set_page(CALCULATOR);calc_preview();calc_error[0]=0;
        }
    } else if(page==GRAPH) {
        if(!strcmp(id,"expression"))edit(80,"y = f(x)",calc_expr);
        else if(!strcmp(id,"range"))set_page(RANGE);
        else if(!strcmp(id,"auto-y"))graph_auto_y();
        else if(!strcmp(id,"zoom-out"))graph_zoom(2);
        else if(!strcmp(id,"zoom-in"))graph_zoom(.5);
    } else if(page==RANGE) {
        int bound=action_index(id,"bound-",4);
        if(bound>=0) {
            char value[80];snprintf(value,sizeof(value),"%.10g",plot_bounds[bound]);
            edit(81+bound,"Graph-Bereich",value);
        } else if(!strcmp(id,"done"))set_page(GRAPH);
    }
    return 1;
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){load();return 1;}
    if(in->kind==INPUT_BACK){set_page(page==RANGE ? GRAPH : CALCULATOR);return 1;}
    if(in->kind==INPUT_PAGE && page==HISTORY){extra_page=in->x;return 1;}
    if(in->kind==INPUT_SELECT && page==SETTINGS) {
        if(in->x==1 && in->y>=0 && in->y<2)calc_degrees=in->y==0;
        if(in->x==2 && in->y>=0 && in->y<3)calc_precision16=(int[]){6,10,15}[in->y];
        persist();return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char value[1024];if(!lumen_string("text",value,sizeof(value)))return 0;
        if(in->x==80){snprintf(calc_expr,sizeof(calc_expr),"%s",value);calc_cursor=strlen(calc_expr);calc_preview();}
        if(in->x>=81 && in->x<=84) {
            char *end;double n=strtod(value,&end);
            int i=in->x-81;double old=plot_bounds[i];
            if(end!=value && !*end && isfinite(n) && fabs(n)<=1e12) {
                plot_bounds[i]=n;
                if(plot_bounds[0]>=plot_bounds[1] || plot_bounds[2]>=plot_bounds[3])plot_bounds[i]=old;
            }
        }
        return 1;
    }
    if(in->kind==INPUT_COMPONENT) {
        char id[32],gesture[24];
        if(!lumen_string("id",id,sizeof(id)) || !lumen_string("gesture",gesture,sizeof(gesture)))return 0;
        if(page==CALCULATOR && !strcmp(id,"expression-input")) {
            if(!strcmp(gesture,"long_pressed"))edit(80,"Rechner",calc_expr);
            else if(!strcmp(gesture,"caret")) {
                char offset[16],*end;
                if(lumen_string("text",offset,sizeof(offset))) {
                    long value=strtol(offset,&end,10);
                    if(end!=offset && !*end && value>=0)calc_cursor=boundary(calc_expr,(size_t)value);
                }
            }
            return 1;
        }
        return !strcmp(gesture,"clicked") && component(id);
    }
    /* A trace samples the plotted function at the tapped coordinate. */
    if(page==GRAPH && in->kind==INPUT_TAP && in->y>=220 && in->y<647 && in->x>=66 && in->x<558) {
        plot_trace=in->x-66+1;return 1;
    }
    return 0;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
