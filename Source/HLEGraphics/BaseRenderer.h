/*
Copyright (C) 2001 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef HLEGRAPHICS_BASERENDERER_H_
#define HLEGRAPHICS_BASERENDERER_H_

#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix4x4.h"

#include "HLEGraphics/DaedalusVtx.h"
#include "HLEGraphics/TextureInfo.h"
#include "Graphics/ColourValue.h"
#include "Interface/Preferences.h"
#include <array>

#ifdef DAEDALUS_PSP
#include <pspgu.h>
#elif defined(DAEDALUS_CTR)
#include <GL/picaGL.h>
#elif defined(DAEDALUS_GLES)
#include "SysGLES/GL.h"
#else
#include "SysGL/GL.h"
#endif

#ifdef DAEDALUS_CTR
#define HD_SCALE 0.8f
#else
#define HD_SCALE 0.754166f
#endif

class CNativeTexture;
struct TempVerts;

// FIXME - this is for the PSP only.
struct TextureVtx
{
	v2  t0;
	v3  pos;
};

struct TriDKR
{
    u8	v2, v1, v0, flag;
    s16	t0, s0;
    s16	t1, s1;
    s16	t2, s2;
};
DAEDALUS_STATIC_ASSERT( sizeof(TriDKR) == 16 );

//Can't be used for DKR since pointer start as odd and even addresses :( //Corn
struct FiddledVtxDKR
{
	s16 y;
	s16 x;

	u8 a;
	u8 b;
	s16 z;

	u8 g;
	u8 r;
};
DAEDALUS_STATIC_ASSERT( sizeof(FiddledVtxDKR) == 10 );

struct FiddledVtxPD
{
	s16 y;
	s16	x;

	u8	cidx;
	u8	pad;
	s16 z;

	s16 tv;
	s16 tu;
};
DAEDALUS_STATIC_ASSERT( sizeof(FiddledVtxPD) == 12 );

struct FiddledVtx
{
        s16 y;
        s16 x;

        union
        {
			s16 flag;
            struct
            {
				s8 normz;
				u8 pad;
            };
        };
        s16 z;

        s16 tv;
        s16 tu;

        union
        {
            struct
            {
                    u8 rgba_a;
                    u8 rgba_b;
                    u8 rgba_g;
                    u8 rgba_r;
            };
            struct
            {
                    s8 norm_a;
                    s8 norm_z;      // b
                    s8 norm_y;      // g
                    s8 norm_x;      // r
            };
        };
};
DAEDALUS_STATIC_ASSERT( sizeof(FiddledVtx) == 16 );

struct alignas(DATA_ALIGN) DaedalusLight
{
	v3		Direction;		// w component is ignored. Should be normalised
	u32		SkipIfZero;		// Used by CBFD & MM
	v3		Colour;			// Colour, components in range 0..1
	f32		Iscale;			// Used by CBFD
	v4		Position;		// Position -32768 to 32767
	f32		ca;				// Used by MM(GBI2 point light)
	f32		la;				// Used by MM(GBI2 point light)
	f32		qa;				// Used by MM(GBI2 point light)
	u32		Pad0;			// Padding
};
DAEDALUS_STATIC_ASSERT( sizeof( DaedalusLight ) == 64 );	//Size=64 bytes and order is important or VFPU ASM for PSP will fail

// Order here should be the same as in TnLMode
enum ETnLModeFlags
{
	TNL_LIGHT		= 1 << 0,
	TNL_TEXGEN		= 1 << 1,
	TNL_TEXGENLIN	= 1 << 2,
	TNL_FOG			= 1 << 3,
	TNL_SHADE		= 1 << 4,
	TNL_ZBUFFER		= 1 << 5,
	TNL_TRICULL		= 1 << 6,
	TNL_CULLBACK	= 1 << 7,
	TNL_POINTLIGHT	= 1 << 8,
};

struct TnLMode
{
	union
	{
		struct
		{
			u32 Light : 1;			// 0x1
			u32 TexGen : 1;			// 0x2
			u32 TexGenLin : 1;		// 0x4
			u32 Fog : 1;			// 0x8
			u32 Shade : 1;			// 0x10
			u32 Zbuffer : 1;		// 0x20
			u32 TriCull : 1;		// 0x40
			u32 CullBack : 1;		// 0x80
			u32 PointLight : 1;		// 0x100
			u32 pad0 : 23;			// 0x0
		};

		struct
		{
			u16 Modes;
			u16 Texture;
		};

		u32	_u32;
	};
};

struct alignas(DATA_ALIGN) TnLParams
// ALIGNED_TYPE(struct, TnLParams, 16)
{
	TnLMode			Flags;			//TnL flags
	u32				NumLights;		//Number of lights
	f32				TextureScaleX;	//Texture scale X
	f32				TextureScaleY;	//Texture scale Y
	DaedalusLight	Lights[12];		//Conker uses up to 12 lights
	f32				CoordMod[16];	//Used by CBFD lights
	f32				FogMult;		//Fog mult
	f32				FogOffs;		//Fog offset
};
//DAEDALUS_STATIC_ASSERT( sizeof( TnLParams ) == 32 );

// Bits for clipping
// 543210
// +++---
// zyxzyx
// NB: These are ordered such that the VFPU can generate them easily - make sure you keep the VFPU code up to date if changing these.
#define X_NEG  0x01	//left
#define Y_NEG  0x02	//bottom
#define Z_NEG  0x04	//far
#define X_POS  0x08	//right
#define Y_POS  0x10	//top
#define Z_POS  0x20	//near

// Test all, including Z_NEG (far plane)? (TODO: Check No Near Plane microcodes)
static const u32 CLIP_TEST_FLAGS = ( X_POS | X_NEG | Y_POS | Y_NEG | Z_POS | Z_NEG );


enum CycleType
{
	CYCLE_1CYCLE = 0,		// Please keep in this order - matches RDP
	CYCLE_2CYCLE,
	CYCLE_COPY,
	CYCLE_FILL,
};

static const u32 kMaxN64Vertices = 80;		// F3DLP.Rej supports up to 80 verts!
//*****************************************************************************
//
//*****************************************************************************
class BaseRenderer
{
public:
	BaseRenderer();
	virtual ~BaseRenderer();

	void				BeginScene();
	void				EndScene();

	void				SetVIScales();
	void				Reset();

	// Various rendering states
	// Don't think we need to updateshademodel, it breaks tiger's honey hunt
#ifdef DAEDALUS_PSP
	inline void			SetTnLMode(u32 mode)					{ mTnL.Flags.Modes = mode; /*UpdateFogEnable(); UpdateShadeModel();*/ }
