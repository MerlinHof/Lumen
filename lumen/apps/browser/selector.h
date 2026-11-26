/* Bounded selector matching for the JavaScript DOM bridge. */
enum { WEB_VIEW_WIDTH=584, WEB_VIEW_HEIGHT=686 };

static int selector_attribute(const MarkupNode *node,const char *at,const char *end) {
    const char *name=at;
    while(at<end && (isalnum((unsigned char)*at) || *at=='-' || *at=='_'))at++;
    size_t length=(size_t)(at-name);
    if(!length)return 0;
    const char *value=NULL;
    for(size_t i=0;i<WEB_ATTRIBUTES;i++)if(strlen(attributes[i])==length && !memcmp(attributes[i],name,length)) {
        value=node->attribute[i];break;
    }
    if(!value)return 0;
    while(at<end && isspace((unsigned char)*at))at++;
    if(at==end)return 1;
    char op=*at=='=' ? 0 : *at++;
    if(at==end || *at++!='=')return 0;
    while(at<end && isspace((unsigned char)*at))at++;
    while(end>at && isspace((unsigned char)end[-1]))end--;
    if(end-at>=2 && (*at=='"' || *at=='\'') && end[-1]==*at){at++;end--;}
    size_t n=(size_t)(end-at),size=strlen(value);
    if(n>size)return 0;
    if(!op)return n==size && !memcmp(value,at,n);
    if(op=='^')return !memcmp(value,at,n);
    if(op=='$')return !memcmp(value+size-n,at,n);
    if(op=='*') {
        for(size_t i=0;i+n<=size;i++)if(!memcmp(value+i,at,n))return 1;
    }
    return 0;
}
static int selector_token(const char *p,size_t n,const char *token) {
    return strlen(token)==n && !strncmp(p,token,n);
}
static int complex_match(const MarkupNode *node,const char *start,const char *end);
static const MarkupNode *element_before(const MarkupNode *node) {
    const MarkupNode *previous=NULL;
    for(const MarkupNode *it=node->parent ? node->parent->child : NULL;it && it!=node;it=it->next)
        if(it->tag)previous=it;
    return previous;
}
static int simple_match(const MarkupNode *node,const char *p,const char *end) {
    if(!node || !node->tag || p==end)return 0;
    while(p<end) {
        if(*p=='[') {
            p++;
            const char *right=memchr(p,']',(size_t)(end-p));
            if(!right || !selector_attribute(node,p,right))return 0;
            p=right+1;continue;
        }
        if(*p==':') {
            const char *name=++p;
            while(p<end && (isalnum((unsigned char)*p) || *p=='-'))p++;
            size_t n=(size_t)(p-name);
            if(selector_token(name,n,"checked")) {
                if(!node->attribute[ATTR_CHECKED] && !node->attribute[ATTR_SELECTED])return 0;
            } else if(selector_token(name,n,"disabled") || selector_token(name,n,"enabled")) {
                if((node->attribute[ATTR_DISABLED]!=NULL)!=selector_token(name,n,"disabled"))return 0;
            } else if(selector_token(name,n,"first-child")) {
                if(!node->parent || element_before(node))return 0;
            } else if(selector_token(name,n,"last-child")) {
                if(!node->parent)return 0;
                for(const MarkupNode *it=node->next;it;it=it->next)if(it->tag)return 0;
            } else if(selector_token(name,n,"empty")) {
                if(node->child)return 0;
            } else if(selector_token(name,n,"root")) {
                if(node->parent)return 0;
            } else if(p<end && *p=='(' && (selector_token(name,n,"is") ||
                       selector_token(name,n,"where") || selector_token(name,n,"not"))) {
                const char *start=++p;
                while(p<end && *p!=')' && *p!='(')p++;
                if(p==end || *p!=')')return 0;
                int found=complex_match(node,start,p++);
                if(found==selector_token(name,n,"not"))return 0;
            } else return 0;
            continue;
        }
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
    if(!node || *budget<=0)return 0;
    --*budget;
    while(end>start && isspace((unsigned char)end[-1]))end--;
    const char *p=end;
    int nested=0;
    while(p>start) {
        char c=p[-1];
        if(c==']' || c==')')nested++;
        else if((c=='[' || c=='(') && nested)nested--;
        else if(!nested && (isspace((unsigned char)c) || c=='>' || c=='+' || c=='~'))break;
        p--;
    }
    if(!simple_match(node,p,end))return 0;
    while(p>start && isspace((unsigned char)p[-1]))p--;
    if(p==start)return 1;
    if(p[-1]=='>')return selector_match(node->parent,start,p-1,budget);
    if(p[-1]=='+')return selector_match(element_before(node),start,p-1,budget);
    if(p[-1]=='~') {
        for(const MarkupNode *it=element_before(node);it && *budget>0;it=element_before(it))
            if(selector_match(it,start,p-1,budget))return 1;
        return 0;
    }
    for(node=node->parent;node && *budget>0;node=node->parent)
        if(selector_match(node,start,p,budget))return 1;
    return 0;
}
static int complex_match(const MarkupNode *node,const char *start,const char *end) {
    const char *group=start;
    int brackets=0,budget=96;
    char quote=0;
    for(const char *p=start;p<=end;p++) {
        if(p==end || (*p==',' && !brackets && !quote)) {
            while(group<p && isspace((unsigned char)*group))group++;
            if(group<p && selector_match(node,group,p,&budget))return 1;
            group=p+1;
        } else if(quote) {
            if(*p==quote)quote=0;
        } else if(*p=='[' || *p=='(')brackets++;
        else if(*p==']' || *p==')')brackets--;
        else if(*p==34 || *p==39)quote=*p;
    }
    return 0;
}
