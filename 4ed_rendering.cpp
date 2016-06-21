/*
* Mr. 4th Dimention - Allen Webster
*
* 12.17.2014
*
* Rendering layer for project codename "4ed"
*
*/

// TOP

inline void
draw_set_clip(Render_Target *target, i32_Rect clip_box){
    glScissor(clip_box.x0,
              target->height - clip_box.y1,
              clip_box.x1 - clip_box.x0,
              clip_box.y1 - clip_box.y0);
}

inline void
draw_bind_texture(Render_Target *target, i32 texid){
    if (target->bound_texture != texid){
        glBindTexture(GL_TEXTURE_2D, texid);
        target->bound_texture = texid;
    }
}

inline void
draw_set_color(Render_Target *target, u32 color){
    if (target->color != color){
        target->color = color;
        Vec4 c = unpack_color4(color);
        glColor4f(c.r, c.g, c.b, c.a);
    }
}

inline void
draw_safe_push(Render_Target *target, i32 size, void *x){
    if (size + target->size <= target->max){
        memcpy(target->push_buffer + target->size, x, size);
        target->size += size;
    }
}

#define PutStruct(s,x) draw_safe_push(target, sizeof(s), &x)

internal void
draw_push_piece(Render_Target *target, Render_Piece_Combined piece){
    PutStruct(Render_Piece_Header, piece.header);
    
    switch (piece.header.type){
        case piece_type_rectangle:
        case piece_type_outline:
        PutStruct(Render_Piece_Rectangle, piece.rectangle);
        break;
        
        case piece_type_gradient:
        PutStruct(Render_Piece_Gradient, piece.gradient);
        break;
        
        case piece_type_glyph:
        case piece_type_mono_glyph:
        PutStruct(Render_Piece_Glyph, piece.glyph);
        break;
        
        case piece_type_mono_glyph_advance:
        PutStruct(Render_Piece_Glyph_Advance, piece.glyph_advance);
        break;
    }
    
    Assert(target->size <= target->max);
}

internal void
draw_push_piece_clip(Render_Target *target, i32_Rect clip_box){
    // TODO(allen): optimize out if there are two clip box changes in a row
    Render_Piece_Change_Clip clip;
    Render_Piece_Header header;
    
    header.type = piece_type_change_clip;
    clip.box = clip_box;
    
    PutStruct(Render_Piece_Header, header);
    PutStruct(Render_Piece_Change_Clip, clip);
}

internal void
draw_push_clip(Render_Target *target, i32_Rect clip_box){
    Assert(target->clip_top == -1 ||
           fits_inside(clip_box, target->clip_boxes[target->clip_top]));
    Assert(target->clip_top+1 < ArrayCount(target->clip_boxes));
    target->clip_boxes[++target->clip_top] = clip_box;
    
    draw_push_piece_clip(target, clip_box);
}

internal i32_Rect
draw_pop_clip(Render_Target *target){
    i32_Rect result = {0};
    i32_Rect clip_box = {0};
    
    Assert(target->clip_top > 0);
    result = target->clip_boxes[target->clip_top];
    --target->clip_top;
    clip_box = target->clip_boxes[target->clip_top];
    draw_push_piece_clip(target, clip_box);
    
    return(result);
}

#define ExtractStruct(s) ((s*)cursor); cursor += sizeof(s)

inline void
private_draw_rectangle(Render_Target *target, f32_Rect rect, u32 color){
    draw_set_color(target, color);
    draw_bind_texture(target, 0);
    glBegin(GL_QUADS);
    {
        glVertex2f(rect.x0, rect.y0);
        glVertex2f(rect.x0, rect.y1);
        glVertex2f(rect.x1, rect.y1);
        glVertex2f(rect.x1, rect.y0);
    }
    glEnd();
}

inline void
private_draw_rectangle_outline(Render_Target *target, f32_Rect rect, u32 color){
    f32_Rect r;
    r.x0 = rect.x0 + .5f;
    r.y0 = rect.y0 + .5f;
    r.x1 = rect.x1 - .5f;
    r.y1 = rect.y1 - .5f;
    
    draw_set_color(target, color);
    draw_bind_texture(target, 0);
    glBegin(GL_LINE_STRIP);
    {
        glVertex2f(r.x0, r.y0);
        glVertex2f(r.x1, r.y0);
        glVertex2f(r.x1, r.y1);
        glVertex2f(r.x0, r.y1);
        glVertex2f(r.x0, r.y0);
    }
    glEnd();
}