#else
	inline void			SetTnLMode(u32 mode)					{ mTnL.Flags.Modes = mode; UpdateFogEnable(); /*UpdateShadeModel();*/ }
#endif
	// Texture stuff
	inline void			SetTextureEnable(bool enable)			{ mTnL.Flags.Texture = enable; }
	inline void			SetTextureTile(u32 tile)				{ mTextureTile = tile; }
	inline void			SetTextureScale(float fScaleX, float fScaleY)	
	{ 
		mTnL.TextureScaleX = fScaleX == 0 ? 1/32.0f : fScaleX;
		mTnL.TextureScaleY = fScaleY == 0 ? 1/32.0f : fScaleY; 
	}
	inline u32			GetTextureTile() const					{ return mTextureTile; }

#ifdef DAEDALUS_CTR
	inline void			SetCullMode(bool enable, bool mode)		{ enable ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE); mode ? glCullFace(GL_BACK) : glCullFace(GL_FRONT); }
#else
	inline void			SetCullMode(bool enable, bool mode)		{ mTnL.Flags.TriCull = enable; mTnL.Flags.CullBack = mode; }
#endif

	// Fog stuff
	inline void			SetFogMultOffs(f32 Mult, f32 Offs)		{ mTnL.FogMult=Mult/255.0f; mTnL.FogOffs=Offs/255.0f;}
#ifdef DAEDALUS_PSP
	inline void			SetFogMinMax(f32 fog_near, f32 fog_far)	{ mfog_near = fog_near; mfog_far = fog_far;}
	inline void			SetFogColour( c32 colour )				{ mFogColour = colour; }
#elif defined(DAEDALUS_VITA) || defined (DAEDALUS_CTR) || defined (DAEDALUS_GL) || defined (DAEDALUS_GLES)
	inline void			SetFogMinMax(f32 fog_near, f32 fog_far)	{ glFogf(GL_FOG_START, fog_near); glFogf(GL_FOG_END, fog_far); }
	inline void			SetFogColour( c32 colour )				{ float fog_clr[4] = {colour.GetRf(), colour.GetGf(), colour.GetBf(), colour.GetAf()}; glFogfv(GL_FOG_COLOR, &fog_clr[0]); }
#endif


	// PrimDepth will replace the z value if depth_source=1 (z range 32767-0 while PSP depthbuffer range 0-65535)//Corn
#ifdef DAEDALUS_PSP
	inline void			SetPrimitiveDepth( u32 z )				{ mPrimDepth = (f32)( ( ( 32767 - z ) << 1) + 1 ); }
