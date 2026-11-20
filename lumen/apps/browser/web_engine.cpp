#include <litehtml.h>
#include <litehtml/render_item.h>
#include <litehtml/media_query.h>
#include <litehtml/style.h>
#include <string_view>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 1600
#define STBI_NO_SIMD
#include "stb_truetype.h"
#include "stb_image.h"
#include "web_engine.h"

using namespace litehtml;

namespace {
using Bytes=std::vector<unsigned char>;

Bytes read_file(const char *path,size_t limit) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    const auto length=stream.tellg();
    if(length<=0 || static_cast<unsigned long long>(length)>limit)return {};
    Bytes contents(static_cast<size_t>(length));
    stream.seekg(0);
    if(!stream.read(reinterpret_cast<char *>(contents.data()),length))return {};
    return contents;
}
struct Face {
    Bytes data;
    stbtt_fontinfo font{};
    bool load(const std::string &file) {
        data=read_file(file.c_str(),4u<<20);
        return !data.empty() && stbtt_InitFont(&font,data.data(),0);
    }
};
struct Font {
    const stbtt_fontinfo *face;
    float scale;
    int ascent,weight;
    bool underline,italic;
};
struct Image {
    std::unique_ptr<unsigned char,decltype(&stbi_image_free)> pixels{nullptr,stbi_image_free};
    int width=0,height=0;
};

int gray(web_color color) { return (54*color.red+183*color.green+19*color.blue)>>8; }

int next_rune(const unsigned char *&at) {
    unsigned value=*at++;
    if(value<128)return value;
    int count=value>=0xf0 && value<0xf5 ? 3 : value>=0xe0 && value<0xf0 ? 2 : value>=0xc2 && value<0xe0 ? 1 : 0;
    if(!count)return 0xfffd;
    value&=(1u<<(6-count))-1;
    while(count--) {
        if((*at&192)!=128)return 0xfffd;
        value=(value<<6)|(*at++&63);
    }
    return value>0x10ffff || (value>=0xd800 && value<=0xdfff) ? 0xfffd : value;
}

Bytes decode_image(const std::string &url) {
    auto comma=url.find(',');
    if(comma==std::string::npos || url.size()>400000 ||
       (url.compare(0,22,"data:image/png;base64,") && url.compare(0,23,"data:image/jpeg;base64,") &&
        url.compare(0,22,"data:image/gif;base64,")))return {};
    Bytes data;
    unsigned value=0,bits=0;
    for(size_t i=comma+1;i<url.size();i++) {
        unsigned c=url[i],n;
        if(c>='A' && c<='Z')n=c-'A';
        else if(c>='a' && c<='z')n=c-'a'+26;
        else if(c>='0' && c<='9')n=c-'0'+52;
        else if(c=='+' || c=='/')n=c=='+' ? 62 : 63;
        else if(c=='=')break;
        else return {};
        value=(value<<6)|n;
        bits+=6;
        if(bits>=8){bits-=8;data.push_back(static_cast<unsigned char>(value>>bits));}
    }
    return data;
}

media_features viewport_features(int width,int height) {
    media_features media;
    media.type=media_type_screen;
    media.width=media.device_width=width;
    media.height=media.device_height=height;
    media.resolution=96;
    media.monochrome=8;
    return media;
}

