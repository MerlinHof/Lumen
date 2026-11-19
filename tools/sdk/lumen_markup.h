#ifndef LUMEN_MARKUP_H
#define LUMEN_MARKUP_H
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
/* Reads the generic document helper's preorder tree in the package process. */
enum { MARKUP_ATTRIBUTES=12, MARKUP_DEPTH=100, MARKUP_FIELD_LIMIT=4<<20 };

typedef struct MarkupNode {
    struct MarkupNode *child,*next,*parent;
    char *tag,*text,*attribute[MARKUP_ATTRIBUTES];
} MarkupNode;
static void markup_free(MarkupNode *node) {
    while(node) {
        MarkupNode *next=node->next;
        markup_free(node->child);
        free(node->tag);free(node->text);
        for(int i=0;i<MARKUP_ATTRIBUTES;i++)free(node->attribute[i]);
        free(node);
        node=next;
    }
}
static int markup_string(FILE *input,char **value,size_t *budget) {
    uint32_t size;
    if(fread(&size,sizeof(size),1,input)!=1 || size>*budget || size>MARKUP_FIELD_LIMIT)return 0;
    if(!size)return 1;
    *budget-=size;
    *value=malloc((size_t)size+1);
    if(!*value || fread(*value,1,size,input)!=size)return 0;
    (*value)[size]=0;
    return 1;
}
static MarkupNode *markup_read(FILE *input,int count) {
    MarkupNode *root=NULL,**links[MARKUP_DEPTH+1]={&root};
    MarkupNode *parents[MARKUP_DEPTH+1]={0};
    uint32_t depth,previous=0;
    size_t budget=16u<<20;
    while(fread(&depth,1,sizeof(depth),input)==sizeof(depth)) {
        if(depth>MARKUP_DEPTH || depth>previous+1 || !links[depth] || budget<sizeof(MarkupNode))goto fail;
        MarkupNode *node=calloc(1,sizeof(*node));
        if(!node)goto fail;
        budget-=sizeof(*node);
        node->parent=depth ? parents[depth-1] : NULL;
        parents[depth]=node;
        *links[depth]=node;
        links[depth]=&node->next;
        if(depth<MARKUP_DEPTH)links[depth+1]=&node->child;
        previous=depth;
        if(!markup_string(input,&node->tag,&budget) || !markup_string(input,&node->text,&budget))goto fail;
        for(int i=0;i<count;i++)
            if(!markup_string(input,&node->attribute[i],&budget))goto fail;
    }
    if(!ferror(input))return root;
fail:
    markup_free(root);
    return NULL;
}
static int wait_success(pid_t child) {
    if(child<0)return 0;
    int status;pid_t done;
    do done=waitpid(child,&status,0);while(done<0 && errno==EINTR);
    return done==child && WIFEXITED(status) && WEXITSTATUS(status)==0;
}
static MarkupNode *markup_load(const char *path,int html,const char *const *names,int count) {
    const char *tool=getenv("LUMEN_DOCUMENT_TOOL");
    if(!tool || !*tool || count<0 || count>MARKUP_ATTRIBUTES)return NULL;
    int source=open(path,O_RDONLY);
    if(source<0)return NULL;
    int output[2];
    if(pipe(output)){close(source);return NULL;}
    pid_t child=fork();
    if(!child) {
        if(dup2(source,STDIN_FILENO)<0 || dup2(output[1],STDOUT_FILENO)<0)_exit(1);
        close(source);close(output[0]);close(output[1]);
        char *args[MARKUP_ATTRIBUTES+4]={(char *)tool,"--markup-tree",html ? "html" : "xml"};
        for(int i=0;i<count;i++)args[3+i]=(char *)names[i];
        args[3+count]=NULL;
        execv(tool,args);
        _exit(1);
    }
    close(source);close(output[1]);
    FILE *stream=fdopen(output[0],"rb");
    MarkupNode *root=child>0 && stream ? markup_read(stream,count) : NULL;
    if(stream)fclose(stream);else close(output[0]);
    if(!wait_success(child)){markup_free(root);root=NULL;}
    return root;
}
static const char *markup_attribute(const MarkupNode *node,const char *name,const char *const *names,int count) {
    for(int i=0;i<count;i++)if(!strcmp(names[i],name))return node->attribute[i];
    return NULL;
}
#endif