#else
	inline void			SetPrimitiveDepth( u32 z )				{ mPrimDepth = (f32)(z - 0x4000) / (f32)0x4000;}
#endif
	inline void			SetPrimitiveLODFraction( f32 f )		{ mPrimLODFraction = f; }
	inline void			SetPrimitiveColour( c32 colour )		{ mPrimitiveColour = colour; }
	inline void			SetEnvColour( c32 colour )				{ mEnvColour = colour; }
	inline void			SetBlendColour( c32 colour )			{ mBlendColour = colour; }
	inline void			SetFillColour( u32 colour )				{ mFillColour = colour; }

	inline void			SetNumLights(u32 num)					{ mTnL.NumLights = num; }
	inline void			SetLightCol(u32 l, u8 r, u8 g, u8 b)	{ mTnL.Lights[l].SkipIfZero=(r+g+b); mTnL.Lights[l].Colour.x= r/255.0f; mTnL.Lights[l].Colour.y= g/255.0f; mTnL.Lights[l].Colour.z= b/255.0f; }
	inline void			SetLightDirection(u32 l, f32 x, f32 y, f32 z) { v3 n(x, y, z); n.Normalise(); mTnL.Lights[l].Direction.x=n.x; mTnL.Lights[l].Direction.y=n.y; mTnL.Lights[l].Direction.z=n.z; }
	inline void			SetLightPosition(u32 l, f32 x, f32 y, f32 z, f32 w) { mTnL.Lights[l].Position.x=x; mTnL.Lights[l].Position.y=y; mTnL.Lights[l].Position.z=z; mTnL.Lights[l].Position.w=w; }
	inline void			SetLightCBFD(u32 l, u8 nonzero)			{ mTnL.Lights[l].Iscale=(f32)(nonzero << 12); mTnL.Lights[l].SkipIfZero = mTnL.Lights[l].SkipIfZero&&nonzero; }
	inline void			SetLightEx(u32 l, f32 ca, f32 la, f32 qa) { mTnL.Lights[l].ca=ca/16.0f; mTnL.Lights[l].la=la/65535.0f; mTnL.Lights[l].qa=qa/(8.0f*65535.0f); }

	inline f32			GetCoordMod( u32 idx )					{ return mTnL.CoordMod[idx]; }
	inline void			SetCoordMod( u32 idx, f32 mod )			{ mTnL.CoordMod[idx] = mod; }
	inline void			SetMux( u64 mux )						{ mMux = mux; }

	// TextRect stuff
	virtual void		TexRect( u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1 ) = 0;
	virtual void		TexRectFlip( u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1 ) = 0;
	virtual void		FillRect( const v2 & xy0, const v2 & xy1, u32 color ) = 0;

	// Texture stuff
	virtual void		Draw2DTexture(f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, const std::shared_ptr<CNativeTexture> texture) = 0;
	virtual void		Draw2DTextureR(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, f32 s, f32 t, const std::shared_ptr <CNativeTexture> texture) = 0;

	// Viewport stuff
	void				SetN64Viewport( const v2 & scale, const v2 & trans );
	void				SetScissor( u32 x0, u32 y0, u32 x1, u32 y1 );

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	void				PrintActive();
#endif
	void				ResetMatrices(u32 size);
	void				SetDKRMat(const u32 address, bool mul, u32 idx);
	void				SetProjection(const u32 address, bool bReplace);
	void				SetWorldView(const u32 address, bool bPush, bool bReplace);
	//inline void			PopProjection() {if (mProjectionTop > 0) --mProjectionTop;	mWorldProjectValid = false;}
	inline void			PopWorldView(u32 num = 1)	{if (mModelViewTop > (num-1))	 mModelViewTop-=num;	mWorldProjectValid = false;}
	void				InsertMatrix(u32 w0, u32 w1);
	void				ForceMatrix(const u32 address);
	inline void			DKRMtxChanged( u32 idx )	{mWPmodified = true; mDKRMatIdx = idx;}

	// Vertex stuff
	void				SetNewVertexInfo(u32 address, u32 v0, u32 n);	// Assumes dwAddress has already been checked!
	void				SetNewVertexInfoConker(u32 address, u32 v0, u32 n);	// For conker..
	void				SetNewVertexInfoDKR(u32 address, u32 v0, u32 n, bool billboard);	// Assumes dwAddress has already been checked!
	void				SetNewVertexInfoPD(u32 address, u32 v0, u32 n);	// Assumes dwAddress has already been checked!
	void				ModifyVertexInfo(u32 whered, u32 vert, u32 val);
	void				SetVtxColor( u32 vert, u32 color );
	inline void			SetVtxTextureCoord( u32 vert, s16 tu, s16 tv ) { mVtxProjected[vert].Texture.x = (f32)tu * (1.0f / 32.0f); mVtxProjected[vert].Texture.y = (f32)tv * (1.0f / 32.0f); }
	inline void			SetVtxXY( u32 vert, float x, float y );
	void				SetVtxZ( u32 vert, float z );
	inline void			CopyVtx( u32 vert_src, u32 vert_dst ) { mVtxProjected[vert_dst] = mVtxProjected[vert_src]; }

	// Returns true if triangle visible, false otherwise
	bool				AddTri(u32 v0, u32 v1, u32 v2);

	// Render our current triangle list to screen
	void				FlushTris();
	//void				Line3D( u32 v0, u32 v1, u32 width );

	// Returns true if bounding volume is visible within NDC box, false if culled
	bool				TestVerts( u32 v0, u32 vn ) const;
	inline f32			GetVtxDepth( u32 i ) const				{ return mVtxProjected[ i ].ProjectedPos.z; }
	inline f32			GetVtxWeight( u32 i ) const				{ return mVtxProjected[ i ].ProjectedPos.w; }
	inline v4			GetTransformedVtxPos( u32 i ) const		{ return mVtxProjected[ i ].TransformedPos; }
	inline v4			GetProjectedVtxPos( u32 i ) const		{ return mVtxProjected[ i ].ProjectedPos; }
	inline u32			GetVtxFlags( u32 i ) const				{ return mVtxProjected[ i ].ClipFlags; }

	inline u64			GetMux() const							{ return mMux; }
	inline c32			GetPrimitiveColour() const				{ return mPrimitiveColour; }
	inline c32			GetEnvColour() const					{ return mEnvColour; }
	inline c32			GetFogColour() const					{ return mFogColour; }
	inline c32			GetBlendColour() const					{ return mBlendColour; }
	inline u32			GetFillColour() const					{ return mFillColour; }

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	// Rendering stats
	inline u32			GetNumTrisRendered() const				{ return mNumTrisRendered; }
	inline u32			GetNumTrisClipped() const				{ return mNumTrisClipped; }
	inline u32			GetNumRect() const						{ return mNumRect; }


	virtual void 		ResetDebugState()						{}
