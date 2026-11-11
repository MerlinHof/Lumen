#include "lumen_app.h"
#include <dirent.h>

enum { MAX_DAYS=2048, PAGE_SIZE=7 };
typedef struct {int version,date,score;char answers[6][8192];} Entry;
static Entry entry;
static int dates[MAX_DAYS],count,page,section,today;
static const char *labels[7][3]={
    {"Gedanken abladen","Clear your mind","Vider son esprit"},
    {"Was war heute schön?","What went well today?","Qu’est-ce qui s’est bien passé ?"},
    {"Was war schwierig?","What was difficult?","Qu’est-ce qui a été difficile ?"},
    {"Was hast du gelernt?","What did you learn?","Qu’avez-vous appris ?"},
    {"Was möchtest du morgen anders machen?","What would you do differently tomorrow?","Que ferez-vous autrement demain ?"},
    {"Wofür bist du dankbar?","What are you grateful for?","De quoi êtes-vous reconnaissant ?"},
    {"Wie geht es dir heute?","How do you feel today?","Comment allez-vous aujourd’hui ?"}
};
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static const char *prompt(int index) {
    return labels[index][lumen_language==1 ? 1 : lumen_language==2 ? 2 : 0];
}
static void filename(char out[24],int date){snprintf(out,24,"%08d.bin",date);}
static void date_label(char out[40],int date){snprintf(out,40,"%02d.%02d.%04d",date%100,date/100%100,date/10000);}
static int save(void) {
    char name[24];filename(name,entry.date);
    return app_state_save(name,&entry,sizeof(entry));
}
static int valid_date(int date) {
    return date>=19000101 && date<=99991231 && date/100%100>=1 && date/100%100<=12 &&
        date%100>=1 && date%100<=31;
}
static void add_date(int date) {
    if(!valid_date(date) || count>=MAX_DAYS)return;
    for(int i=0;i<count;i++)if(dates[i]==date)return;
    dates[count++]=date;
}
static int newest(const void *a,const void *b){return *(const int *)b-*(const int *)a;}
static void scan(const char *directory) {
    if(!directory)return;
    DIR *dir=opendir(directory);
    if(!dir)return;
    struct dirent *item;
    while((item=readdir(dir))) {
        int date,n=0;
        if(sscanf(item->d_name,"%8d.bin%n",&date,&n)==1 && n==12 && !item->d_name[n])add_date(date);
    }
    closedir(dir);
}
static void legacy_notes(int wanted) {
    const char *root=getenv("LUMEN_LEGACY_DATA");
    char folder[2048];
    if(!root || snprintf(folder,sizeof(folder),"%s/notes",root)>=(int)sizeof(folder))return;
    DIR *dir=opendir(folder);
    if(!dir)return;
    struct dirent *item;
    while((item=readdir(dir))) {
        int id,end=0;
        if(sscanf(item->d_name,"%d.txt%n",&id,&end)!=1 || !end || item->d_name[end] || id<0)continue;
        char path[2112];
        snprintf(path,sizeof(path),"%s/%d.txt",folder,id);
        FILE *file=fopen(path,"rb");
        if(!file)continue;
        char heading[160];
        int day,month,year;
        if(!fgets(heading,sizeof(heading),file) ||
           sscanf(heading,"# Journal · %d.%d.%d",&day,&month,&year)!=3 ||
           year<1900 || year>9999 || month<1 || month>12 || day<1 || day>31){fclose(file);continue;}
        int date=year*10000+month*100+day;
        if(!wanted){add_date(date);fclose(file);continue;}
        if(date!=wanted){fclose(file);continue;}
        static char text[65537];
        text[0]='\n';
        size_t bytes=fread(text+1,1,sizeof(text)-2,file);
        fclose(file);text[bytes+1]=0;
        char *cursor=text;
        for(int i=0;i<6;i++) {
            char *head=strstr(cursor,"\n## ");
            char *begin=head ? strchr(head+4,'\n') : NULL;
            if(!begin)break;
            while(*begin=='\n')begin++;
            char *stop=strstr(begin,"\n## ");
            if(!stop)stop=strstr(begin,"\nStimmung:");
            if(!stop)stop=begin+strlen(begin);
            cursor=stop;
            while(stop>begin && stop[-1]=='\n')stop--;
            size_t length=(size_t)(stop-begin);
            if(length>=sizeof(entry.answers[i]))length=sizeof(entry.answers[i])-1;
            memcpy(entry.answers[i],begin,length);entry.answers[i][length]=0;
        }
        const char *mood=strstr(text,"Stimmung:");
        if(mood)entry.score=atoi(mood+9);
        break;
    }
    closedir(dir);
}
static void refresh(void) {
    time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
    today=(t.tm_year+1900)*10000+(t.tm_mon+1)*100+t.tm_mday;
    count=0;add_date(today);
    scan(getenv("LUMEN_APP_DATA"));
    const char *root=getenv("LUMEN_LEGACY_DATA");
    char legacy[2048];
    if(root && snprintf(legacy,sizeof(legacy),"%s/journals17",root)<(int)sizeof(legacy))scan(legacy);
    legacy_notes(0);
    qsort(dates,count,sizeof(*dates),newest);
    if(page*PAGE_SIZE>=count)page=0;
}
static void open_day(int date) {
    char name[24];filename(name,date);
    if(!app_state_load(name,&entry,sizeof(entry)) || entry.version!=1 || entry.date!=date) {
        memset(&entry,0,sizeof(entry));
        const char *root=getenv("LUMEN_LEGACY_DATA");
        char path[2048];
        int loaded=0;
        if(root && snprintf(path,sizeof(path),"%s/journals17/%s",root,name)<(int)sizeof(path)) {
            FILE *file=fopen(path,"rb");
            if(file) {
                uint32_t header[2];
                loaded=fread(header,8,1,file)==1 && header[0]==0x4c554d16 && header[1]==sizeof(entry) &&
                    fread(&entry,sizeof(entry),1,file)==1 && entry.version==1 && entry.date==date;
                fclose(file);
            }
        }
        if(!loaded) {
            memset(&entry,0,sizeof(entry));entry.score=3;
            legacy_notes(date);
        }
    }
    entry.version=1;entry.date=date;section=0;lumen_back=1;
    if(entry.score<1 || entry.score>5)entry.score=3;
    for(int i=0;i<6;i++)entry.answers[i][8191]=0;
}
static void draw(void) {
    if(!entry.date) {
        ui_paint_heading("Journal",0);
        for(int i=0;i<PAGE_SIZE && page*PAGE_SIZE+i<count;i++) {
            int date=dates[page*PAGE_SIZE+i],y=145+i*74;
            char label[40];date_label(label,date);
            ui_paint_hitbox(28,y,544,74);
            lumen_text(32,y+4,470,33,label,PAINT_HEADING,-1,1);
            lumen_text(32,y+36,470,30,date==today ?
                word("Heute · Gedanken festhalten","Today · Capture your thoughts","Aujourd’hui · Notez vos pensées") :
                word("Öffnen und weiter schreiben","Open and continue writing","Ouvrir et continuer"),
                PAINT_SECONDARY,-1,1);
            ui_paint_icon_button(528,y+15,IC_NEXT);
            ui_paint_line(28,y+73,572,y+73,1,190);
        }
        lumen_pager(745,page,(count+PAGE_SIZE-1)/PAGE_SIZE);
        return;
    }
    char label[40];date_label(label,entry.date);
    ui_paint_heading(label,section<6);
    lumen_select_row(140,word("Abschnitt","Section","Section"),prompt(section));
    if(section<6) {
        ui_paint_icon_button(528,74,IC_EDIT);
        ui_paint_hitbox(28,218,544,480);
        lumen_wrapped(30,225,540,470,*entry.answers[section] ? entry.answers[section] :
            word("Hier tippen und schreiben …","Tap here to write …","Touchez ici pour écrire…"),PAINT_BODY,12);
    } else {
        ui_paint_text(28,245,544,48,prompt(6),PAINT_HEADING);
        for(int i=1;i<=5;i++) {
            snprintf(label,sizeof(label),"%d",i);
            lumen_button(30+(i-1)*110,335,100,70,label,entry.score==i);
        }
        ui_paint_text(28,445,544,44,word("1 · schwer    —    5 · sehr gut","1 · difficult    —    5 · very good",
                                       "1 · difficile    —    5 · très bien"),PAINT_BODY);
    }
}
static void save_change(void) {
    if(!save())lumen_request("notify","",word("Speichern fehlgeschlagen.","Could not save.","Enregistrement impossible."),0);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){refresh();return 1;}
    if(in->kind==INPUT_BACK){entry.date=0;lumen_back=0;refresh();return 1;}
    if(in->kind==INPUT_PAGE && !entry.date){page=in->x;return 1;}
    if(in->kind==INPUT_TEXT && entry.date && in->x>=10 && in->x<16) {
        if(lumen_string("text",entry.answers[in->x-10],8192))save_change();
        return 1;
    }
    if(in->kind==INPUT_SELECT && entry.date && in->x==1 && in->y>=0 && in->y<7){section=in->y;return 1;}
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(!entry.date) {
        if(x>=28 && x<572 && y>=145 && y<663) {
            int i=page*PAGE_SIZE+(y-145)/74;
            if(i<count)open_day(dates[i]);
        }
    } else if(y>=140 && y<188 && x>=28 && x<572) {
        const char *options[7];
        for(int i=0;i<7;i++)options[i]=prompt(i);
        lumen_select(options,7,section,1);
    } else if(section<6 && ((x>=28 && x<572 && y>=218 && y<698) || (x>=528 && x<572 && y>=74 && y<118))) {
        lumen_multiline(prompt(section),entry.answers[section],10+section);
    } else if(section==6 && x>=30 && x<570 && y>=335 && y<405 && (x-30)%110<100) {
        entry.score=1+(x-30)/110;save_change();
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
