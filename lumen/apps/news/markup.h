static int convert_image(const char *source,const char *target) {
    const char *tool=getenv("LUMEN_DOCUMENT_TOOL");
    if(!tool || !*tool)return 0;
    pid_t child=fork();
    if(!child) {
        execl(tool,tool,"--document-image",source,target,"540","280",(char *)NULL);
        _exit(1);
    }
    return wait_success(child);
}
