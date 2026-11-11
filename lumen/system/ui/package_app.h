/* All installed app packages use the same system controls and input bridge. */
static JsonToken package_tokens[32768];
static int package_value(Json*j,int obj,const char*key){double v=json_number(j,json_get(j,obj,key));return isfinite(v)&&v>=-100000&&v<=100000?(int)v:0;}
static int package_input_action, package_select_action;
static int64_t package_deadline;
static int package_timer_owner=-1;
static int package_clock_wait(void) {
    if(!package_deadline || package_timer_owner!=(int)app_for_route(ui_route()))return -1;
    int64_t delta=package_deadline-extra_millis();
    return delta>0 ? (int)delta : 0;
}
static void package_clock_tick(void) {
    if(package_clock_wait()==0) {
        package_deadline=0;
        app_process_send("{\"event\":\"tick\"}");
    }
}
static struct { int y,page,count; } package_pager;
static void package_asset_paint(const char *name,int x,int y,int w,int h) {
    char path[2048];
    if(w<1 || h<1 || w>WIDTH || h>HEIGHT ||
       !app_package_asset(app_for_route(ui_route()),name,path,sizeof(path)))return;
    FILE *file=fopen(path,"rb");
    if(!file)return;
    int aw,ah,range;
    if(fscanf(file,"P5 %d %d %d",&aw,&ah,&range)!=3 || fgetc(file)!='\n' ||
       aw!=w || ah!=h || range!=255){fclose(file);return;}
    unsigned char row[WIDTH];
    int left=x<0 ? -x : 0,right=x+w>WIDTH ? WIDTH-x : w;
    for(int r=0;r<h && fread(row,1,w,file)==(size_t)w;r++)
        if(y+r>=STATUS_HEIGHT && y+r<HEIGHT && right>left)
            memcpy(ui_pixels+(y+r)*WIDTH+x+left,row+left,right-left);
    fclose(file);
}
static char package_options[48][128];
static const char *package_labels[48];
static void package_ui_choice(int choice){char event[128];snprintf(event,sizeof(event),"{\"event\":\"select\",\"action\":%d,\"value\":%d}",package_select_action,choice);app_process_send(event);}
typedef struct {
    int top,height,field;
    char id[64],label[120],value[512];
} PackageComponent;
static PackageComponent package_components[16];
static int package_component_count,package_component_layout,package_component_top;
static char package_focused_component[64];
static void package_component_event(const char *id,const char *text) {
    char *message=NULL;size_t length=0;
    FILE *out=open_memstream(&message,&length);
    if(!out)return;
    fprintf(out,"{\"event\":\"component\",\"changed\":%d,\"id\":",text!=NULL);
    json_escape16(out,id);
    if(text){fputs(",\"text\":",out);json_escape16(out,text);}
    fputc('}',out);fclose(out);
    app_process_send(message);free(message);
}
static void package_paint_frame(const char *text) {
    Json j;if(!json_parse(&j,text,package_tokens,32768))return;
    int commands=json_get(&j,0,"ui");if(commands<0 || j.tokens[commands].type!='[' || j.tokens[commands].count>2048)return;
    size_t cost=0;
    for(int item=commands+1;item<j.tokens[commands].next;item=j.tokens[item].next) {
        char type[32],label[2048];
        json_string(&j,json_get(&j,item,"type"),type,sizeof(type));json_string(&j,json_get(&j,item,"text"),label,sizeof(label));
        int x=package_value(&j,item,"x"),y=package_value(&j,item,"y"),w=package_value(&j,item,"w"),h=package_value(&j,item,"h");
        if(!strcmp(type,"page_layout")) {
            package_component_layout=1;package_component_top=141;
            heading_actions(label,0);continue;
        }
        int field=!strcmp(type,"component_field");
        if(field || !strcmp(type,"component_button")) {
            int height=field ? 70 : 50;
            if(!package_component_layout || package_component_count>=16 ||
               package_component_top+height>HEIGHT-8)continue;
            PackageComponent *component=&package_components[package_component_count];
            json_string(&j,json_get(&j,item,"id"),component->id,sizeof(component->id));
            if(!*component->id)continue;
            component->top=package_component_top;component->height=height;component->field=field;
            snprintf(component->label,sizeof(component->label),"%.*s",(int)sizeof(component->label)-1,label);
            if(field) {
                json_string(&j,json_get(&j,item,"value"),component->value,sizeof(component->value));
                type_label(28,component->top,544,22,label,UI_SECONDARY);
                control_surface(28,component->top+22,544,44,RADIUS_SMALL,236);
                label_ellipsis(40,component->top+22,520,44,component->value,ui_font(UI_BODY),0);
                region(28,component->top,544,height);
            } else button_h(28,component->top,544,height,label,package_value(&j,item,"primary"));
            package_component_top+=height+12;package_component_count++;continue;
        }
        /* Content coordinates may never paint or intercept the status bar. */
        if((y<STATUS_HEIGHT && strcmp(type,"asset")) || y<0 || y>800 || x < -600 || x>600 || w<0 || w>1200 || h<0 || h>800)continue;
        size_t estimate=(size_t)w*h;
        if(!strcmp(type,"ellipse"))estimate=4u*(unsigned)abs(package_value(&j,item,"rx"))*(unsigned)abs(package_value(&j,item,"ry"));
        if(!strcmp(type,"hex_cell"))estimate=65*37;
        if(!strcmp(type,"ring")){int r=package_value(&j,item,"radius");estimate=4u*(unsigned)abs(r)*(unsigned)abs(r);}
        if(!strcmp(type,"line"))estimate=(size_t)(abs(x-package_value(&j,item,"x2"))+32)*(abs(y-package_value(&j,item,"y2"))+32);
        if(estimate>4000000 || cost+estimate>4000000)break;cost+=estimate;
        if(!strcmp(type,"asset"))package_asset_paint(label,x,y,w,h);
        else if(!strcmp(type,"qr")) {
            if(w<80 || w>320 || h!=w)continue;
            uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)],tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
            if(qrcodegen_encodeText(label,tmp,qr,qrcodegen_Ecc_MEDIUM,1,10,qrcodegen_Mask_AUTO,true)) {
                int n=qrcodegen_getSize(qr),step=w/(n+8),offset=(w-step*(n+8))/2+4*step;
                for(int row=0;row<n;row++)for(int col=0;col<n;col++)
                    if(qrcodegen_getModule(qr,col,row))
                        rect(x+offset+col*step,y+offset+row*step,step,step,0);
            }
        }
        else if(!strcmp(type,"app_tile")) {
            int id=app_package_find(label);
            if(id>=0 && app_definition(id)->entry>=0 && w>=90 && w<=WIDTH)
                app_tile(x,y,w,id);
        }
        else if(!strcmp(type,"pager")) {
            int page=package_value(&j,item,"page"), count=package_value(&j,item,"count");
            if(page>=0 && count>1 && count<=4096 && page<count && y+44<=HEIGHT) {
                package_pager.y=y;package_pager.page=page;package_pager.count=count;
                pager_draw(PAGER_X,y,PAGER_W,page,count);
            }
        }
        else if(!strcmp(type,"raised"))raised_surface(x,y,w,h);
        else if(!strcmp(type,"wrap")) {
            int lines=package_value(&j,item,"lines");
            int role=package_value(&j,item,"role");
            const Font *font=ui_font(role==4 ? UI_TITLE : role==1 ? UI_HEADING : UI_BODY);
            if(lines>0 && lines<=12)wrapped_raw(x,y,label,font,w,lines);
        }
        else if(!strcmp(type,"heading"))heading_actions(label,package_value(&j,item,"actions"));
        else if(!strcmp(type,"text")) {
            int role=package_value(&j,item,"role");
            static const TextRole roles[]={UI_BODY,UI_HEADING,UI_DISPLAY,UI_SECONDARY,UI_TITLE};
            TextStyle style=ui_text_style((unsigned)role<sizeof(roles)/sizeof(roles[0]) ? roles[role] : UI_BODY);
            double override=json_number(&j,json_get(&j,item,"ink"));
            int previous=ink_color, left=package_value(&j,item,"left");
            ink_color=override>=0 && override<=255 ? (int)override : style.color;
            for(int dx=0;dx<=!!style.weight;dx++) {
                if(left)label_ellipsis(x+dx,y,w-dx,h,label,style.font,0);
                else centered(x+dx,y,w-dx,h,label,style.font);
            }
            ink_color=previous;
        }
        else if(!strcmp(type,"mono")) {
            int ink=package_value(&j,item,"ink");
            if(ink<0 || ink>255)ink=0;
            int advance=package_value(&j,item,"advance");
            if(advance<7 || advance>12)advance=12;
            const Font *mono_font=h>=26 && advance==11 ? &small : &caption;
            int glyph_height=advance<=9 ? 22 : mono_font->h;
            int baseline=y+(h-glyph_height)/2;
            const char *start=label;
            for(int column=0;*start && column*advance+advance<=w;column++) {
                if(*start=='\t'){column+=3-column%4;start++;continue;}
                unsigned code=glyph(&start);
                const GlyphBounds *bounds=text_bounds(mono_font,code);
                const unsigned char *pixels=glyph_pixels(mono_font,code);
                int left=bounds ? bounds->left : 0, top=bounds ? bounds->top : 0;
                int width=bounds ? bounds->right-left : mono_font->w;
                int bottom=bounds ? bounds->bottom : mono_font->h;
                int span=width<advance-1 ? width : advance-1;
                int at=x+column*advance+(advance-span)/2;
                for(int row=top;row<bottom;row++) {
                    int yy=baseline+row*glyph_height/mono_font->h;
                    if(yy<STATUS_HEIGHT || yy>=HEIGHT)continue;
                    for(int px=0;px<span;px++) {
                        int xx=at+px;
                        if(xx<0 || xx>=WIDTH)continue;
                        int alpha=255-pixels[row*mono_font->w+left+px*width/span];
                        unsigned char *dst=ui_pixels+yy*WIDTH+xx;
                        *dst=(unsigned char)(*dst+(ink-*dst)*alpha/255);
                    }
                }
            }
        }
        else if(!strcmp(type,"button"))button_h(x,y,w,h,label,package_value(&j,item,"primary"));
        else if(!strcmp(type,"checkbox"))checkbox(x,y,package_value(&j,item,"value"));
        else if(!strcmp(type,"toggle"))toggle(x,y,package_value(&j,item,"value"));
        else if(!strcmp(type,"dropdown"))dropdown_button(x,y,w,h,label);
        else if(!strcmp(type,"select")){char value[256];json_string(&j,json_get(&j,item,"value"),value,sizeof(value));select_row17(y,label,value);}
        else if(!strcmp(type,"icon"))icon_button(x,y,package_value(&j,item,"icon"));
        else if(!strcmp(type,"hit"))region(x,y,w,h);
        else if(!strcmp(type,"surface"))ui_paint_surface(x,y,w,h,package_value(&j,item,"radius"),package_value(&j,item,"gray"),package_value(&j,item,"outline"));
        else if(!strcmp(type,"ellipse")){int rx=package_value(&j,item,"rx"),ry=package_value(&j,item,"ry");if(rx>=0&&rx<=300&&ry>=0&&ry<=400)ui_paint_ellipse(x,y,rx,ry,package_value(&j,item,"gray"));}
        else if(!strcmp(type,"hex_cell")) {
            int fill=package_value(&j,item,"gray");
            if(fill<0 || fill>255)continue;
            for(int dy=-18;dy<=18;dy++) {
                int w=32-abs(dy)*16/18;
                rect(x-w,y+dy,2*w+1,1,fill);
            }
            const int dx[]={-32,-16,16,32,16,-16},dy[]={0,-18,-18,0,18,18};
            for(int k=0;k<6;k++) {
                int n=(k+1)%6;
                stroke_color(x+dx[k],y+dy[k],x+dx[n],y+dy[n],2,0);
            }
        }
        else if(!strcmp(type,"line")){int x2=package_value(&j,item,"x2"),y2=package_value(&j,item,"y2"),weight=package_value(&j,item,"weight");if(y2>=56&&y2<=800&&x2>=-600&&x2<=1200&&weight>0&&weight<=32)ui_paint_line(x,y,x2,y2,weight,package_value(&j,item,"gray"));}
        else if(!strcmp(type,"ring")){int r=package_value(&j,item,"radius"),weight=package_value(&j,item,"weight");if(r>0&&r<=300&&weight>0&&weight<=24)ui_paint_ring(x,y,r,weight);}
    }

}
static void package_ui_draw(void) {
    package_pager.count=0;
    package_component_count=package_component_layout=0;
    if(!*app_process_error())app_process_open(app_for_route(ui_route()));
    static unsigned char status_bar[600*STATUS_HEIGHT];
    memcpy(status_bar,ui_pixels,sizeof(status_bar));
    const char*text=app_process_frame();
    if(!*text) {heading(app_definition(app_for_route(ui_route()))->name);centered(28,340,544,60,*app_process_error()?tr(app_process_error()):tr("App wird geöffnet …"),ui_font(UI_BODY));return;}
    package_paint_frame(text);
    memcpy(ui_pixels,status_bar,sizeof(status_bar));
}
static int package_ui_input(const Input*in) {
    if(in->kind!=INPUT_TAP && in->kind!=INPUT_SWIPE && in->kind!=INPUT_HOLD)return 2;
    if(in->kind==INPUT_TAP && in->x>=28 && in->x<572) {
        for(int i=0;i<package_component_count;i++) {
            PackageComponent *component=&package_components[i];
            if(in->y<component->top || in->y>=component->top+component->height)continue;
            if(component->field) {
                snprintf(package_focused_component,sizeof(package_focused_component),"%s",component->id);
                open_keyboard(2000,component->label,component->value);
                return 1;
            }
            package_component_event(component->id,NULL);return 2;
        }
    }
    if(package_pager.count>1) {
        int next=-1;
        if(in->kind==INPUT_SWIPE && abs(in->dx)>abs(in->dy))
            next=package_pager.page+(in->dx<0 ? 1 : -1);
        if(in->kind==INPUT_TAP && in->y>=package_pager.y && in->y<package_pager.y+44)
            next=pager_target(in->x,PAGER_X,PAGER_W,package_pager.page,package_pager.count);
        if(next>=0 && next<package_pager.count) {
            char event[64];snprintf(event,sizeof(event),"{\"event\":\"page\",\"page\":%d}",next);
            app_process_send(event);return 2;
        }
    }
    char event[256];snprintf(event,sizeof(event),"{\"event\":\"input\",\"kind\":%d,\"x\":%d,\"y\":%d,\"dx\":%d,\"dy\":%d}",in->kind,in->x,in->y,in->dx,in->dy);
    app_process_open(app_for_route(ui_route()));app_process_send(event);return 2;
}
static int package_ui_commit(int target,const char*text) {
    if(target!=2000)return 0;
    if(*package_focused_component) {
        package_component_event(package_focused_component,text);
        package_focused_component[0]=0;
        return 1;
    }
    char*event=NULL;size_t n=0;FILE*f=open_memstream(&event,&n);fprintf(f,"{\"event\":\"text\",\"action\":%d,\"text\":",package_input_action);
    json_escape16(f,text);fputc('}',f);fclose(f);app_process_send(event);free(event);return 1;
}
static int package_ui_poll(void) {
    int changed=app_process_poll();
    static uint64_t timer_frame;
    uint64_t serial=app_process_frame_serial();
    if(serial!=timer_frame && app_package_installed(app_for_route(ui_route()))) {
        timer_frame=serial;
        Json frame;
        if(json_parse(&frame,app_process_frame(),package_tokens,32768)) {
            int tick=package_value(&frame,0,"tick");
            package_deadline=tick>=250 && tick<=60000 ? extra_millis()+tick : 0;
            package_timer_owner=app_for_route(ui_route());
        }
    }
    if(changed && editor_ui.keyboard && editor_ui.keyboard_target==2000)editor_ui.keyboard_background_valid18=0;
    if(changed && *app_process_error())ui_message(app_process_error());
    const char*message=app_process_request();if(!*message)return changed;
    Json j;JsonToken t[512];if(!json_parse(&j,message,t,512))return changed;
    char kind[32],text[8192],title[128];json_string(&j,json_get(&j,0,"request"),kind,sizeof(kind));json_string(&j,json_get(&j,0,"text"),text,sizeof(text));json_string(&j,json_get(&j,0,"title"),title,sizeof(title));
    if(!strcmp(kind,"input")){package_focused_component[0]=0;package_input_action=package_value(&j,0,"action");open_keyboard(2000,title,text);}
    else if(!strcmp(kind,"select")) {
        int array=json_get(&j,0,"options");if(array>=0 && j.tokens[array].type=='[' && j.tokens[array].count>0 && j.tokens[array].count<=48) {
            int count=j.tokens[array].count;for(int i=0;i<count;i++){json_string(&j,json_at(&j,array,i),package_options[i],128);package_labels[i]=package_options[i];}
            package_select_action=package_value(&j,0,"action");open_dropdown(2001,package_labels,count,package_value(&j,0,"selected"),220,180,350);
        }
    }
    else if(!strcmp(kind,"service")) {
        char service[64],operation[64];
        json_string(&j,json_get(&j,0,"service"),service,sizeof(service));
        json_string(&j,json_get(&j,0,"operation"),operation,sizeof(operation));
        package_service_request(service,operation,text,package_value(&j,0,"action"));
    }
    else if(!strcmp(kind,"notify"))ui_message(text);
    else if(!strcmp(kind,"reader"))reader_markdown17(title,text,ui_route());
    else if(!strcmp(kind,"intent")) {
        char app[64],operation[48],object[32];
        json_string(&j,json_get(&j,0,"app"),app,sizeof(app));
        json_string(&j,json_get(&j,0,"operation"),operation,sizeof(operation));
        json_string(&j,json_get(&j,0,"object"),object,sizeof(object));
        int id=app_package_find(app);
        if(id>=0 && !strcmp(operation,"open")) {
            AppIntent intent={.app=id,.kind=INTENT_OPEN};
            app_send(&intent);
        } else if(id==APP_READER && !strcmp(operation,"open_book") &&
                  strlen(object)==16 && strspn(object,"0123456789abcdef")==16) {
            AppIntent intent={.app=APP_READER,.kind=INTENT_BOOK,.object_id=strtoull(object,NULL,16)};
            app_send(&intent);
        } else if(id==APP_READER && !strcmp(operation,"open_document") && *text)
            reader_markdown17(title,text,ui_route());
    }
    else if(!strcmp(kind,"open")){char key[96];json_string(&j,json_get(&j,0,"app"),key,sizeof(key));int id=-1;for(int i=0;i<APP_LIMIT;i++)if(!strcmp(key,app_package_key(i))){id=i;break;}if(id>=0){AppIntent intent={.app=id,.kind=*text?INTENT_SEARCH:INTENT_OPEN};snprintf(intent.text,sizeof(intent.text),"%s",text);app_send(&intent);}}
    return 1;
}
