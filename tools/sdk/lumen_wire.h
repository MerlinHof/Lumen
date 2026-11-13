#ifndef LUMEN_WIRE_H
#define LUMEN_WIRE_H
#include <errno.h>
#include <limits.h>

/* Protocol 1 background operation payload, measured in decoded UTF-8 bytes. */
enum { LUMEN_SERVICE_DATA_MAX = 8191 };

static int wire_space(unsigned char c){return c==' ' || c=='\t' || c=='\r' || c=='\n';}
static const char *wire_skip_space(const char *p){while(wire_space(*p))p++;return p;}
static int wire_digit(unsigned char c){return c>='0' && c<='9';}
static int wire_hex(const char **at,unsigned *value) {
    const char *p=*at;*value=0;
    for(int i=0;i<4;i++) {
        unsigned char c=*p;
        int digit=wire_digit(c) ? c-'0' : c>='a' && c<='f' ? c-'a'+10 :
                  c>='A' && c<='F' ? c-'A'+10 : -1;
        if(digit<0)return 0;
        *value=(*value<<4)|(unsigned)digit;p++;
    }
    *at=p;return 1;
}
static const char *wire_decode_string(const char *p,char *out,size_t cap,int *truncated) {
    if(*p++!='"')return NULL;
    size_t used=0;int clipped=0;
    while(*p && *p!='"') {
        unsigned char c=*p++;
        if(c<32)return NULL;
        unsigned char bytes[4]={c};unsigned count=1;
        if(c=='\\') {
            c=*p;
            if(!c)return NULL;
            p++;
            const char *escapes="\"\\/bfnrt";
            const char *position=strchr(escapes,c);
            if(position)bytes[0]=(unsigned char)"\"\\/\b\f\n\r\t"[position-escapes];
            else if(c=='u') {
                unsigned cp;
                if(!wire_hex(&p,&cp))return NULL;
                if(cp>=0xd800 && cp<=0xdbff) {
                    if(p[0]!='\\' || p[1]!='u')return NULL;
                    p+=2;unsigned low;
                    if(!wire_hex(&p,&low) || low<0xdc00 || low>0xdfff)return NULL;
                    cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);
                } else if(cp>=0xdc00 && cp<=0xdfff)return NULL;
                if(!cp)cp=0xfffd;
                if(cp<128)bytes[0]=cp;
                else if(cp<2048){bytes[0]=0xc0|(cp>>6);bytes[1]=0x80|(cp&63);count=2;}
                else if(cp<65536){bytes[0]=0xe0|(cp>>12);bytes[1]=0x80|((cp>>6)&63);bytes[2]=0x80|(cp&63);count=3;}
                else {bytes[0]=0xf0|(cp>>18);bytes[1]=0x80|((cp>>12)&63);bytes[2]=0x80|((cp>>6)&63);bytes[3]=0x80|(cp&63);count=4;}
            } else return NULL;
        } else if(c>=0x80) {
            count=c>=0xc2 && c<=0xdf ? 2 : c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
            if(!count)return NULL;
            for(unsigned i=1;i<count;i++) {
                if(((unsigned char)*p&0xc0)!=0x80)return NULL;
                bytes[i]=(unsigned char)*p++;
            }
            if((c==0xe0 && bytes[1]<0xa0)||(c==0xed && bytes[1]>=0xa0)||
               (c==0xf0 && bytes[1]<0x90)||(c==0xf4 && bytes[1]>=0x90))return NULL;
        }
        if(out && !clipped && used+count<cap){memcpy(out+used,bytes,count);used+=count;}
        else clipped=1;
    }
    if(out && cap)out[used]=0;
    if(truncated)*truncated=clipped;
    return *p=='"' ? p+1 : NULL;
}
static const char *wire_string(const char *p,char *out,size_t cap) {
    return wire_decode_string(p,out,cap,NULL);
}
static const char *wire_number(const char *p) {
    if(*p=='-')p++;
    if(*p=='0')p++;
    else {
        if(!wire_digit(*p))return NULL;
        while(wire_digit(*p))p++;
    }
    if(*p=='.') {
        p++;if(!wire_digit(*p))return NULL;
        while(wire_digit(*p))p++;
    }
    if(*p=='E' || *p=='e') {
        p++;if(*p=='+' || *p=='-')p++;
        if(!wire_digit(*p))return NULL;
        while(wire_digit(*p))p++;
    }
    return p;
}
static const char *wire_value(const char *p,unsigned depth) {
    p=wire_skip_space(p);
    if(depth>32 || !*p)return NULL;
    if(*p=='"')return wire_string(p,NULL,0);
    if(*p=='{' || *p=='[') {
        int object=*p=='{',close=object ? '}' : ']';
        p=wire_skip_space(p+1);
        if(*p==close)return p+1;
        for(;;) {
            if(object) {
                p=wire_string(p,NULL,0);
                if(!p || *(p=wire_skip_space(p))!=':')return NULL;
                p++;
            }
            p=wire_value(p,depth+1);
            if(!p)return NULL;
            p=wire_skip_space(p);
            if(*p==close)return p+1;
            if(*p!=',')return NULL;
            p=wire_skip_space(p+1);
        }
    }
    if(!strncmp(p,"true",4) || !strncmp(p,"null",4))return p+4;
    if(!strncmp(p,"false",5))return p+5;
    return wire_number(p);
}
typedef struct {char key[48];const char *value,*end;} WireField;
typedef struct {WireField fields[160];unsigned count;} WireObject;
static WireObject wire_event;
static int wire_read_object(WireObject *object,const char *p) {
    object->count=0;
    p=wire_skip_space(p);
    if(*p++!='{')return 0;
    p=wire_skip_space(p);
    if(*p=='}')return !*wire_skip_space(p+1);
    for(;;) {
        if(object->count>=sizeof(object->fields)/sizeof(*object->fields))return 0;
        WireField *field=&object->fields[object->count++];
        const char *key_start=p;
        p=wire_string(p,field->key,sizeof(field->key));
        if(!p)return 0;
        if((size_t)(p-key_start)>sizeof(field->key)+1)field->key[0]=0;
        if(*(p=wire_skip_space(p))!=':')return 0;
        field->value=wire_skip_space(p+1);
        field->end=wire_value(field->value,0);
        if(!field->end)return 0;
        p=wire_skip_space(field->end);
        if(*p=='}')return !*wire_skip_space(p+1);
        if(*p!=',')return 0;
        p=wire_skip_space(p+1);
    }
}
static inline const WireField *wire_object_get(const WireObject *object,const char *key) {
    for(unsigned i=0;i<object->count;i++)
        if(!strcmp(object->fields[i].key,key))return &object->fields[i];
    return NULL;
}
static inline int wire_read(const char *p) {return wire_read_object(&wire_event,p);}
static inline const WireField *wire_get(const char *key) {return wire_object_get(&wire_event,key);}
static inline int wire_field_string(const WireField *field,char *out,size_t cap) {
    if(!cap)return 0;
    out[0]=0;
    return field && *field->value=='"' && wire_string(field->value,out,cap)==field->end;
}
static inline int wire_bounded_string(const char *key,char *out,size_t cap) {
    const WireField *field=wire_get(key);
    int truncated=0;
    return field && *field->value=='"' &&
        wire_decode_string(field->value,out,cap,&truncated)==field->end && !truncated;
}
static inline int lumen_number(const char *key) {
    const WireField *field=wire_get(key);
    if(!field)return 0;
    if(!strncmp(field->value,"true",4))return 1;
    if(*field->value!='-' && !wire_digit(*field->value))return 0;
    errno=0;char *end;long value=strtol(field->value,&end,10);
    return errno || end!=field->end || value<INT_MIN || value>INT_MAX ? 0 : (int)value;
}
static inline int lumen_string(const char *key,char *out,size_t cap) {
    return wire_field_string(wire_get(key),out,cap);
}
#endif