inline void
private_draw_gradient(Render_Target *target, f32_Rect rect,
                      Vec4 color_left, Vec4 color_right){
    Vec4 cl = color_left;
    Vec4 cr = color_right;
    
    draw_bind_texture(target, 0);
    glBegin(GL_QUADS);
    {
        glColor4f(cl.r, cl.g, cl.b, cl.a);
        glVertex2f(rect.x0, rect.y0);
        glVertex2f(rect.x0, rect.y1);
        
        glColor4f(cr.r, cr.g, cr.b, cr.a);
        glVertex2f(rect.x1, rect.y1);
        glVertex2f(rect.x1, rect.y0);
    }
    glEnd();
}

struct Render_Quad{
    f32 x0, y0, x1, y1;
    f32 s0, t0, s1, t1;
};

inline Render_Quad
get_render_quad(Glyph_Data *b, int pw, int ph, float xpos, float ypos){
    Render_Quad q;
    
    float ipw = 1.0f / pw, iph = 1.0f / ph;
    
    q.x0 = xpos + b->xoff;
    q.y0 = ypos + b->yoff;
    q.x1 = xpos + b->xoff2;
    q.y1 = ypos + b->yoff2;
    
    q.s0 = b->x0 * ipw;
    q.t0 = b->y0 * iph;
    q.s1 = b->x1 * ipw;
    q.t1 = b->y1 * iph;
    
    return(q);
}

inline Render_Quad
get_exact_render_quad(Glyph_Data *b, int pw, int ph, float xpos, float ypos){
    Render_Quad q;
    
    float ipw = 1.0f / pw, iph = 1.0f / ph;
    
    q.x0 = xpos;
    q.y0 = ypos + b->yoff;
    q.x1 = xpos + (b->xoff2 - b->xoff);
    q.y1 = ypos + b->yoff2;
    
    q.s0 = b->x0 * ipw;
    q.t0 = b->y0 * iph;
    q.s1 = b->x1 * ipw;
    q.t1 = b->y1 * iph;
    
    return(q);
}

inline void
private_draw_glyph(Render_Target *target, Render_Font *font,
                   u8 character, f32 x, f32 y, u32 color){
    Render_Quad q = get_render_quad(
        font->glyphs + character,
        font->tex_width, font->tex_height, x, y
        );
    
    draw_set_color(target, color);
    draw_bind_texture(target, font->tex);
    glBegin(GL_QUADS);
    {
        glTexCoord2f(q.s0, q.t1); glVertex2f(q.x0, q.y1);
        glTexCoord2f(q.s1, q.t1); glVertex2f(q.x1, q.y1);
        glTexCoord2f(q.s1, q.t0); glVertex2f(q.x1, q.y0);
        glTexCoord2f(q.s0, q.t0); glVertex2f(q.x0, q.y0);
    }
    glEnd();
}

inline void
private_draw_glyph_mono(Render_Target *target, Render_Font *font, u8 character,
                        f32 x, f32 y, f32 advance, u32 color){
    
    f32 left = font->glyphs[character].x0;
    f32 right = font->glyphs[character].x1;
    f32 width = (right - left);
    f32 x_shift = (advance - width) * .5f;
    
    x += x_shift;
    
    Render_Quad q = get_exact_render_quad(
        font->glyphs + character,
        font->tex_width, font->tex_height, x, y
        );
    
    draw_set_color(target, color);
    draw_bind_texture(target, font->tex);
    glBegin(GL_QUADS);
    {
        glTexCoord2f(q.s0, q.t1); glVertex2f(q.x0, q.y1);
        glTexCoord2f(q.s1, q.t1); glVertex2f(q.x1, q.y1);
        glTexCoord2f(q.s1, q.t0); glVertex2f(q.x1, q.y0);
        glTexCoord2f(q.s0, q.t0); glVertex2f(q.x0, q.y0);
    }
    glEnd();
}

inline void
private_draw_glyph_mono(Render_Target *target, Render_Font *font, u8 character,
                        real32 x, real32 y, u32 color){
    private_draw_glyph_mono(target, font, character, x, y, (f32)font->advance, color);
}

