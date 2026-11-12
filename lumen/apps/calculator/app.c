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
#define region ui_paint_hitbox
#define key_outline(x,y,w,h) ui_paint_surface(x,y,w,h,10,255,105)
#define centered(x,y,w,h,text,role) ui_paint_text(x,y,w,h,tr(text),role)
#define label_at(x,y,w,h,text,role) lumen_text(x,y,w,h,text,role,0,1)
#define drawtext_raw(x,y,text,role,w) lumen_text(x,y,w,24,text,role,0,1)
#define drawtext(x,y,text,role,w) drawtext_raw(x,y,tr(text),role,w)
#define wrapped(x,y,text,role,w,lines) lumen_wrapped(x,y,w,lines*28,tr(text),role,lines)
#define rect(x,y,w,h,gray) ui_paint_surface(x,y,w,h,0,gray,-1)
#define stroke(x,y,u,v,w) ui_paint_line(x,y,u,v,w,0)
#define icon ui_paint_icon_button
#define icon_button ui_paint_icon_button
#define button_h(x,y,w,h,text,active) lumen_button(x,y,w,h,tr(text),active)
#define filled_button(x,y,w,h,text,role) lumen_button(x,y,w,h,text,1)
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
        lumen_select_row(203,word("Winkelmaß","Angle unit","Unité d’angle"),calc_degrees ? "DEG" : "RAD");
        lumen_select_row(271,word("Ergebnisstellen","Significant digits","Chiffres significatifs"),value);
        lumen_button(28,367,544,48,word("Verlauf löschen","Clear history","Effacer l’historique"),0);
    } else app_calculator();
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){load();return 1;}
    if(in->kind==INPUT_BACK){set_page(page==RANGE ? GRAPH : CALCULATOR);return 1;}
    if(in->kind==INPUT_PAGE && page==HISTORY){extra_page=in->x;return 1;}
    if(in->kind==INPUT_SERVICE && in->x==7 && in->y) {
        int offset=lumen_number("offset");
        calc_cursor=boundary(calc_expr,offset<0 ? 0 : (size_t)offset);
        return 1;
    }
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
    if(in->kind==INPUT_HOLD && page==CALCULATOR && in->x>=28 && in->x<526 && in->y>=361 && in->y<413){
        edit(80,"Rechner",calc_expr);return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(page==SETTINGS) {
        if(x>=360 && y>=203 && y<251){const char *choices[]={"DEG","RAD"};lumen_select(choices,2,!calc_degrees,1);}
        else if(x>=360 && y>=271 && y<319){const char *choices[]={"6","10","15"};lumen_select(choices,3,calc_precision16==6 ? 0 : calc_precision16==15 ? 2 : 1,2);}
        else if(y>=367 && y<415){calc_history_count=0;lumen_storage_write("calculator-history.txt","",0);}
        return 1;
    }
    if (page == CALCULATOR) {
        if (y >= 74 && y < 118 && x >= 408 && x < 460) {
            if (!*calc_expr)
                snprintf(calc_expr, sizeof(calc_expr), "sin(x)");
            set_page(GRAPH);
            return 1;
        }
        if (y >= 74 && y < 118 && x >= 460 && x < 516) {
            extra_page=0;set_page(HISTORY);
            return 1;
        }
        if(y>=74 && y<118 && x>=528){set_page(SETTINGS);return 1;}
        if (y >= 361 && y < 413 && x >= 528) {
            calc_backspace();
            return 1;
        }
        if (y >= 361 && y < 413) {
            char target[1100];
            snprintf(target,sizeof(target),"%d %d %s",x-43,calc_cursor,calc_expr);
            lumen_service("editor","caret",target,7);
            return 1;
        }
        if (y >= 429 && y < 531 && x >= 28 && x < 572) {
            if ((!calc_alt && x >= 534) || (calc_alt && x < 66)) {
                calc_alt ^= 1;
                return 1;
            }
            int r = (y - 429) / 51, c = (x - calc_function_left()) / calc_function_pitch();
            if (c >= 0 && c < 8 && y < 429 + r * 51 + 44 &&
                x < calc_function_left() + c * calc_function_pitch() + 57) {
                const char *key = (calc_alt ? calc_all1 : calc_all0)[r * 8 + c];
                if (!strcmp(key, "M+")) {
                    double value;
                    char err[80];
                    calc_memory += calc_evaluate(calc_expr, calc_degrees, calc_answer, calc_memory,
                                                 &value, err, sizeof(err))
                                       ? value
                                       : calc_answer;
                } else if (!strcmp(key, "MC"))
                    calc_memory = 0;
                else if (isalpha((unsigned char)key[0]) && strcmp(key, "pi") && strcmp(key, "e") &&
                         strcmp(key, "ans") && strcmp(key, "mem") && strcmp(key, "x") &&
                         strcmp(key, "y")) {
                    calc_insert(key);
                    calc_insert("(");
                } else
                    calc_insert(key);
            }
            persist();
            return 1;
        }
        if (y >= 540 && y < 782) {
            if (x >= 460) {
                if (y >= 664)
                    {calc_run();persist();}
                else if (y < 596) {
                    if (!*calc_expr)
                        calc_insert("-");
                    else if (strlen(calc_expr) < sizeof(calc_expr) - 4) {
                        char opposite[1024];
                        snprintf(opposite, sizeof(opposite), "-(%s)", calc_expr);
                        snprintf(calc_expr, sizeof(calc_expr), "%s", opposite);
                        calc_cursor = strlen(calc_expr);
                        calc_preview();
                    }
                } else if (y >= 602 && y < 658) {
                    calc_expr[0] = calc_result[0] = calc_error[0] = 0;
                    calc_cursor = 0;
                }
            } else if (x >= 28 && x < 452) {
                int c = (x - 28) / 108, r = (y - 540) / 62;
                if (c < 4 && r < 4 && x < 28 + c * 108 + 100 && y < 540 + r * 62 + 56)
                    calc_insert(calc_digits[r * 4 + c]);
            }
            return 1;
        }
        return 1;
    }
    if (page == HISTORY) {
        if (y >= 145 && y < 657) {
            int i = calc_history_count - 1 - (extra_page * 8 + (y - 145) / 64);
            if (i >= 0) {
                snprintf(calc_expr, sizeof(calc_expr), "%s", calc_history[i]);
                calc_cursor = strlen(calc_expr);
                set_page(CALCULATOR);
                calc_preview();
                calc_error[0] = 0;
            }
        } else if (y >= 679 && y < 723) {
            if (x < 300 && extra_page)
                extra_page--;
            else if (x >= 300 && (extra_page + 1) * 8 < calc_history_count)
                extra_page++;
        }
        return 1;
    }
    if (page == GRAPH) {
        if (y >= 128 && y < 178)
            edit(80, "y = f(x)", calc_expr);
        else if (y >= 220 && y < 647 && x >= 66 && x < 558)
            plot_trace = x - 66 + 1;
        else if (y >= 712) {
            if (x < 178)
                set_page(RANGE);
            else if (x >= 416)
                graph_auto_y();
            else {
                double f = x < 300 ? 2 : .5, cx = (plot_bounds[0] + plot_bounds[1]) / 2,
                       cy = (plot_bounds[2] + plot_bounds[3]) / 2,
                       dx = (plot_bounds[1] - plot_bounds[0]) * f / 2,
                       dy = (plot_bounds[3] - plot_bounds[2]) * f / 2;
                if (dx > 1e-9 && dx < 1e12 && dy > 1e-9 && dy < 1e12) {
                    plot_bounds[0] = cx - dx;
                    plot_bounds[1] = cx + dx;
                    plot_bounds[2] = cy - dy;
                    plot_bounds[3] = cy + dy;
                }
            }
        }
        return 1;
    }
    if (page == RANGE) {
        if (y >= 165 && y < 549) {
            int i = (y - 165) / 108;
            if (i < 4) {
                char b[80];
                snprintf(b, sizeof(b), "%.10g", plot_bounds[i]);
                edit(81 + i, "Graph-Bereich", b);
            }
        } else if (y >= 650 && y < 706)
            set_page(GRAPH);
        return 1;
    }
    return 0;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