#endif
#ifdef DAEDALUS_CTR
	inline float		N64ToScreenX(float x) const				{ return x * mN64ToScreenScale.x; }
	inline float		N64ToScreenY(float y) const				{ return y * mN64ToScreenScale.y; }
#else
	inline float		N64ToScreenX(float x) const				{ return x * mN64ToScreenScale.x + mN64ToScreenTranslate.x; }
	inline float		N64ToScreenY(float y) const				{ return y * mN64ToScreenScale.y + mN64ToScreenTranslate.y; }
#endif

	std::shared_ptr<CNativeTexture> LoadTextureDirectly( const TextureInfo & ti );

protected:
#ifdef DAEDALUS_PSP
	inline void			UpdateFogEnable()						{ if(gFogEnabled) mTnL.Flags.Fog ? sceGuEnable(GU_FOG) : sceGuDisable(GU_FOG); }
	inline void			UpdateShadeModel()						{ sceGuShadeModel( mTnL.Flags.Shade ? GU_SMOOTH : GU_FLAT ); }
#else
	inline void			UpdateFogEnable()						{ if(gFogEnabled) mTnL.Flags.Fog ? glEnable(GL_FOG) : glDisable(GL_FOG); }
	inline void			UpdateShadeModel()						{ glShadeModel( mTnL.Flags.Shade ? GL_SMOOTH : GL_FLAT ); }
#endif
	void				UpdateTileSnapshots( u32 tile_idx );
	void				UpdateTileSnapshot( u32 index, u32 tile_idx );

	void 				PrepareTexRectUVs(TexCoord * puv0, TexCoord * puv1);

	virtual void		RestoreRenderStates() = 0;

	//*****************************************************************************
	// We round these value here, so that when we scale up the coords to our screen
	// coords we don't get any gaps.
	//*****************************************************************************
	inline void ConvertN64ToScreen( const v2 & n64_coords, v2 & answ ) const
	{
		answ.x = roundf( N64ToScreenX( roundf( n64_coords.x ) ) );
		answ.y = roundf( N64ToScreenY( roundf( n64_coords.y ) ) );
	}

	inline void ScaleN64ToScreen( const v2 & n64_coords, v2 & answ ) const
	{
		answ.x = roundf( roundf( n64_coords.x ) * mN64ToScreenScale.x );
		answ.y = roundf( roundf( n64_coords.y ) * mN64ToScreenScale.y );
	}


	virtual void		RenderTriangles( DaedalusVtx * p_vertices, u32 num_vertices, bool disable_zbuffer ) = 0;

	void 				TestVFPUVerts( u32 v0, u32 num, const FiddledVtx * verts, const Matrix4x4 & mat_world );
	template< bool FogEnable, int TextureMode >
	void ProcessVerts( u32 v0, u32 num, const FiddledVtx * verts, const Matrix4x4 & mat_world );


	void				PrepareTrisClipped( TempVerts * temp_verts ) const;
	void				PrepareTrisUnclipped( TempVerts * temp_verts ) const;

	v3					LightVert( const v3 & norm ) const;
	v3					LightPointVert( const v4 & w ) const;