internal void
launch_rendering(Render_Target *target){
    byte *cursor = target->push_buffer;
    byte *cursor_end = cursor + target->size;
    
    for (; cursor < cursor_end;){
        Render_Piece_Header *header = ExtractStruct(Render_Piece_Header);
        
        i32 type = header->type;
        switch (type){
            case piece_type_rectangle:
            {
                Render_Piece_Rectangle *rectangle =
                    ExtractStruct(Render_Piece_Rectangle);
                private_draw_rectangle(target, rectangle->rect, rectangle->color);
            }break;
            
            case piece_type_outline:
            {
                Render_Piece_Rectangle *rectangle =
                    ExtractStruct(Render_Piece_Rectangle);
                private_draw_rectangle_outline(target, rectangle->rect, rectangle->color);
            }break;
            
            case piece_type_gradient:
            {
                Render_Piece_Gradient *gradient =
                    ExtractStruct(Render_Piece_Gradient);
                private_draw_gradient(target, gradient->rect,
                                      unpack_color4(gradient->left_color),
                                      unpack_color4(gradient->right_color));
            }break;
            
            case piece_type_glyph:
            {
                Render_Piece_Glyph *glyph =
                    ExtractStruct(Render_Piece_Glyph);
                
                Render_Font *font = get_font_info(&target->font_set, glyph->font_id)->font;
                if (font)
                    private_draw_glyph(target, font, glyph->character,
                                       glyph->pos.x, glyph->pos.y, glyph->color);
            }break;
            
            case piece_type_mono_glyph:
            {
                Render_Piece_Glyph *glyph =
                    ExtractStruct(Render_Piece_Glyph);
                
                Render_Font *font = get_font_info(&target->font_set, glyph->font_id)->font;
                if (font)            
                    private_draw_glyph_mono(target, font, glyph->character,
                                            glyph->pos.x, glyph->pos.y, glyph->color);
            }break;
            
            case piece_type_mono_glyph_advance:
            {
                Render_Piece_Glyph_Advance *glyph =
                    ExtractStruct(Render_Piece_Glyph_Advance);
                
                Render_Font *font = get_font_info(&target->font_set, glyph->font_id)->font;
                if (font)
                    private_draw_glyph_mono(target, font, glyph->character,
                                            glyph->pos.x, glyph->pos.y,
                                            glyph->advance, glyph->color);
            }break;
            
            case piece_type_change_clip:
            {
                Render_Piece_Change_Clip *clip =
                    ExtractStruct(Render_Piece_Change_Clip);
                draw_set_clip(target, clip->box);
            }break;
        }
    }
}

#undef ExtractStruct

internal void*
part_alloc(int size, void *context){
    Partition *part = (Partition*)context;
    void *result = push_block(part, size);
    return(result);
}

internal void
part_free(void *ptr, void *context){
}

#define STBTT_malloc part_alloc
#define STBTT_free part_free

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

internal i32
font_load_stb(Partition *part,
              Render_Font *font_out,
              char *filename_untranslated,
              i32 pt_size,
              i32 tab_width,
              i32 oversample,
              b32 store_texture){
    
    char space_[1024];
    String filename = make_fixed_width_string(space_);
    b32 translate_success = sysshared_to_binary_path(&filename, filename_untranslated);
    if (!translate_success) return 0;
    
    i32 result = 1;
    
    stbtt_packedchar chardata[256];
    
    File_Data file = sysshared_load_file(filename.str);
    
    if (!file.data.data){
        result = 0;
    }
    
    else{
        stbtt_fontinfo font;
        if (!stbtt_InitFont(&font, (u8*)file.data.data, 0)){
            result = 0;
        }
        else{
            memset(font_out, 0, sizeof(*font_out));
            
            i32 ascent, descent, line_gap;
            stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
            
            f32 scale = stbtt_ScaleForPixelHeight(&font, (f32)pt_size);
            
            f32 scaled_ascent = scale*ascent;
            f32 scaled_descent = scale*descent;
            f32 scaled_line_gap = scale*line_gap;
            
            font_out->height = (i32)(scaled_ascent - scaled_descent + scaled_line_gap);
            font_out->ascent = (i32)(scaled_ascent);
            font_out->descent = (i32)(scaled_descent);
            font_out->line_skip = (i32)(scaled_line_gap);
            
            if (store_texture){
                Temp_Memory temp = begin_temp_memory(part);
                
                i32 tex_width = pt_size*16*oversample;
                i32 tex_height = pt_size*16*oversample;
                void *block = sysshared_push_block(part, tex_width * tex_height);
                
                font_out->tex_width = tex_width;
                font_out->tex_height = tex_height;
                
                /////////////////////////////////////////////////////////////////
                stbtt_pack_context spc;
                
                if (stbtt_PackBegin(&spc, (u8*)block, tex_width, tex_height,
                                    tex_width, 1, part)){
                    stbtt_PackSetOversampling(&spc, oversample, oversample);
                    if (!stbtt_PackFontRange(&spc, (u8*)file.data.data, 0,
                                             STBTT_POINT_SIZE((f32)pt_size),
                                             0, 128, chardata)){
                        result = 0;
                    }
                    
                    stbtt_PackEnd(&spc);
                }
                else{
                    result = 0;
                }
                /////////////////////////////////////////////////////////////////
                
                if (result){
                    GLuint font_tex;
                    glGenTextures(1, &font_tex);
                    glBindTexture(GL_TEXTURE_2D, font_tex);
                    
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tex_width, tex_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, block);
                    
                    font_out->tex = font_tex;
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    f32 *advance_data = font_out->advance_data;
                    Glyph_Data *glyphs = font_out->glyphs;
                    for (u8 code_point = 0; code_point < 128; ++code_point){
                        advance_data[code_point]   = (f32)(CEIL32(chardata[code_point].xadvance));
                        glyphs[code_point].x0      = chardata[code_point].x0;
                        glyphs[code_point].y0      = chardata[code_point].y0;
                        glyphs[code_point].x1      = chardata[code_point].x1;
                        glyphs[code_point].y1      = chardata[code_point].y1;
                        glyphs[code_point].xoff    = chardata[code_point].xoff;
                        glyphs[code_point].yoff    = chardata[code_point].yoff  + font_out->ascent;
                        glyphs[code_point].xoff2   = chardata[code_point].xoff2;
                        glyphs[code_point].yoff2   = chardata[code_point].yoff2 + font_out->ascent;
                    }
                    
                    glyphs['\r'] = glyphs[' '];
                    advance_data['\r'] = advance_data[' '];
                    
                    glyphs['\n'] = glyphs[' '];
                    advance_data['\n'] = advance_data[' '];
                    
                    glyphs['\t'] = glyphs[' '];
                    advance_data['\t'] = advance_data[' ']*tab_width;
                    
                    i32 max_advance = 0;
                    for (u8 code_point = 0; code_point < 128; ++code_point){
                        if (stbtt_FindGlyphIndex(&font, code_point) != 0){
                            font_out->glyphs[code_point].exists = 1;
                            i32 advance = CEIL32(advance_data[code_point]);
                            if (max_advance < advance){
                                max_advance = advance;
                            }
                        }
                    }
                    font_out->advance = max_advance - 1;
                }
                
                end_temp_memory(temp);
            }
        }
        system_free_memory(file.data.data);
    }
    
    return(result);
}