bool supported_declaration(std::string_view declaration) {
    const auto colon=declaration.find(':');
    if(colon==std::string_view::npos)return false;
    auto property=declaration.substr(0,colon);
    const auto start=property.find_first_not_of(" \t\r\n");
    const auto end=property.find_last_not_of(" \t\r\n");
    if(start==std::string_view::npos)return false;
    std::string name(property.substr(start,end-start+1));
    for(char &c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const auto id=_id(name);
    style candidate;
    candidate.add_property(id,std::string(declaration.substr(colon+1)));
    return !std::holds_alternative<invalid>(candidate.get_property(id));
}

bool supported_condition(std::string_view condition,int depth=0) {
    if(depth>=8 || condition.size()>512)return false;
    const auto start=condition.find_first_not_of(" \t\r\n");
    const auto end=condition.find_last_not_of(" \t\r\n");
    if(start==std::string_view::npos)return false;
    condition=condition.substr(start,end-start+1);
    if(condition.substr(0,4)=="not ")return !supported_condition(condition.substr(4),depth+1);
    if(condition.size()<3 || condition.front()!='(' || condition.back()!=')')return false;
    condition.remove_prefix(1);
    condition.remove_suffix(1);
    return supported_declaration(condition);
}

size_t css_skip_literal(std::string_view css,size_t i) {
    if(css[i]=='"' || css[i]==39) {
        const char delimiter=css[i++];
        while(i<css.size() && css[i]!=delimiter) {
            if(css[i]=='\\' && i+1<css.size())++i;
            ++i;
        }
        return std::min(i+1,css.size());
    }
    if(css[i]=='/' && i+1<css.size() && css[i+1]=='*') {
        const auto end=css.find("*/",i+2);
        return end==std::string_view::npos ? css.size() : end+2;
    }
    if(css[i]=='\\' && i+1<css.size())return i+2;
    return i;
}

/* Keep rules within feature queries or cascade-layer blocks that this renderer
   can use. Layers retain source order in the lightweight renderer. */
std::string compatible_css(std::string_view css,int depth=0) {
    if(depth>=16 || (css.find("@layer")==std::string_view::npos &&
                     css.find("@supports")==std::string_view::npos))return std::string(css);
    std::string out;
    out.reserve(css.size());
    for(size_t i=0;i<css.size();) {
        const auto literal=css_skip_literal(css,i);
        if(literal!=i) {
            out.append(css.substr(i,literal-i));
            i=literal;
            continue;
        }
        if(std::isspace(static_cast<unsigned char>(css[i]))) {
            out+=css[i++];
            continue;
        }
        const auto begin=i;
        int parens=0;
        for(;i<css.size();) {
            const auto skip=css_skip_literal(css,i);
            if(skip!=i){i=skip;continue;}
            if(css[i]=='(' || css[i]=='[')++parens;
            if(css[i]==')' || css[i]==']')parens=std::max(0,parens-1);
            if(!parens && (css[i]==';' || css[i]=='{'))break;
            ++i;
        }
        const auto prelude=css.substr(begin,i-begin);
        const bool layer=prelude.substr(0,6)=="@layer" &&
            (prelude.size()==6 || std::isspace(static_cast<unsigned char>(prelude[6])));
        const bool support=prelude.substr(0,9)=="@supports" &&
            (prelude.size()==9 || std::isspace(static_cast<unsigned char>(prelude[9])));
        if(i==css.size() || css[i]==';') {
            if(!layer)out.append(css.substr(begin,std::min(i+1,css.size())-begin));
            if(i<css.size())++i;
            continue;
        }
        const auto body=++i;
        int level=1;
        while(i<css.size() && level) {
            const auto skip=css_skip_literal(css,i);
            if(skip!=i){i=skip;continue;}
            if(css[i]=='{')++level;
            if(css[i]=='}')--level;
            if(level)++i;
        }
        if(level) {out.append(css.substr(begin));break;}
        if(layer || (support && supported_condition(prelude.substr(9))))
            out+=compatible_css(css.substr(body,i-body),depth+1);
        else if(!support) {
            out.append(prelude);
            out+='{';
            if(prelude.substr(0,6)=="@media")out+=compatible_css(css.substr(body,i-body),depth+1);
            else out.append(css.substr(body,i-body));
            out+='}';
        }
        ++i;
    }
    return out;
}

class Screen final : public document_container {
    Face sans,serif,cjk;
    bool has_serif=false,has_cjk=false;
    int width,height;
    std::map<std::string,Image> images;
    std::vector<position> clips;
    unsigned image_requests=0,style_requests=0;

    position fit(position box) const {
        const auto &clip=clips.back();
        if(!std::isfinite(box.x) || !std::isfinite(box.y) ||
           !std::isfinite(box.width) || !std::isfinite(box.height))return {};
        pixel_t left=std::min(clip.right(),std::max(box.left(),clip.left()));
        pixel_t top=std::min(clip.bottom(),std::max(box.top(),clip.top()));
        return {left,top,std::max(0.f,std::min(box.right(),clip.right())-left),
                         std::max(0.f,std::min(box.bottom(),clip.bottom())-top)};
    }
    void pixel(int x,int y,int tone,int alpha) {
        if(!alpha)return;
        const auto &clip=clips.back();
        if(x<clip.left() || x>=clip.right() || y<clip.top() || y>=clip.bottom())return;
        auto &p=pixels[static_cast<size_t>(y)*width+x];
        p=static_cast<unsigned char>((p*(255-alpha)+tone*alpha+127)/255);
    }
    void fill(position box,web_color color,int radius=0) {
        if(!color.alpha)return;
        position bounded=fit(box);
        int tone=gray(color);
        radius=std::clamp(radius,0,std::max(0,int(std::min(box.width,box.height)/2)));
        for(int y=bounded.top();y<bounded.bottom();y++)for(int x=bounded.left();x<bounded.right();x++) {
            if(radius) {
                int dx=std::max({box.left()+radius-x, x-(box.right()-radius-1),0.f});
                int dy=std::max({box.top()+radius-y, y-(box.bottom()-radius-1),0.f});
                if(dx && dy && dx*dx+dy*dy>radius*radius)continue;
            }
            pixel(x,y,tone,color.alpha);
        }
    }
    Image &image(const std::string &url,const char *base) {
        auto [entry,added]=images.try_emplace(url);
        auto &result=entry->second;
        if(!added)return result;
        auto bytes=decode_image(url);
        if(bytes.empty() && image_requests<8 && url.compare(0,5,"data:")) {
            size_t size=0;
            ++image_requests;
            std::unique_ptr<unsigned char,decltype(&std::free)> remote(
                web_engine_image(url.c_str(),base,&size),std::free);
            if(remote && size<=256u*1024)bytes.assign(remote.get(),remote.get()+size);
        }
        if(bytes.empty())return result;
        int channels=0;
        if(!stbi_info_from_memory(bytes.data(),static_cast<int>(bytes.size()),&result.width,&result.height,&channels) ||
           result.width<1 || result.height<1 || result.width>1600 || result.height>1600) {
            result.width=result.height=0;return result;
        }
        result.pixels.reset(stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),
                                                  &result.width,&result.height,&channels,2));
        if(!result.pixels)result.width=result.height=0;
        return result;
    }
    const stbtt_fontinfo *face_for(const Font &font,int code) const {
        if(has_cjk && code>255 && !stbtt_FindGlyphIndex(font.face,code) &&
           stbtt_FindGlyphIndex(&cjk.font,code))return &cjk.font;
        return font.face;
    }
