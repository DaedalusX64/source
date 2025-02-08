#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL.h>

// XXX Temporary
#ifdef DAEDALUS_PSP
 SDL_Renderer * gSdlRenderer;
#else
#include "SysGLES/GL.h"
#endif

#include "UI/DrawText.h"
#include "Utility/Translate.h"




static float scaleX = 1.0f, scaleY = 1.0f;

TTF_Font *gFonts[] =
{
		nullptr,
		nullptr,
};
DAEDALUS_STATIC_ASSERT(std::size(gFonts) == CDrawText::NUM_FONTS);

void CDrawText::Initialise()
{
    #ifdef DAEDALUS_PSP
        // I dunno, maybe 8?
        gFonts[CDrawText::F_REGULAR] = TTF_OpenFont("Resources/OpenSans.ttf", 8);
        gFonts[CDrawText::F_LARGE_BOLD] = TTF_OpenFont("Resources/OpenSans.ttf", 8);
     #else
        gFonts[CDrawText::F_REGULAR] = TTF_OpenFont("Resources/OpenSans.ttf", 48);
        gFonts[CDrawText::F_LARGE_BOLD] = TTF_OpenFont("Resources/OpenSans.ttf", 48);
    #endif
    TTF_SetFontStyle(gFonts[CDrawText::F_LARGE_BOLD], TTF_STYLE_BOLD);

    if (gFonts[0] == 0)
    {
		printf( "SDL could not open TTF Font! SDL Error: %s\n", SDL_GetError() );
	}

}

void CDrawText::Destroy()
{
    TTF_CloseFont(gFonts[CDrawText::F_REGULAR]);
    TTF_CloseFont(gFonts[CDrawText::F_LARGE_BOLD]);
}


u32 CDrawText::Render(EFont font, s32 x, s32 y, float scale, const std::string p_str, u32 length, c32 colour)
{
	return Render(font, x, y, scale, p_str, length, colour, c32(0, 0, 0, 160));
}


u32 CDrawText::Render(EFont font_type, s32 x, s32 y, float scale, const std::string p_str, u32 length, c32 colour, c32 drop_colour)
{
    DAEDALUS_ASSERT(font_type >= 0 && font_type < (s32)NUM_FONTS, "Invalid font");

    // Get the translated string and its length
    const char* translated_str = Translate_Strings(p_str, length);

    TTF_Font *font = gFonts[font_type];
    if (font && translated_str[0] != '\0')
    {
        SDL_Color c {colour.GetR(), colour.GetG(), colour.GetB()};
        SDL_Color dc {drop_colour.GetR(), drop_colour.GetG(), drop_colour.GetB()}; // Unused currently
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font, translated_str, c);

        if (!surface)
        {
            SDL_Log("Unable to render text surface: %s", SDL_GetError());
            return 0; // Return 0 to indicate failure
        }

        SDL_Texture* Message = SDL_CreateTextureFromSurface(gSdlRenderer, surface);
        if (!Message)
        {
            SDL_Log("Unable to create texture from surface: %s", SDL_GetError());
            SDL_FreeSurface(surface);
            return 0; // Return 0 to indicate failure
        }

        SDL_Rect Message_rect;
        Message_rect.x = x * scaleX; 
        Message_rect.y = y * scaleY;
        Message_rect.w = surface->w * scaleX * scale;
        Message_rect.h = surface->h * scaleY * scale;

        SDL_RenderCopy(gSdlRenderer, Message, nullptr, &Message_rect);

        SDL_FreeSurface(surface);
        SDL_DestroyTexture(Message);

        return Message_rect.w;
    }

    return length * 16; // Return based on original length as a fallback
}


s32 CDrawText::GetTextWidth(EFont font_type, const std::string p_str, u32 length)
{
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT(font_type >= 0 && font_type < (s32)NUM_FONTS, "Invalid font");
#endif
	TTF_Font *font = gFonts[font_type];
	if (font)
	{
        int w, h;
        std::string substr = (length >= p_str.size()) ? p_str : p_str.substr(0, length);
        std::string_view str_view = substr;
        TTF_SizeText(font, str_view.data(), &w, &h);
        return static_cast<s32>(Translate_Strings(p_str, length), w);
    }

	return 0;
}



s32 CDrawText::GetFontHeight(EFont font_type)
{
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT(font_type >= 0 && font_type < (s32)NUM_FONTS, "Invalid font");
#endif
	TTF_Font *font(gFonts[font_type]);
	if (font)
	{
        return TTF_FontAscent(font);
    }

	return 0; // Return a reasonable value. Better off just returning 0?
}

void DrawText_SetScale(float X, float Y)
{
    //TODO: we can scale the font size?
    scaleX = X;
    scaleY = Y;
}