// NOTE(allen): Thanks to insofaras.
// This is copy-pasted from some work he
// did to get free type working on linux.
// Once it is working on both sides it might
// be possible to pull some parts out as
// portable FT rendering.

#undef internal
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H
#define internal static

internal u32
next_pow_of_2(u32 v){
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return ++v;
}

#define NUM_GLYPHS 128
#define ENABLE_LCD_FILTER 0

internal b32
font_load_freetype(Partition *part,
                   Render_Font *rf,
                   char *filename,
                   i32 pt_size,
                   i32 tab_width){
    
    memset(rf, 0, sizeof(*rf));
    
    //TODO(inso): put stuff in linuxvars / init in main
    FT_Library ft;
    FT_Face face;
    b32 use_lcd_filter = 0;
    
    FT_Init_FreeType(&ft);
    
    //NOTE(inso): i'm not sure the LCD filter looks better, and it doesn't work perfectly with the coloring stuff
    // it will probably need shaders to work properly
#if ENABLE_LCD_FILTER
    if(FT_Library_SetLcdFilter(ft, FT_LCD_FILTER_DEFAULT) == 0){
        puts("LCD Filter on");
        use_lcd_filter = 1;
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
#endif
    
    FT_New_Face(ft, filename, 0, &face);
    
    // set size & metrics
    FT_Size_RequestRec_ size = {};
    size.type   = FT_SIZE_REQUEST_TYPE_NOMINAL;
    size.height = pt_size << 6;
    FT_Request_Size(face, &size);
    
    rf->loaded    = 1;
    rf->ascent    = CEIL32  (face->size->metrics.ascender    / 64.0f);
    rf->descent   = FLOOR32 (face->size->metrics.descender   / 64.0f);
    rf->advance   = CEIL32  (face->size->metrics.max_advance / 64.0f);
    rf->height    = CEIL32  (face->size->metrics.height      / 64.0f);
    rf->line_skip = rf->height - (rf->ascent - rf->descent);
    
    rf->height -= rf->line_skip;
    rf->line_skip = 0;
    
    int max_glyph_w = face->size->metrics.x_ppem;
    int max_glyph_h = rf->height;
    int tex_width   = 64;
    int tex_height  = 0;
    
    // estimate upper bound on texture width
    do {
        tex_width *= 2;
        float glyphs_per_row = ceilf(tex_width / (float) max_glyph_w);
        float rows = ceilf(NUM_GLYPHS / glyphs_per_row);
        tex_height = CEIL32(rows * (max_glyph_h + 2));
    } while(tex_height > tex_width);
    
    tex_height = next_pow_of_2(tex_height);
    
    int pen_x = 0;
    int pen_y = 0;
    
    u32* pixels = push_array(part, u32, tex_width * tex_height);
    memset(pixels, 0, tex_width * tex_height * sizeof(u32));
    
    // XXX: test if AUTOHINT looks better or not
    // NOTE(allen): As of now FT_LOAD_FORCE_AUTOHINT looks much better for
    // Liberation Mono which is one of the included 4coder fonts.
    const u32 ft_extra_flags = use_lcd_filter ? FT_LOAD_TARGET_LCD : FT_LOAD_FORCE_AUTOHINT;
    
    for(int i = 0; i < NUM_GLYPHS; ++i){
        if(FT_Load_Char(face, i, FT_LOAD_RENDER | ft_extra_flags) != 0) continue;
        
        int w = face->glyph->bitmap.width;
        int h = face->glyph->bitmap.rows;
        
        // lcd filter produces RGB bitmaps, need to account for the extra components
        if(use_lcd_filter){
            w /= 3;
        }
        
        // move to next line if necessary
        if(pen_x + w >= tex_width){
            pen_x = 0;
            pen_y += (max_glyph_h + 2);
        }
        
        // set all this stuff the renderer needs
        Glyph_Data* c = rf->glyphs + i;
        
        c->x0 = (f32)(pen_x);
        c->y0 = (f32)(pen_y);
        c->x1 = (f32)(pen_x + w);
        c->y1 = (f32)(pen_y + h + 1);
        
        c->xoff = (f32)(face->glyph->bitmap_left);
        c->yoff = (f32)(rf->ascent - face->glyph->bitmap_top);
        
        c->xoff2 = w + c->xoff;
        c->yoff2 = h + c->yoff + 1;
        
        rf->advance_data[i] = (f32)(face->glyph->advance.x >> 6);
        
        rf->glyphs[i].exists = 1;
        
        
        int pitch = face->glyph->bitmap.pitch;
        
        // write to texture atlas
        for(int j = 0; j < h; ++j){
            for(int i = 0; i < w; ++i){
                int x = pen_x + i;
                int y = pen_y + j;
                
                if(use_lcd_filter){
#if 1
                    u8 a = face->glyph->bitmap.buffer[j * pitch + i * 3 + 1];
                    u8 r = face->glyph->bitmap.buffer[j * pitch + i * 3 + 0];
                    u8 b = face->glyph->bitmap.buffer[j * pitch + i * 3 + 2];
                    
                    pixels[y * tex_width + x] = (a << 24) | (b << 16) | (a << 8) | r;
                    
#else
                    
                    u8 r = face->glyph->bitmap.buffer[j * pitch + i * 3];
                    u8 g = face->glyph->bitmap.buffer[j * pitch + i * 3 + 1];
                    u8 b = face->glyph->bitmap.buffer[j * pitch + i * 3 + 2];
                    u8 a = (u8)ROUND32((r + g + b) / 3.0f);
                    
                    pixels[y * tex_width + x] = (a << 24) | (r << 16) | (g << 8) | b;
#endif
                } else {
                    pixels[y * tex_width + x] = face->glyph->bitmap.buffer[j * pitch + i] * 0x1010101;
                }
            }
        }
        
        pen_x = CEIL32(c->x1 + 1);
    }
    
    Glyph_Data space_glyph = rf->glyphs[' '];
    f32 space_width = rf->advance_data[' '];
    
    rf->glyphs['\r'] = space_glyph;
    rf->advance_data['\r'] = space_width*tab_width;
    
    rf->glyphs['\n'] = space_glyph;
    rf->advance_data['\n'] = space_width*tab_width;
    
    rf->glyphs['\t'] = space_glyph;
    rf->advance_data['\t'] = space_width*tab_width;
    
    FT_Done_FreeType(ft);
    
    tex_height = next_pow_of_2(pen_y + max_glyph_h + 2);
    
    rf->tex_width  = tex_width;
    rf->tex_height = tex_height;
    
    // upload texture
    glGenTextures(1, &rf->tex);
    glBindTexture(GL_TEXTURE_2D, rf->tex);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    
    if(use_lcd_filter){
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tex_width, tex_height, 0, GL_ALPHA, GL_UNSIGNED_INT, pixels);
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return 1;
}

internal
Release_Font_Sig(draw_release_font){
    glDeleteTextures(1, &font->tex);
    font->tex = 0;
}

// BOTTOM