public:
    Bytes pixels;
    bool ready=false;
    explicit Screen(int w,int h,const std::string &assets,const char *fallback,bool allocate=true) :
        width(w),height(h),pixels(allocate ? static_cast<size_t>(w)*h : 0,255) {
        if(!allocate)return;
        ready=sans.load(assets+"/assets/NotoSans-Regular.otf");
        has_serif=serif.load(assets+"/assets/NotoSerif-Regular.otf");
        if(fallback && *fallback)has_cjk=cjk.load(fallback);
        clips.emplace_back(0,0,w,h);
    }
    uint_ptr create_font(const font_description &style,const document *,font_metrics *metrics) override {
        int size=std::isfinite(style.size) ? static_cast<int>(std::clamp(style.size,6.f,112.f)) : 17;
        if(!ready) {
            metrics->font_size=metrics->height=size;
            metrics->ascent=size*0.8f;
            metrics->descent=size-metrics->ascent;
            metrics->x_height=metrics->ch_width=size*0.5f;
            metrics->draw_spaces=false;
            return 0;
        }
        const auto *face=has_serif && style.family.find("serif")!=std::string::npos &&
            style.family.find("sans")==std::string::npos ? &serif.font : &sans.font;
        float scale=stbtt_ScaleForMappingEmToPixels(face,static_cast<float>(size));
        int asc,des,gap;
        stbtt_GetFontVMetrics(face,&asc,&des,&gap);
        auto *font=new Font{face,scale,static_cast<int>(std::ceil(asc*scale)),style.weight,
            (style.decoration_line&text_decoration_line_underline)!=0,style.style==font_style_italic};
        metrics->font_size=size;
        metrics->ascent=font->ascent;
        metrics->descent=static_cast<int>(std::ceil(-des*scale));
        metrics->height=std::max<float>(size,metrics->ascent+metrics->descent);
        metrics->x_height=size/2;
        metrics->ch_width=text_width("0",reinterpret_cast<uint_ptr>(font));
        metrics->draw_spaces=false;
        return reinterpret_cast<uint_ptr>(font);
    }
    void delete_font(uint_ptr handle) override {delete reinterpret_cast<Font *>(handle);}
    pixel_t text_width(const char *text,uint_ptr handle) override {
        if(!handle || !text)return 0;
        const auto &font=*reinterpret_cast<Font *>(handle);
        auto p=reinterpret_cast<const unsigned char *>(text);
        float width=0;
        for(int previous=0;*p;) {
            int code=next_rune(p),advance,bearing;
            const auto *face=face_for(font,code);
            stbtt_GetCodepointHMetrics(face,code,&advance,&bearing);
            if(previous && face==font.face)width+=stbtt_GetCodepointKernAdvance(face,previous,code)*font.scale;
            width+=advance*font.scale;
            previous=code;
        }
        return static_cast<pixel_t>(std::ceil(width));
    }
    void draw_text(uint_ptr,const char *text,uint_ptr handle,web_color color,const position &box) override {
        auto visible=fit(box);
        if(!handle || !text || visible.height<=0 || visible.width<=0 ||
           box.x<-100000 || box.y<-100000)return;
        const auto &font=*reinterpret_cast<Font *>(handle);
        auto p=reinterpret_cast<const unsigned char *>(text);
        float pen=static_cast<float>(box.x);
        int baseline=box.y+font.ascent,value=gray(color);
        for(int previous=0;*p;) {
            int code=next_rune(p),w,h,x0,y0,advance,bearing;
            const auto *face=face_for(font,code);
            if(previous && face==font.face)pen+=stbtt_GetCodepointKernAdvance(face,previous,code)*font.scale;
            const int x=static_cast<int>(std::lround(pen));
            auto *bitmap=stbtt_GetCodepointBitmap(face,font.scale,font.scale,code,&w,&h,&x0,&y0);
            if(bitmap) {
                for(int row=0;row<h;row++)for(int col=0;col<w;col++) {
                    int alpha=bitmap[row*w+col]*color.alpha/255;
                    int slant=font.italic ? std::max(0,-(y0+row)/5) : 0;
                    pixel(x+x0+col+slant,baseline+y0+row,value,alpha);
                    if(font.weight>=650)pixel(x+x0+col+slant+1,baseline+y0+row,value,alpha/2);
                }
                stbtt_FreeBitmap(bitmap,nullptr);
            }
            stbtt_GetCodepointHMetrics(face,code,&advance,&bearing);
            pen+=advance*font.scale;
            previous=code;
        }
        if(font.underline)fill(position(box.x,baseline+2,std::max(0.f,pen-box.x),1),color);
    }
    pixel_t pt_to_px(float pt) const override {return static_cast<pixel_t>(std::lround(pt*4/3));}
    pixel_t get_default_font_size() const override {return 17;}
    const char *get_default_font_name() const override {return "sans-serif";}
    void draw_list_marker(uint_ptr hdc,const list_marker &marker) override {
        if(marker.image.size())return;
        if(marker.marker_type==list_style_type_decimal && marker.font) {
            auto text=std::to_string(marker.index)+".";
            draw_text(hdc,text.c_str(),marker.font,marker.color,marker.pos);
        } else {
            int size=std::max(2,int(std::min(marker.pos.width,marker.pos.height)/3));
            fill(position(marker.pos.x,marker.pos.y+marker.pos.height/2-size/2,size,size),marker.color,size/2);
        }
    }
    void load_image(const char *src,const char *base,bool) override {if(src)image(src,base);}
    void get_image_size(const char *src,const char *base,size &size) override {
        auto &r=image(src ? src : "",base);
        size.width=r.width;size.height=r.height;
    }
    void draw_image(uint_ptr,const background_layer &layer,const std::string &url,const std::string &base) override {
        auto &r=image(url,base.c_str());
        const auto &box=layer.origin_box;
        if(!std::isfinite(box.x) || !std::isfinite(box.y) || std::fabs(box.x)>100000 ||
           std::fabs(box.y)>100000 || !(box.width>0 && box.width<=100000) ||
           !(box.height>0 && box.height<=100000))return;
        const int w=static_cast<int>(box.width),h=static_cast<int>(box.height);
        if(!r.pixels || w<=0 || h<=0)return;
        auto bounded=fit(layer.clip_box);
        for(int y=bounded.top();y<bounded.bottom();y++)for(int x=bounded.left();x<bounded.right();x++) {
            int sx=x-box.x,sy=y-box.y;
            if(layer.repeat==background_repeat_repeat || layer.repeat==background_repeat_repeat_x)
                sx=(sx%w+w)%w;
            if(layer.repeat==background_repeat_repeat || layer.repeat==background_repeat_repeat_y)
                sy=(sy%h+h)%h;
            if(sx<0 || sx>=w || sy<0 || sy>=h)continue;
            const size_t index=2u*(static_cast<size_t>(sy)*r.height/h*r.width+
                                    static_cast<size_t>(sx)*r.width/w);
            pixel(x,y,r.pixels.get()[index],r.pixels.get()[index+1]);
        }
    }
    void draw_solid_fill(uint_ptr,const background_layer &layer,const web_color &color) override {
        fill(layer.is_root ? clips.front() : layer.clip_box,color,layer.border_radius.top_left_x);
    }
    template <typename Sample>
    void draw_gradient(const background_layer &layer,const background_layer::gradient_base &gradient,Sample sample) {
        const auto &points=gradient.color_points;
        if(points.empty())return;
        const auto box=fit(layer.clip_box);
        for(int y=box.top();y<box.bottom();y++)for(int x=box.left();x<box.right();x++) {
            const double position=sample(x+0.5,y+0.5);
            const auto stop=std::upper_bound(points.begin(),points.end(),position,
                [](double position,const auto &point){return position<point.offset;});
            if(stop==points.begin() || stop==points.end()) {
                const auto &color=(stop==points.begin() ? points.front() : points.back()).color;
                pixel(x,y,gray(color),color.alpha);
                continue;
            }
            const auto &left=*(stop-1), &right=*stop;
            const double t=(position-left.offset)/(right.offset-left.offset);
            const double left_alpha=left.color.alpha*(1-t),right_alpha=right.color.alpha*t;
            const double alpha=left_alpha+right_alpha;
            if(alpha>0)pixel(x,y,std::lround((gray(left.color)*left_alpha+gray(right.color)*right_alpha)/alpha),
                             std::lround(alpha));
        }
    }
    void draw_linear_gradient(uint_ptr,const background_layer &layer,const background_layer::linear_gradient &value) override {
        const double dx=value.end.x-value.start.x,dy=value.end.y-value.start.y,scale=dx*dx+dy*dy;
        if(!std::isfinite(scale) || scale<=0)return;
        draw_gradient(layer,value,[&](double x,double y){
            return ((x-value.start.x)*dx+(y-value.start.y)*dy)/scale;
        });
    }
    void draw_radial_gradient(uint_ptr,const background_layer &layer,const background_layer::radial_gradient &value) override {
        if(!(value.radius.x>0 && value.radius.y>0))return;
        draw_gradient(layer,value,[&](double x,double y){
            return std::hypot((x-value.position.x)/value.radius.x,(y-value.position.y)/value.radius.y);
        });
    }
    void draw_conic_gradient(uint_ptr,const background_layer &layer,const background_layer::conic_gradient &value) override {
        draw_gradient(layer,value,[&](double x,double y){
            constexpr double tau=6.283185307179586;
            const double turn=std::atan2(x-value.position.x,value.position.y-y)/tau-value.angle/360.0;
            return turn-std::floor(turn);
        });
    }
    void draw_borders(uint_ptr,const borders &b,const position &box,bool) override {
        if(b.top.width>0)fill({box.x,box.y,box.width,b.top.width},b.top.color);
        if(b.bottom.width>0)fill({box.x,box.bottom()-b.bottom.width,box.width,b.bottom.width},b.bottom.color);
        if(b.left.width>0)fill({box.x,box.y,b.left.width,box.height},b.left.color);
        if(b.right.width>0)fill({box.right()-b.right.width,box.y,b.right.width,box.height},b.right.color);
    }
    void set_caption(const char *) override {}
    void set_base_url(const char *) override {}
    void link(const std::shared_ptr<document> &,const element::ptr &) override {}
    void on_anchor_click(const char *,const element::ptr &) override {}
    void on_mouse_event(const element::ptr &,mouse_event) override {}
    void set_cursor(const char *) override {}
    void transform_text(string &text,text_transform transform) override {
        bool capital=true;
        for(char &c:text) {
            unsigned u=static_cast<unsigned char>(c);
            if(u>=128){capital=false;continue;}
            if(transform==text_transform_uppercase || (transform==text_transform_capitalize && capital))
                c=static_cast<char>(std::toupper(u));
            else if(transform==text_transform_lowercase)c=static_cast<char>(std::tolower(u));
            capital=std::isspace(u)!=0;
        }
    }
    void import_css(string &text,const string &url,string &base) override {
        if(style_requests>=8)return;
        ++style_requests;
        char final[1024]={};
        std::unique_ptr<char,decltype(&std::free)> css(web_engine_stylesheet(url.c_str(),base.c_str(),final),std::free);
        if(css){text=compatible_css(css.get());base=final;}
    }
    void set_clip(const position &box,const border_radiuses &) override {clips.push_back(fit(box));}
    void del_clip() override {if(clips.size()>1)clips.pop_back();}
    void get_viewport(position &box) const override {box=position(0,0,width,height);}
    element::ptr create_element(const char *,const string_map &,const std::shared_ptr<document> &) override {return nullptr;}
    void get_media_features(media_features &media) const override {
        media=viewport_features(width,height);
    }
    void get_language(string &language,string &culture) const override {language="en";culture="en";}
};

}