private:
	void				InitViewport();
	void				UpdateViewport();

	inline void			UpdateWorldProject();

protected:
	TnLParams			mTnL;

	v2					mN64ToScreenScale;
	v2					mN64ToScreenTranslate;

	v2					mVpScale;
	v2					mVpTrans;

	u64					mMux;
	
	u32					mTextureTile;

	f32					mPrimDepth;
	f32					mPrimLODFraction;
	f32 				mfog_near;
	f32 				mfog_far;

	c32					mFogColour;				// Blender
	c32					mPrimitiveColour;		// Combiner
	c32					mEnvColour;				// Combiner
	c32					mBlendColour;			// Blender
	u32					mFillColour;			// RDP. NB u32 not c32 as this is typically 2 16-bit colour values.
	// Texturing
	struct TextureWrap
	{
		u32	u;
		u32 v;
	};
	struct TextureShift
	{
		u8 s;
		u8 t;
	};
	static const u32 kNumBoundTextures = 2;

	std::array<TextureInfo, kNumBoundTextures> mBoundTextureInfo;
	std::shared_ptr<CNativeTexture>	mBoundTexture[ kNumBoundTextures ];
	std::array<TexCoord, kNumBoundTextures> mTileTopLeft;
	std::array<TextureWrap, kNumBoundTextures> mTexWrap;

	// TextureInfo				mBoundTextureInfo[ kNumBoundTextures ];
	// TexCoord				mTileTopLeft[ kNumBoundTextures ];
	// TextureWrap				mTexWrap[ kNumBoundTextures ];

	// Index of the corresponding tile state.
	// u8						mActiveTile[ kNumBoundTextures ];
	std::array<u8, kNumBoundTextures> mActiveTile;

	//Max is 18 according to the manual //Corn
	//I think we should make this more deep to avoid any issues //Salvy
	static const u32 MATRIX_STACK_SIZE = 20;

	mutable Matrix4x4	mWorldProject;
	Matrix4x4			mTempMat;
	Matrix4x4			mProjectionMat;
	Matrix4x4			mModelViewStack[MATRIX_STACK_SIZE];	//DKR reuses these and need at least 4 //Corn
	u32					mModelViewTop;
	u32					mMatStackSize;
	mutable bool		mWorldProjectValid;
	bool				mReloadProj;
	bool				mWPmodified;
	u32					mDKRMatIdx;

	float				mScreenWidth;
	float				mScreenHeight;

#if defined(DAEDALUS_GL) || defined(DAEDALUS_CTR) || defined(DAEDALUS_GLES)
	Matrix4x4			mScreenToDevice;					// Used by OSX renderer - scales screen coords (0..640 etc) to device coords (-1..+1)
#endif

	static const u32 	kMaxIndices = 320;					// We need at least 80 verts * 3 = 240? But Flying Dragon uses more than 256 //Corn
	// std::array<u16, kMaxIndices> mIndexBuffer;
	u16					mIndexBuffer[kMaxIndices];
	u32					mNumIndices;

	// Processed vertices waiting for output...
	// std::array<DaedalusVtx4, kMaxN64Vertices> mVtxProjected;
	DaedalusVtx4		mVtxProjected[kMaxN64Vertices];		// Transformed and projected vertices (suitable for clipping etc)
	u32					mVtxClipFlagsUnion;					// Bitwise OR of all the vertex flags added to the current batch. If this is 0, we can trivially accept everything without clipping


#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	//
	// Stats
	//
	u32					mNumTrisRendered;
	u32					mNumTrisClipped;
	u32					mNumRect;

	// Debugging
	bool				mNastyTexture;
#endif
};

bool CreateRenderer();
void DestroyRenderer();
extern BaseRenderer * gRenderer;

inline s16 ApplyShift(s16 c, u8 shift)
{
	if (shift <= 10)
		return c << shift;

	return c >> (16 - shift);
}

#endif // HLEGRAPHICS_BASERENDERER_H_
