
#ifndef SYSVITA_HLEGRAPHICS_RENDERERLEGACY_H_
#define SYSVITA_HLEGRAPHICS_RENDERERLEGACY_H_

#include <map>
#include <set>

#include "HLEGraphics/BaseRenderer.h"
#include "SysVita/HLEGraphics/BlendModes.h"

class CBlendStates;

struct DebugBlendSettings
{
	u32 TexInstall;	//defaults to texture installed
	u32	SetRGB;		//defaults to OFF
	u32	SetA;		//defaults to OFF
	u32	SetRGBA;	//defaults to OFF
	u32	ModRGB;		//defaults to OFF
	u32	ModA;		//defaults to OFF
	u32	ModRGBA;	//defaults to OFF
	u32	SubRGB;		//defaults to OFF
	u32	SubA;		//defaults to OFF
	u32	SubRGBA;	//defaults to OFF
	u32	AOpaque;	//defaults to OFF
	u32	sceENV;		//defaults to OFF
	u32	TXTFUNC;	//defaults to MODULATE_RGB
	u32 ForceRGB;	//defaults to OFF
};

class RendererLegacy : public BaseRenderer
{
public:
	RendererLegacy();
	~RendererLegacy();

	virtual void		RestoreRenderStates();

	virtual void		RenderTriangles(uint32_t *colors, u32 num_vertices, bool disable_zbuffer);

	virtual void		TexRect(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1);
	virtual void		TexRectFlip(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1);
	virtual void		FillRect(const v2 & xy0, const v2 & xy1, u32 color);

	virtual void		Draw2DTexture(f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1);
	virtual void		Draw2DTextureR(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, f32 s, f32 t);
	
	virtual uint32_t	PrepareTrisUnclipped(uint32_t **clr);
	
	virtual void		DoGamma(float gamma);
	virtual void		DrawUITexture();
	
	struct SBlendStateEntry
	{
		SBlendStateEntry() : OverrideFunction( NULL ), States( NULL ) {}
		OverrideBlendModeFn			OverrideFunction;
		const CBlendStates *		States;
	};

	SBlendStateEntry	LookupBlendState( u64 mux, bool two_cycles );

private:
	void				RenderUsingCurrentBlendMode(const float (&mat_project)[16], u32 * p_vertices, u32 num_vertices, u32 triangle_mode, bool disable_zbuffer, bool is_3d);
	void				RenderUsingRenderSettings(const CBlendStates * states, u32 * p_vertices, u32 num_vertices, u32 triangle_mode, bool is_3d);
	
	// Temporary vertex storage
	u32			mVtx_Save[320];
	
	// BlendMode support
	//
	CBlendStates *		mCopyBlendStates;
	CBlendStates *		mFillBlendStates;

	typedef std::map< u64, SBlendStateEntry > BlendStatesMap;
	BlendStatesMap		mBlendStatesMap;
};

// NB: this is equivalent to gRenderer, but points to the implementation class, for platform-specific functionality.
extern RendererLegacy * gRendererLegacy;

#endif // SYSVITA_HLEGRAPHICS_RENDERERLEGACY_H_