extern "C" int web_engine_css_supports(const char *property,const char *value) {
    if(!property || std::strlen(property)>512 || (value && std::strlen(value)>512))return 0;
    try {
        if(!value)return supported_condition(property);
        return supported_declaration(std::string(property)+":"+value);
    } catch(const std::exception &) {
        return 0;
    }
}
extern "C" void web_engine_inline_css(FILE *out,const char *css) {
    try {
        const auto compatible=compatible_css(css);
        std::fwrite(compatible.data(),1,compatible.size(),out);
    } catch(const std::exception &) {
        std::fputs(css,out);
    }
}

extern "C" int web_engine_media_matches(const char *query,int width,int height) {
    if(!query || !*query || std::strlen(query)>512)return 0;
    try {
        Screen viewport(width,height,"",nullptr,false);
        auto doc=document::createFromString(estring("<html><body></body></html>"),&viewport);
        const auto queries=parse_media_query_list(query,doc);
        return !queries.empty() && queries.check(viewport_features(width,height));
    } catch(const std::exception &) {
        return 0;
    }
}

extern "C" int web_engine_render(const char *html,const char *raster,int page,int width,int height,
                                 const char *assets,const char *fallback,FILE *metadata) {
    if(!assets || page<0 || width<64 || width>600 || height<64 || height>800)return 0;
    try {
        const auto bytes=read_file(html,8u<<20);
        if(bytes.empty())return 0;
        Screen screen(width,height,assets,fallback);
        if(!screen.ready)return 0;
        const std::string content(bytes.begin(),bytes.end());
        auto doc=document::createFromString(estring(content),&screen);
        if(!doc || !doc->root())return 0;
        doc->render(width);
        const int step=height-24;
        if(!std::isfinite(doc->height()) || doc->height()>4096.f*step)return 0;
        int pages=1+(std::max(0,static_cast<int>(std::ceil(doc->height()))-height)+step-1)/step;
        if(page>=pages || pages>4096)return 0;
        position viewport(0,0,width,height);
        doc->draw(0,0,-page*step,&viewport);
        if(page==0) {
            const auto range=std::minmax_element(screen.pixels.begin(),screen.pixels.end());
            if(*range.second-*range.first<12)return 2;
        }
        FILE *file=std::fopen(raster,"wb");
        if(!file)return 0;
        const bool written=std::fwrite(screen.pixels.data(),1,screen.pixels.size(),file)==screen.pixels.size();
        if(std::fclose(file) || !written)return 0;
        std::fprintf(metadata,"%d\n",pages);
        int emitted=0;
        std::set<std::string> emitted_controls;
        bool missing_control=false;
        auto targets=doc->root()->select_all("[data-lumen-action]");
        const auto anchors=doc->root()->select_all("a[href]");
        targets.insert(targets.end(),anchors.begin(),anchors.end());
        for(const auto &anchor:targets) {
            const char *uri=anchor->get_attr("data-lumen-action");
            if(!uri)uri=anchor->get_attr("href");
            if(!uri || strpbrk(uri,"\r\n\t") || std::strlen(uri)>1023)continue;
            const bool control=!std::strncmp(uri,"lumen-",6);
            auto item=anchor->get_render_item();
            if(!item) {
                missing_control=missing_control || control;
                continue;
            }
            std::string jump;
            if(*uri=='#') {
                for(const auto &target:doc->root()->select_all("[id]")) {
                    const char *id=target->get_attr("id");
                    if(id && !strcmp(id,uri+1)) {
                        float y=target->get_placement().y;
                        if(std::isfinite(y))jump="#"+std::to_string(static_cast<int>(std::clamp(y/step,0.f,float(pages-1))));
                        break;
                    }
                }
                if(jump.empty())continue;
                uri=jump.c_str();
            }
            position::vector boxes;
            item->get_rendering_boxes(boxes);
            bool covered=false;
            for(const auto &box:boxes) {
                if(!std::isfinite(box.left()) || !std::isfinite(box.right()) ||
                   !std::isfinite(box.top()) || !std::isfinite(box.bottom()))continue;
                int x=static_cast<int>(std::clamp(std::floor(box.left()),0.f,float(width)));
                int y=static_cast<int>(std::clamp(std::floor(box.top())-page*step,0.f,float(height)));
                int right=static_cast<int>(std::clamp(std::ceil(box.right()),0.f,float(width)));
                int bottom=static_cast<int>(std::clamp(std::ceil(box.bottom())-page*step,0.f,float(height)));
                if(emitted<64 && x<right && y<bottom) {
                    std::fprintf(metadata,"%d\t%d\t%d\t%d\t%s\n",x,y,right,bottom,uri);
                    emitted++;
                    covered=true;
                }
            }
            if(control) {
                if(covered)emitted_controls.emplace(uri);
                else missing_control=true;
            }
        }
        if(missing_control && emitted<64) {
            struct Hit {int x0,y0,x1,y1;};
            std::map<std::string,Hit> hits;
            /* Inline controls inside positioned wrappers can have no independent
               boxes. Ask the same layout tree that receives pointer events. */
            for(int y=7;y<height;y+=15)for(int x=7;x<width;x+=15) {
                auto element=doc->root_render()->get_element_by_point(x,y+page*step,x,y,nullptr);
                const char *url=nullptr;
                for(;element && !url;element=element->parent()) {
                    url=element->get_attr("data-lumen-action");
                    if(!url)url=element->get_attr("href");
                }
                if(!url || std::strncmp(url,"lumen-",6) || emitted_controls.count(url))continue;
                auto [where,inserted]=hits.try_emplace(url,Hit{x,y,x,y});
                if(!inserted) {
                    auto &hit=where->second;
                    hit.x0=std::min(hit.x0,x);hit.y0=std::min(hit.y0,y);
                    hit.x1=std::max(hit.x1,x);hit.y1=std::max(hit.y1,y);
                }
            }
            for(const auto &[url,hit]:hits) {
                if(emitted==64)break;
                std::fprintf(metadata,"%d\t%d\t%d\t%d\t%s\n",
                    std::max(0,hit.x0-7),std::max(0,hit.y0-7),
                    std::min(width,hit.x1+8),std::min(height,hit.y1+8),url.c_str());
                emitted++;
            }
        }
        return !std::ferror(metadata);
    } catch(const std::exception &) {
        return 0;
    }
}
