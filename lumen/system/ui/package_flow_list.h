/* Measured lists with stable row IDs and optional checkbox, pin and trash
 * affordances. The app supplies order and grouping; the shell owns geometry. */
static void package_flow_action(const char *id,int left,int top,int width,int height,int action) {
    if(package_component_count>=PACKAGE_COMPONENT_LIMIT)return;
    PackageComponent *c=&package_components[package_component_count++];
    *c=(PackageComponent){.left=left,.top=top,.width=width,.height=height,.kind=COMPONENT_FLOW};
    snprintf(c->id,sizeof(c->id),"%s",id);
    c->checked=action;
}
static void package_flow_list(Json *j,int item) {
    int rows=json_get(j,item,"rows");
    if(rows<0 || j->tokens[rows].type!='[' || j->tokens[rows].count>512)return;
    int columns=package_value(j,item,"columns"),selected=package_value(j,item,"page");
    if(columns<1 || columns>2 || selected<0 || selected>512)return;
    int first=columns==2 ? 195 : 137;
    int width=columns==2 ? 262 : 544,text_width=columns==2 ? 148 : 365;
    int height[512],row_page[512],row_top[512],line_count[512];
    int offset[]={first,first},pages[]={0,0},count=j->tokens[rows].count;
    for(int i=0,r=rows+1;i<count;i++,r=j->tokens[r].next) {
        int col=package_value(j,r,"column");
        height[i]=0;
        if(col<0 || col>=columns)continue;
        char title[512];json_string(j,json_get(j,r,"title"),title,sizeof(title));
        int details=json_get(j,r,"detail");
        int available=text_width-(details>=0 ? 46 : 0);
        FlowLine lines[14];int n=text_flow(title,ui_font(UI_BODY),available,lines,14);
        int h=n*27+22+(details>=0 && j->tokens[details].end-j->tokens[details].start>2 ? 21 : 0);
        if(h<68)h=68;
        if(offset[col]+h>720 && offset[col]>first){offset[col]=first;pages[col]++;}
        height[i]=h;row_top[i]=offset[col];row_page[i]=pages[col];line_count[i]=n;
        offset[col]+=h+4;
    }
    int total=1+(pages[0]>pages[1] ? pages[0] : pages[1]);
    if(selected>=total)selected=total-1;
    for(int i=0,r=rows+1;i<count;i++,r=j->tokens[r].next) {
        if(!height[i] || row_page[i]!=selected)continue;
        int col=package_value(j,r,"column"),x=28+col*282;
        int y=row_top[i],center=y+height[i]/2;
        char id[64],title[512];json_string(j,json_get(j,r,"id"),id,sizeof(id));
        json_string(j,json_get(j,r,"title"),title,sizeof(title));
        if(!*id)continue;
        int checked=package_value(j,r,"checked"),pinned=package_value(j,r,"pinned");
        int has_action=json_get(j,r,"detail")>=0,warning=package_value(j,r,"warning");
        char detail[96];json_string(j,json_get(j,r,"detail"),detail,sizeof(detail));
        if(columns==2)key_outline(x,y,width,height[i]);
        else divider(x,y+height[i],width);
        region(x,y,width,height[i]);
        int left=x+(columns==2 ? 8 : 1),flag=x+(columns==2 ? 212 : 430);
        int action_x=flag-48,delete_x=flag+48,text_x=x+(columns==2 ? 60 : 52);
        int available=text_width-(has_action ? 46 : 0);
        int text_top=center-(line_count[i]*27+(*detail ? 21 : 0))/2;
        checkbox(left,center-15,checked);
        icon_button(flag,center-22,pinned ? IC_FLAG_FILLED : IC_FLAG);
        if(columns==1)icon_button(delete_x,center-22,IC_TRASH);
        if(has_action)icon_button(action_x,center-22,IC_CLOCK);
        text_flow_draw(text_x,text_top,available,title,ui_font(UI_BODY),27,14,checked);
        if(*detail) {
            int detail_y=text_top+line_count[i]*27-2;
            if(warning) {
                outline_surface(text_x,detail_y+4,16,16,8,0);
                centered(text_x,detail_y+1,16,21,"!",&caption);
            }
            int shift=warning ? 21 : 0;
            label_ellipsis(text_x+shift,detail_y,available-shift,25,detail,&caption,0);
        }
        package_flow_action(id,left,center-24,44,48,1);
        package_flow_action(id,flag,center-22,44,44,2);
        if(columns==1)package_flow_action(id,delete_x,center-22,44,44,3);
        if(has_action)package_flow_action(id,action_x,center-22,44,44,4);
        package_flow_action(id,x,y,width,height[i],0);
    }
    if(total>1) {
        package_pager.y=746;package_pager.page=selected;package_pager.count=total;
        pager_draw(PAGER_X,746,PAGER_W,selected,total);
    }
}
