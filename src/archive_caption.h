#ifndef LIMINAL_ARCHIVE_CAPTION_H
#define LIMINAL_ARCHIVE_CAPTION_H

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace liminal {
struct CaptionBounds { int x, y, width, height, font_pixels; };

// Windows' Unicode rasterizer preserves the archived spelling, punctuation and
// accents. Only a grayscale coverage mask is used, never a desktop screenshot.
inline CaptionBounds DrawArchiveCaption(std::vector<unsigned char>& pixels, int size,
    int center_x, int top, int width, int font_pixels, const std::string& utf8, int bottom_limit,
    const std::vector<CaptionBounds>* obstacles=nullptr)
{
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),nullptr,0);
    if (!count) throw std::runtime_error("Invalid or empty UTF-8 place title");
    std::wstring title(count,L' ');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),title.data(),count);
    struct Context {
        HDC dc=CreateCompatibleDC(nullptr);
        HFONT font=nullptr; HBITMAP bitmap=nullptr;
        HGDIOBJ old_font=nullptr,old_bitmap=nullptr;
        ~Context() {
            if (old_bitmap) SelectObject(dc,old_bitmap);
            if (old_font) SelectObject(dc,old_font);
            if (bitmap) DeleteObject(bitmap);
            if (font) DeleteObject(font);
            if (dc) DeleteDC(dc);
        }
    } context;
    if (!context.dc) throw std::runtime_error("Cannot create title rasterizer");
    width=std::clamp(width,32,size-16);
    context.font=CreateFontW(-font_pixels,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    if (!context.font) throw std::runtime_error("Cannot create title font");
    context.old_font=SelectObject(context.dc,context.font);
    constexpr UINT flags=DT_CENTER|DT_WORDBREAK|DT_NOPREFIX;
    RECT measured{0,0,width-4,0};
    if (!DrawTextW(context.dc,title.data(),count,&measured,flags|DT_CALCRECT))
        throw std::runtime_error("Cannot measure place title");
    width=std::min(size-16,std::max(width,static_cast<int>(measured.right)+4));
    const int height=static_cast<int>(measured.bottom)+4;
    int x=std::clamp(center_x-width/2,8,size-width-8);
    int y=std::clamp(top,8,std::max(8,bottom_limit-height));
    if (obstacles) {
        const int origin_x=x, origin_y=y;
        int best_cost=size*size;
        // Stay below the tile and choose the nearest free rectangle, keeping
        // names clear of the existing large room numbers and other captions.
        for (int cy=origin_y;cy+height<=bottom_limit;cy+=4) {
            if ((cy-origin_y)*3>=best_cost) break;
            for (int cx=8;cx+width<=size-8;cx+=4) {
                const int cost=std::abs(cx-origin_x)+(cy-origin_y)*3;
                if (cost>=best_cost) continue;
                bool collision=false;
                for (const auto& r:*obstacles) {
                    if (cx<r.x+r.width+3 && cx+width>r.x-3 && cy<r.y+r.height+3 && cy+height>r.y-3) {
                        collision=true; break;
                    }
                }
                if (!collision) { x=cx; y=cy; best_cost=cost; }
            }
        }
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    context.bitmap=CreateDIBSection(context.dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (!context.bitmap || !bits) throw std::runtime_error("Cannot allocate title bitmap");
    context.old_bitmap=SelectObject(context.dc,context.bitmap);
    auto* mask=static_cast<unsigned char*>(bits);
    std::fill(mask,mask+static_cast<size_t>(width)*height*4,static_cast<unsigned char>(0));
    SetTextColor(context.dc,RGB(255,255,255)); SetBkMode(context.dc,TRANSPARENT);
    RECT draw{2,2,width-2,height-2};
    if (!DrawTextW(context.dc,title.data(),count,&draw,flags)) throw std::runtime_error("Cannot draw place title");
    GdiFlush();
    auto blend=[&](int px,int py,float alpha,float r,float g,float b) {
        if (px<0 || py<0 || px>=size || py>=size) return;
        const size_t index=static_cast<size_t>((py*size+px)*3);
        const float color[]={r,g,b};
        for (int c=0;c<3;++c) pixels[index+c]=static_cast<unsigned char>(pixels[index+c]*(1-alpha)+color[c]*alpha);
    };
    // Small dark outline keeps the thin letters readable over terrain and links.
    for (int py=0;py<height;++py) for (int px=0;px<width;++px) {
        const float alpha=mask[(py*width+px)*4]/255.0f;
        if (!alpha) continue;
        for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx)
            blend(x+px+dx,y+py+dy,alpha*.48f,20,24,22);
    }
    for (int py=0;py<height;++py) for (int px=0;px<width;++px)
        blend(x+px,y+py,mask[(py*width+px)*4]/255.0f,250,245,225);
    return {x,y,width,height,font_pixels};
}
} // namespace liminal
#endif
#endif
