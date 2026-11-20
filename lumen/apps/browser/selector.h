/* Bounded selector matching for the JavaScript DOM bridge. */
enum { WEB_VIEW_WIDTH=584, WEB_VIEW_HEIGHT=686 };

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
