/* Generic private document raster tool for SDK packages. */
static int render_document(const char *file,int number,int width,int height,const char *output) {
    if(number<0 || width<64 || width>600 || height<64 || height>800)return 0;
    fz_context *context=fz_new_context(NULL,NULL,12u<<20);
    if(!context)return 0;
    fz_register_document_handlers(context);
    fz_document *document=NULL;
    fz_page *page=NULL;
    fz_pixmap *pixels=NULL;
    fz_device *canvas=NULL;
    fz_link *links=NULL;
    FILE *file_out=NULL;
    int ok=0;
    fz_var(document);fz_var(page);fz_var(pixels);fz_var(canvas);fz_var(links);fz_var(file_out);fz_var(ok);
    fz_try(context) {
        document=fz_open_document(context,file);
        fz_layout_document(context,document,width,height,19);
        int count=fz_count_pages(context,document);
        if(number>=count || count>4096)fz_throw(context,FZ_ERROR_LIMIT,"Document pagination failed");
        page=fz_load_page(context,document,number);
        fz_rect bounds=fz_bound_page(context,page);
        if(fz_is_empty_rect(bounds) || fz_is_infinite_rect(bounds))
            fz_throw(context,FZ_ERROR_FORMAT,"Invalid page dimensions");
        float scale=fminf(width/(bounds.x1-bounds.x0),height/(bounds.y1-bounds.y0));
        fz_matrix transform={scale,0,0,scale,-bounds.x0*scale,-bounds.y0*scale};
        pixels=fz_new_pixmap(context,fz_device_gray(context),width,height,NULL,0);
        fz_clear_pixmap_with_value(context,pixels,255);
        canvas=fz_new_draw_device(context,fz_identity,pixels);
        fz_run_page(context,page,canvas,transform,NULL);
        fz_close_device(context,canvas);
        file_out=fopen(output,"wb");
        if(!file_out)fz_throw(context,FZ_ERROR_SYSTEM,"Could not create page raster");
        int stride=fz_pixmap_stride(context,pixels);
        unsigned char *samples=fz_pixmap_samples(context,pixels);
        for(int y=0;y<height;y++)
            if(fwrite(samples+y*stride,1,(size_t)width,file_out)!=(size_t)width)
                fz_throw(context,FZ_ERROR_SYSTEM,"Could not write page raster");
        printf("%d\n",count);
        links=fz_load_links(context,page);
        int emitted=0;
        for(fz_link *link=links;link && emitted<64;link=link->next) {
            fz_rect box=fz_intersect_rect(fz_transform_rect(link->rect,transform),
                                          (fz_rect){0,0,width,height});
            if(fz_is_empty_rect(box) || !link->uri || strpbrk(link->uri,"\t\r\n") ||
               strlen(link->uri)>1023)continue;
            char anchor[24];
            const char *target=link->uri;
            if(!fz_is_external_link(context,target)) {
                int p=fz_page_number_from_location(context,document,fz_resolve_link(context,document,target,NULL,NULL));
                if(p<0 || p>=count)continue;
                snprintf(anchor,sizeof(anchor),"#%d",p);
                target=anchor;
            }
            fprintf(stdout,"%d\t%d\t%d\t%d\t%s\n",(int)floorf(box.x0),(int)floorf(box.y0),
                    (int)ceilf(box.x1),(int)ceilf(box.y1),target);
            emitted++;
        }
        ok=!fflush(file_out) && !fflush(stdout);
    }
    fz_always(context) {
        if(file_out && fclose(file_out))ok=0;
        fz_drop_link(context,links);
        fz_drop_device(context,canvas);
        fz_drop_pixmap(context,pixels);
        fz_drop_page(context,page);
        fz_drop_document(context,document);
    }
    fz_catch(context) {ok=0;}
    fz_drop_context(context);
    return ok;
}
