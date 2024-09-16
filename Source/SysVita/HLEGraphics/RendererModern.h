
#ifndef SYSVITA_HLEGRAPHICS_RENDERERMODERN_H_
#define SYSVITA_HLEGRAPHICS_RENDERERMODERN_H_

#include <map>
#include <set>

#include "HLEGraphics/BaseRenderer.h"

class RendererModern : public BaseRenderer
{
public:
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
	
private:
	void 				MakeShaderConfigFromCurrentState(struct ShaderConfiguration * config) const;
	float 				CalculateAlphaThreshold() const;

	void 				PrepareRenderState(const float (&mat_project)[16], bool disable_zbuffer);

	void 				RenderDaedalusVtx(int prim, const DaedalusVtx * vertices, int count);
	void 				RenderDaedalusVtxStreams(int prim, const float * positions, const float * uvs, const u32 * colours, int count);
};

// NB: this is equivalent to gRenderer, but points to the implementation class, for platform-specific functionality.
extern RendererModern * gRendererModern;

#endif // SYSVITA_HLEGRAPHICS_RENDERERMODERN_H_
