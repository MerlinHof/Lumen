/* Resolve responsive column groups at the actual browser viewport. */
enum { WEB_VIEW_WIDTH=544, WEB_VIEW_HEIGHT=560, WEB_LAYOUT_RULES=128 };
typedef struct {char selector[96];int columns,gap,specificity;} WebLayoutRule;
typedef struct {WebLayoutRule rules[WEB_LAYOUT_RULES];int count;} WebLayout;

static int simple_match(const MarkupNode *node,const char *p,const char *end) {
    if(!node || !node->tag || p==end)return 0;
    while(p<end) {
        char kind=*p=='.' || *p=='#' ? *p++ : 0;
        const char *part=p;
        while(p<end && (isalnum((unsigned char)*p) || *p=='-' || *p=='_' || *p=='*'))p++;
        size_t length=(size_t)(p-part);
        if(!length)return 0;
        if(kind=='.') {
            const char *value=node->attribute[2];
            int found=0;
            while(value && *value) {
                value+=strspn(value," \n\r\t");
                size_t n=strcspn(value," \n\r\t");
                if(n==length && !memcmp(value,part,n)){found=1;break;}
                value+=n;
            }
            if(!found)return 0;
        } else if(length!=1 || *part!='*') {
            const char *value=kind=='#' ? node->attribute[3] : node->tag;
            if(!value || strlen(value)!=length || strncmp(value,part,length))return 0;
        }
    }
    return 1;
}
static int selector_match(const MarkupNode *node,const char *start,const char *end,int *budget) {
    if(*budget<=0)return 0;
    --*budget;
    while(end>start && isspace((unsigned char)end[-1]))end--;
    const char *p=end;
    while(p>start && !isspace((unsigned char)p[-1]) && p[-1]!='>')p--;
    if(!simple_match(node,p,end))return 0;
    while(p>start && isspace((unsigned char)p[-1]))p--;
    if(p==start)return 1;
    if(p[-1]=='>')return selector_match(node->parent,start,p-1,budget);
    for(node=node->parent;node && *budget>0;node=node->parent)
        if(selector_match(node,start,p,budget))return 1;
    return 0;
}
static int complex_match(const MarkupNode *node,const char *start,const char *end) {
    int budget=96;
    return selector_match(node,start,end,&budget);
}
static int media_matches(const char *start,const char *end) {
    if(end-start>512)return 0;
    const char *comma=start;
    while(comma<end && *comma!=',')comma++;
    const char *p=start;
    const char *first=p;
    while(first<comma && isspace((unsigned char)*first))first++;
    int matched=first<comma && strncmp(first,"print",5) && strncmp(first,"not ",4);
    for(;matched && p<comma;p++) {
        if(*p!='(')continue;
        p++;
        char feature[24],value[32];
        int used=0;
        if(sscanf(p," %23[a-z-] : %31[a-z0-9.] %n",feature,value,&used)!=2 ||
           !used || p+used>=comma || p[used]!=')'){matched=0;break;}
        if(!strcmp(feature,"orientation"))matched=!strcmp(value,"portrait");
        else {
            int width=strstr(feature,"width")!=NULL;
            int height=strstr(feature,"height")!=NULL;
            char *unit;
            double n=strtod(value,&unit);
            if(!strcmp(unit,"em") || !strcmp(unit,"rem"))n*=16;
            else if(strcmp(unit,"px") && *unit)matched=0;
            if(width || height) {
                int viewport=width ? WEB_VIEW_WIDTH : WEB_VIEW_HEIGHT;
                if(!strncmp(feature,"min-",4))matched=matched && viewport>=n;
                else if(!strncmp(feature,"max-",4))matched=matched && viewport<=n;
                else matched=matched && viewport==n;
            } else matched=0;
        }
        p+=used;
    }
    return matched || (comma<end && media_matches(comma+1,end));
}
static const char *css_property(const char *p,const char *end,const char *name) {
    size_t n=strlen(name);
    while(p<end) {
        while(p<end && (isspace((unsigned char)*p) || *p==';'))p++;
        if((size_t)(end-p)>n && !strncasecmp(name,p,n)) {
            const char *value=p+n;
            while(value<end && isspace((unsigned char)*value))value++;
            if(value<end && *value==':') {
                value++;
                while(value<end && isspace((unsigned char)*value))value++;
                if(value<end)return value;
            }
        }
        while(p<end && *p!=';')p++;
    }
    return NULL;
}
static int layout_value(const char *value,const char *end,const char *expected) {
    size_t n=strlen(expected);
    return value && (size_t)(end-value)>=n && !strncasecmp(value,expected,n) &&
        (value+n==end || value[n]==';' || isspace((unsigned char)value[n]));
}
static void layout_record(WebLayout *layout,const char *selector,const char *open,const char *end) {
    const char *display=css_property(open+1,end,"display");
    if(!display)return;
    int columns=0,gap=0;
    if(layout_value(display,end,"flex") || layout_value(display,end,"inline-flex")) {
        const char *direction=css_property(open+1,end,"flex-direction");
        if(!direction)direction=css_property(open+1,end,"flex-flow");
        if(!layout_value(direction,end,"column") && !layout_value(direction,end,"column-reverse"))columns=-1;
    } else if(layout_value(display,end,"grid") || layout_value(display,end,"inline-grid")) {
        const char *value=css_property(open+1,end,"grid-template-columns");
        if(value && sscanf(value,"repeat( %d",&columns)!=1) {
            columns=0;
            int word=0;
            for(;value<end && *value!=';';value++) {
                if(isspace((unsigned char)*value))word=0;
                else if(!word){columns++;word=1;}
            }
        }
        if(columns<2 || columns>4)columns=2;
    }
    const char *spacing=css_property(open+1,end,"gap");
    if(spacing){gap=atoi(spacing);if(gap<0)gap=0;if(gap>36)gap=36;}
    while(selector<open && layout->count<WEB_LAYOUT_RULES) {
        while(selector<open && isspace((unsigned char)*selector))selector++;
        const char *comma=selector;
        while(comma<open && *comma!=',')comma++;
        const char *stop=comma;
        while(stop>selector && isspace((unsigned char)stop[-1]))stop--;
        if(stop>selector && stop-selector<(int)sizeof(layout->rules[0].selector) &&
           css_chars(selector,stop,"#.*_->")) {
            WebLayoutRule *rule=&layout->rules[layout->count++];
            snprintf(rule->selector,sizeof(rule->selector),"%.*s",(int)(stop-selector),selector);
            rule->columns=columns;rule->gap=gap;
            rule->specificity=0;
            int previous=0;
            for(const char *p=selector;p<stop;p++) {
                if(*p=='#')rule->specificity+=100;
                else if(*p=='.')rule->specificity+=10;
                else if(!previous && isalpha((unsigned char)*p))rule->specificity++;
                previous=!isspace((unsigned char)*p) && *p!='>';
            }
        }
        selector=comma<open ? comma+1 : open;
    }
}
static const WebLayoutRule *layout_for(const WebLayout *layout,const MarkupNode *node) {
    const WebLayoutRule *best=NULL;
    for(int i=layout->count-1;i>=0;i--) {
        const WebLayoutRule *rule=&layout->rules[i];
        if(best && best->specificity>=rule->specificity)continue;
        if(complex_match(node,rule->selector,rule->selector+strlen(rule->selector)))best=rule;
    }
    return best;
}
