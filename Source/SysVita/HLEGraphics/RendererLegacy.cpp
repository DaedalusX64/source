#include "stdafx.h"
#include "RendererLegacy.h"

#include <vitaGL.h>

#include "Combiner/BlendConstant.h"
#include "Combiner/CombinerTree.h"
#include "Combiner/RenderSettings.h"
#include "Core/ROM.h"
#include "Debug/Dump.h"
#include "Debug/DBGConsole.h"
#include "Graphics/GraphicsContext.h"
#include "Graphics/NativeTexture.h"
#include "HLEGraphics/CachedTexture.h"
#include "HLEGraphics/DLDebug.h"
#include "HLEGraphics/RDPStateManager.h"
#include "HLEGraphics/TextureCache.h"
#include "Math/MathUtil.h"
#include "OSHLE/ultra_gbi.h"
#include "Utility/IO.h"
#include "Utility/Profiler.h"

#define NORMALIZE_C1842XX(x) ((x) > 16.5f ? ((x) / ((x) / 16.0f)) : (x))

BaseRenderer *gRenderer    = nullptr;
RendererLegacy  *gRendererLegacy = nullptr;

extern float *gVertexBuffer;
extern uint32_t *gColorBuffer;
extern float *gTexCoordBuffer;
extern u32 aux_draws;
extern u32 aux_discard;

bool gUseMipmaps = false;

struct ScePspFMatrix4
{
	float m[16];
};


ScePspFMatrix4		gProjection;
void sceGuSetMatrix(int type, const ScePspFMatrix4 * mtx)
{
	if (type == GL_PROJECTION)
	{
		sceClibMemcpy(&gProjection, mtx, sizeof(gProjection));
	}
}

static void InitBlenderMode()
{
	u32 cycle_type    = gRDPOtherMode.cycle_type;
	u32 cvg_x_alpha   = gRDPOtherMode.cvg_x_alpha;
	u32 alpha_cvg_sel = gRDPOtherMode.alpha_cvg_sel;
	u32 blendmode     = gRDPOtherMode.blender;

	// NB: If we're running in 1cycle mode, ignore the 2nd cycle.
	u32 active_mode = (cycle_type == CYCLE_2CYCLE) ? blendmode : (blendmode & 0xCCCC);
	
	if (alpha_cvg_sel && (gRDPOtherMode.L & 0x7000) != 0x7000) {
		switch (active_mode) {
		case 0x4055: // Mario Golf
		case 0x5055: // Paper Mario Intro
			glBlendFunc(GL_ZERO, GL_ONE);
			glEnable(GL_BLEND);
			break;
		default:
			glDisable(GL_BLEND);
		}
		return;
	}

	switch (active_mode)
	{
	case 0x00C0: // ISS 64
	case 0x0091: // Mace special blend mode
	case 0x0302: // Bomberman 2 special blend mode
	case 0x0382: // Mace objects
	case 0x07C2: // ISS 64
	case 0x0C08: // 1080 sky
	case 0xC302: // ISS 64
	case 0xC702: // Donald Duck: Quack Attack
	case 0xC800: // Conker's Bad Fur Day
	case 0x0F0A: // DK64 blueprints
	case 0xA500: // Sin and Punishment
	case 0xFA00: // Bomberman second attack
		glDisable(GL_BLEND);
		break;
	case 0x55F0: // Bust-A-Move 3 DX
		glBlendFunc(GL_ONE, GL_SRC_ALPHA);
		glEnable(GL_BLEND);
		break;
	case 0x0F1A:
		if (cycle_type == CYCLE_1CYCLE)
			glDisable(GL_BLEND);
		else {
			glBlendFunc(GL_ZERO, GL_ONE);
			glEnable(GL_BLEND);
		}
		break;
	case 0x0448: // Space Invaders
	case 0x0554:
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_BLEND);
		break;
	case 0x0F5A: // Zelda: MM
	case 0x0FA5: // OOT menu
	case 0x5055: // Paper Mario intro
	case 0xAF50: // Zelda: MM
	case 0xC712: // Pokemon Stadium
		glBlendFunc(GL_ZERO, GL_ONE);
		glEnable(GL_BLEND);
		break;
	case 0x0C40: // Extreme-G
	case 0x0C48: // Star Wars: Shadow of the Empire text and hud
	case 0x4C40: // Wave Race
	case 0x5F50:
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		break;
	case 0x0010: // Diddy Kong rare logo
	case 0x0040: // F-Zero X
	case 0x0050: // A Bug's Life
	case 0x0051:
	case 0x0055:
	case 0x0150: // Spiderman
	case 0x0321:
	case 0x0440: // Bomberman 64
	case 0x04D0: // Conker's Bad Fur Day
	case 0x0550: // Bomberman 64
	case 0x0C18: // StarFox 64 main menu
	case 0x0F54: // Star Wars racers
	case 0xC410: // Donald Duck: Quack Attack dust
	case 0xC440: // Banjo-Kazooie / Banjo-Tooie
	case 0xC810: // AeroGauge
	case 0xCB02: // Doom 64 weapons
	case 0x0D18:
	case 0x8410: // Paper Mario
	case 0xF550:
		if (!(!alpha_cvg_sel || cvg_x_alpha)) glDisable(GL_BLEND);
		else {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
		}
		break;
	case 0xC912: // 40 Winks
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glEnable(GL_BLEND);
		break;
	case 0x0C19:
	case 0xC811:
		glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
		glEnable(GL_BLEND);
		break;
	case 0x5000: // V8 explosions
		glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
		glEnable(GL_BLEND);
		break;
	default:
		//DBGConsole_Msg(0, "Uncommon blender mode: 0x%04X", active_mode);
		if (!(!alpha_cvg_sel || cvg_x_alpha)) glDisable(GL_BLEND);
		else {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
		}
		break;
	}
}

RendererLegacy::RendererLegacy()
{
	//
	//	Set up RGB = T0, A = T0
	//
	mCopyBlendStates = new CBlendStates;
	{
		CAlphaRenderSettings *	alpha_settings( new CAlphaRenderSettings( "Copy" ) );
		CRenderSettingsModulate *	colour_settings( new CRenderSettingsModulate( "Copy" ) );

		alpha_settings->AddTermTexel0();
		colour_settings->AddTermTexel0();

		mCopyBlendStates->SetAlphaSettings( alpha_settings );
		mCopyBlendStates->AddColourSettings( colour_settings );
	}

	//
	//	Set up RGB = Diffuse, A = Diffuse
	//
	mFillBlendStates = new CBlendStates;
	{
		CAlphaRenderSettings *	alpha_settings( new CAlphaRenderSettings( "Fill" ) );
		CRenderSettingsModulate *	colour_settings( new CRenderSettingsModulate( "Fill" ) );

		alpha_settings->AddTermConstant( new CBlendConstantExpressionValue( BC_SHADE ) );
		colour_settings->AddTermConstant(  new CBlendConstantExpressionValue( BC_SHADE ) );

		mFillBlendStates->SetAlphaSettings( alpha_settings );
		mFillBlendStates->AddColourSettings( colour_settings );
	}

}

RendererLegacy::~RendererLegacy()
{
	delete mFillBlendStates;
	delete mCopyBlendStates;
}

void RendererLegacy::RestoreRenderStates()
{
	// Initialise the device to our default state

	// No fog
	glDisable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	
	glEnable(GL_SCISSOR_TEST);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable( GL_BLEND );
	
	// Default is ZBuffer disabled
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_DEPTH_TEST);
	
	// Enable this for rendering decals (glPolygonOffset).
	glEnable(GL_POLYGON_OFFSET_FILL);
		
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	float envcolor[4] = {c32::White.GetRf(), c32::White.GetGf(), c32::White.GetBf(), c32::White.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

RendererLegacy::SBlendStateEntry RendererLegacy::LookupBlendState( u64 mux, bool two_cycles )
{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DAEDALUS_PROFILE( "RendererPSP::LookupBlendState" );
	mRecordedCombinerStates.insert( mux );
#endif

	REG64 key;
	key._u64 = mux;

	// Top 8 bits are never set - use the very top one to differentiate between 1/2 cycles
	key._u32_1 |= (two_cycles << 31);

	BlendStatesMap::const_iterator	it( mBlendStatesMap.find( key._u64 ) );
	if( it != mBlendStatesMap.end() )
	{
		return it->second;
	}

	// Blendmodes with Inexact blends either get an Override blend or a Default blend (GU_TFX_MODULATE)
	// If its not an Inexact blend then we check if we need to Force a blend mode none the less// Salvy
	//
	SBlendStateEntry entry;
	CCombinerTree tree( mux, two_cycles );
	entry.States = tree.GetBlendStates();

	if( entry.States->IsInexact() )
	{
		entry.OverrideFunction = LookupOverrideBlendModeInexact( mux );
	}
	else
	{
		// This is for non-inexact blends, errg hacks and such to be more precise
		entry.OverrideFunction = LookupOverrideBlendModeForced( mux );
	}

	#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	printf( "Adding %08x%08x - %d cycles - %s\n", u32(mux>>32), u32(mux), two_cycles ? 2 : 1, entry.States->IsInexact() ?  IsCombinerStateDefault(mux) ? "Inexact(Default)" : "Inexact(Override)" : entry.OverrideFunction==nullptr ? "Auto" : "Forced");
	#endif

	//Add blend mode to the Blend States Map
	mBlendStatesMap[ key._u64 ] = entry;

	return entry;
}

void RendererLegacy::RenderUsingRenderSettings( const CBlendStates * states, u32 * p_vertices, u32 num_vertices, u32 triangle_mode, bool is_3d)
{
	const CAlphaRenderSettings *	alpha_settings( states->GetAlphaSettings() );

	SRenderState	state;

	state.Vertices = p_vertices;
	state.NumVertices = num_vertices;
	state.PrimitiveColour = mPrimitiveColour;
	state.EnvironmentColour = mEnvColour;
	
	if( states->GetNumStates() > 1 )
	{
		sceClibMemcpy( mVtx_Save, p_vertices, num_vertices * sizeof( u32 ) );
	}
	
	glEnableClientState(GL_COLOR_ARRAY);

	for( u32 i = 0; i < states->GetNumStates(); ++i )
	{
		const CRenderSettings *		settings( states->GetColourSettings( i ) );

		bool install_texture0( settings->UsesTexture0() || alpha_settings->UsesTexture0() );
		bool install_texture1( settings->UsesTexture1() || alpha_settings->UsesTexture1() );

		SRenderStateOut out;

		memset( &out, 0, sizeof( out ) );

		settings->Apply( install_texture0 || install_texture1, state, out );
		alpha_settings->Apply( install_texture0 || install_texture1, state, out );

		if( i > 0 )
		{
			sceClibMemcpy(gColorBuffer, mVtx_Save, num_vertices * sizeof( u32 ) );
			p_vertices = gColorBuffer;
			gColorBuffer += num_vertices;
		}

		if(out.VertexExpressionRGB != nullptr)
		{
			out.VertexExpressionRGB->ApplyExpressionRGB( state );
		}
		if(out.VertexExpressionA != nullptr)
		{
			out.VertexExpressionA->ApplyExpressionAlpha( state );
		}

		bool installed_texture {false};

		u32 texture_idx;

		if(install_texture0 || install_texture1)
		{
			u32	tfx = GL_MODULATE;
			switch( out.BlendMode )
			{
			case PBM_REPLACE:
				{
					tfx = GL_REPLACE;
					break;
				}
			case PBM_BLEND:
				{
					tfx = GL_BLEND;
					float envcolor[4] = {out.TextureFactor.GetRf(), out.TextureFactor.GetGf(), out.TextureFactor.GetBf(), out.TextureFactor.GetAf()};
					glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
					break;
				}
			default:
				{
					tfx = GL_MODULATE;
					break;
				}
			}
			
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tfx);

			if( g_ROM.TEXELS_HACK )
			{
				// NB if install_texture0 and install_texture1 are both set, 1 wins out
				texture_idx = install_texture1;

				// NOTE: Rinnegatamante 15/04/20
				// Technically we calculate this on DrawTriangles, is it enough? Can tex1 and tex0 be of different sizes?
				
				/*const CNativeTexture * texture1 = mBoundTexture[ 1 ];

				if( install_texture1 && texture1 && mTnL.Flags.Texture && (mTnL.Flags._u32 & (TNL_LIGHT|TNL_TEXGEN)) != (TNL_LIGHT|TNL_TEXGEN) )
				{
					
					float scale_x = texture1->GetScaleX();
					float scale_y = texture1->GetScaleY();

					sceGuTexOffset( -mTileTopLeft[ 1 ].s * scale_x / 4.f,
									-mTileTopLeft[ 1 ].t * scale_y / 4.f );
					sceGuTexScale( scale_x, scale_y );
				}*/
			}
			else
			{
				// NB if install_texture0 and install_texture1 are both set, 0 wins out
				texture_idx = install_texture0 ? 0 : 1;
			}

			CRefPtr<CNativeTexture> texture;

			if(out.MakeTextureWhite)
			{
				TextureInfo white_ti = mBoundTextureInfo[ texture_idx ];
				white_ti.SetWhite(true);
				texture = CTextureCache::Get()->GetOrCreateTexture( white_ti );
			}
			else
			{
				texture = mBoundTexture[ texture_idx ];
			}

			if(texture != nullptr)
			{
				texture->InstallTexture();
				if (is_3d && gUseMipmaps) texture->GenerateMipmaps();
				installed_texture = true;
			}
		}

		// If no texture was specified, or if we couldn't load it, clear it out
		if( !installed_texture )
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mTexWrap[texture_idx].u);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mTexWrap[texture_idx].v);
		}
		
		glEnableClientState(GL_COLOR_ARRAY);
		vglColorPointerMapped(GL_UNSIGNED_BYTE, p_vertices);	
		vglDrawObjects(triangle_mode, num_vertices, GL_TRUE);
	}
}


void RendererLegacy::RenderUsingCurrentBlendMode(const float (&mat_project)[16], uint32_t *p_vertices, u32 num_vertices, u32 triangle_mode, bool disable_zbuffer, bool is_3d)
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((float*)mat_project);
	if (g_ROM.PROJ_HACK && is_3d) glScalef(1, -1, 1);
	
	if ( disable_zbuffer )
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	else
	{
		// Decal mode
		if( gRDPOtherMode.zmode == 3 )
		{
			glPolygonOffset(-1.0, -1.0);
		}
		else
		{
			glPolygonOffset(0.0, 0.0);
		}
		
		// Enable or Disable ZBuffer test
		if ((mTnL.Flags.Zbuffer && gRDPOtherMode.z_cmp) || gRDPOtherMode.z_upd)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		glDepthMask(gRDPOtherMode.z_upd ? GL_TRUE : GL_FALSE );
	}
	
	u32 cycle_mode = gRDPOtherMode.cycle_type;
	
	// Initiate Texture Filter
	//
	// G_TF_AVERAGE : 1, G_TF_BILERP : 2 (linear)
	// G_TF_POINT   : 0 (nearest)
	//
	if (((gRDPOtherMode.text_filt != G_TF_POINT) && cycle_mode != CYCLE_COPY) || (gGlobalPreferences.ForceLinearFilter))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	
	// Initiate Blender
	//
	if(cycle_mode < CYCLE_COPY && gRDPOtherMode.force_bl)
	{
		InitBlenderMode();
	}
	else if (gRDPOtherMode.clr_on_cvg)
	{
		if ((cycle_mode == CYCLE_1CYCLE && gRDPOtherMode.c1_m2a == 1) ||
		    (cycle_mode == CYCLE_2CYCLE && gRDPOtherMode.c2_m2a == 1)) {
			glBlendFunc(GL_ZERO, GL_ONE);
			glEnable(GL_BLEND);
		} else
			glDisable( GL_BLEND );
	}
	else
	{
		glDisable( GL_BLEND );
	}
	
	// Initiate Alpha test
	//
	if( (gRDPOtherMode.alpha_compare == G_AC_THRESHOLD) && !gRDPOtherMode.alpha_cvg_sel )
	{
		u8 alpha_threshold = mBlendColour.GetA();
		glAlphaFunc((alpha_threshold || g_ROM.ALPHA_HACK) ? GL_GEQUAL : GL_GREATER, mBlendColour.GetAf());
		glEnable(GL_ALPHA_TEST);
	}
	else if (gRDPOtherMode.cvg_x_alpha)
	{
		glAlphaFunc(GL_GEQUAL, 0.04392f);
		glEnable(GL_ALPHA_TEST);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}
	
	SBlendStateEntry		blend_entry;

	switch ( cycle_mode )
	{
		case CYCLE_COPY:		blend_entry.States = mCopyBlendStates; break;
		case CYCLE_FILL:		blend_entry.States = mFillBlendStates; break;
		case CYCLE_1CYCLE:		blend_entry = LookupBlendState( mMux, false ); break;
		case CYCLE_2CYCLE:		blend_entry = LookupBlendState( mMux, true ); break;
	}
	
	if( blend_entry.OverrideFunction != nullptr )
	{
		// Local vars for now
		SBlendModeDetails details;

		details.EnvColour = mEnvColour;
		details.PrimColour = mPrimitiveColour;
		details.InstallTexture = true;
		details.ColourAdjuster.Reset();

		blend_entry.OverrideFunction( details );
		
		if( details.InstallTexture )
		{
			int texture_idx = g_ROM.TEXELS_HACK ? 1 : 0;
			if( mBoundTexture[ texture_idx ] )
			{
				mBoundTexture[ texture_idx ]->InstallTexture();
				if (is_3d && gUseMipmaps) mBoundTexture[ texture_idx ]->GenerateMipmaps();
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mTexWrap[texture_idx].u);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mTexWrap[texture_idx].v);
			} else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		} else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		
		details.ColourAdjuster.Process(p_vertices, num_vertices);
		glEnableClientState(GL_COLOR_ARRAY);
		vglColorPointerMapped(GL_UNSIGNED_BYTE, p_vertices);
		vglDrawObjects(triangle_mode, num_vertices, GL_TRUE);
	}
	else if( blend_entry.States != nullptr )
	{
		RenderUsingRenderSettings( blend_entry.States, p_vertices, num_vertices, triangle_mode, is_3d );
	}
	else
	{
		#ifdef DAEDALUS_DEBUG_CONSOLE
		// Set default states
		DAEDALUS_ERROR( "Unhandled blend mode" );
		#endif
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		vglColorPointerMapped(GL_UNSIGNED_BYTE, p_vertices);
		vglDrawObjects(triangle_mode, num_vertices, GL_TRUE);
	}

}

void RendererLegacy::RenderTriangles(uint32_t *colors, u32 num_vertices, bool disable_zbuffer)
{	
	SetPositiveViewport();
	RenderUsingCurrentBlendMode(gProjection.m, colors, num_vertices, GL_TRIANGLES, disable_zbuffer, true);
}

void RendererLegacy::TexRect(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1)
{
	if (g_ROM.GameHacks == POKEMON_STADIUM) {
		if (aux_draws) {
			aux_draws--;
			if ((aux_draws < aux_discard) && (xy0.x == 52.0f)) return;
			SetN64Viewport( aux_scale, aux_trans );
		}
	}
	
	UpdateTileSnapshots( tile_idx );
	PrepareTexRectUVs(&st0, &st1);
	
	v2 uv0( (float)st0.s / 32.f, (float)st0.t / 32.f );
	v2 uv1( (float)st1.s / 32.f, (float)st1.t / 32.f );

	//if ((gRDPOtherMode.L & 0xFFFFFF00) == 0x0C184200) CDebugConsole::Get()->Msg(1, "TexRect: L: 0x%08X, U: %f, V: %f, U2: %f, V2: %f", gRDPOtherMode.L, uv0.x, uv0.y, uv1.x, uv1.y);

	v2 screen0;
	v2 screen1;
	if (gAspectRatio == RATIO_16_9_HACK) {
		screen0.x = roundf( roundf( HD_SCALE * xy0.x ) * mN64ToScreenScale.x + 118 );
		screen0.y = roundf( roundf( xy0.y )            * mN64ToScreenScale.y + mN64ToScreenTranslate.y );
		screen1.x = roundf( roundf( HD_SCALE * xy1.x ) * mN64ToScreenScale.x + 118 );
		screen1.y = roundf( roundf( xy1.y )            * mN64ToScreenScale.y + mN64ToScreenTranslate.y );
	} else {
		ScaleN64ToScreen( xy0, screen0 );
		ScaleN64ToScreen( xy1, screen1 );
	}
	
	const f32 depth = gRDPOtherMode.depth_source ? mPrimDepth : 0.0f;

	CNativeTexture *texture = mBoundTexture[0];
	float scale_x = texture->GetScaleX();
	float scale_y = texture->GetScaleY();
	
	if (g_ROM.TEXELS_HACK && (gRDPOtherMode.L == 0x0C184244)) {
		scale_x *= 0.125f;
		scale_y *= 0.125f;
	}
	
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	if (g_ROM.TEXELS_HACK && (gRDPOtherMode.L == 0x0C184244)) {
		gTexCoordBuffer[0] = NORMALIZE_C1842XX(uv0.x) * scale_x;
		gTexCoordBuffer[1] = NORMALIZE_C1842XX(uv0.y) * scale_y;
		gTexCoordBuffer[2] = NORMALIZE_C1842XX(uv1.x) * scale_x;
		gTexCoordBuffer[3] = NORMALIZE_C1842XX(uv0.y) * scale_y;
		gTexCoordBuffer[4] = NORMALIZE_C1842XX(uv0.x) * scale_x;
		gTexCoordBuffer[5] = NORMALIZE_C1842XX(uv1.y) * scale_y;
		gTexCoordBuffer[6] = NORMALIZE_C1842XX(uv1.x) * scale_x;
		gTexCoordBuffer[7] = NORMALIZE_C1842XX(uv1.y) * scale_y;	
	} else {
		gTexCoordBuffer[0] = uv0.x * scale_x;
		gTexCoordBuffer[1] = uv0.y * scale_y;
		gTexCoordBuffer[2] = uv1.x * scale_x;
		gTexCoordBuffer[3] = uv0.y * scale_y;
		gTexCoordBuffer[4] = uv0.x * scale_x;
		gTexCoordBuffer[5] = uv1.y * scale_y;
		gTexCoordBuffer[6] = uv1.x * scale_x;
		gTexCoordBuffer[7] = uv1.y * scale_y;
	}
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gTexCoordBuffer += 8;
	
	gVertexBuffer[0] = screen0.x;
	gVertexBuffer[1] = screen0.y;
	gVertexBuffer[2] = depth;
	gVertexBuffer[3] = screen1.x;
	gVertexBuffer[4] = screen0.y;
	gVertexBuffer[5] = depth;
	gVertexBuffer[6] = screen0.x;
	gVertexBuffer[7] = screen1.y;
	gVertexBuffer[8] = depth;
	gVertexBuffer[9] = screen1.x;
	gVertexBuffer[10] = screen1.y;
	gVertexBuffer[11] = depth;
	vglVertexPointerMapped(3, gVertexBuffer);
	gVertexBuffer += 12;

	uint32_t *p_vertices = gColorBuffer;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = 0xFFFFFFFF;
	gColorBuffer += 4;
	
	SetNegativeViewport();
	
	RenderUsingCurrentBlendMode(mScreenToDevice.mRaw, p_vertices, 4, GL_TRIANGLE_STRIP, gRDPOtherMode.depth_source ? false : true, false);
}

void RendererLegacy::TexRectFlip(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1)
{
	UpdateTileSnapshots( tile_idx );
	PrepareTexRectUVs(&st0, &st1);

	v2 uv0( (float)st0.s / 32.f, (float)st0.t / 32.f );
	v2 uv1( (float)st1.s / 32.f, (float)st1.t / 32.f );

	//if ((gRDPOtherMode.L & 0xFFFFFF00) == 0x0C184200) CDebugConsole::Get()->Msg(1, "TexRectFlip: L: 0x%08X, U: %f, V: %f, U2: %f, V2: %f", gRDPOtherMode.L, uv0.x, uv0.y, uv1.x, uv1.y);

	v2 screen0;
	v2 screen1;
	ScaleN64ToScreen( xy0, screen0 );
	ScaleN64ToScreen( xy1, screen1 );

	CNativeTexture *texture = mBoundTexture[0];
	float scale_x = texture->GetScaleX();
	float scale_y = texture->GetScaleY();
	
	gTexCoordBuffer[0] = uv0.x * scale_x;
	gTexCoordBuffer[1] = uv0.y * scale_y;
	gTexCoordBuffer[2] = uv0.x * scale_x;
	gTexCoordBuffer[3] = uv1.y * scale_y;
	gTexCoordBuffer[4] = uv1.x * scale_x;
	gTexCoordBuffer[5] = uv0.y * scale_y;
	gTexCoordBuffer[6] = uv1.x * scale_x;
	gTexCoordBuffer[7] = uv1.y * scale_y;
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gTexCoordBuffer += 8;
	
	gVertexBuffer[0] = screen0.x;
	gVertexBuffer[1] = screen0.y;
	gVertexBuffer[2] = 0.0f;
	gVertexBuffer[3] = screen1.x;
	gVertexBuffer[4] = screen0.y;
	gVertexBuffer[5] = 0.0f;
	gVertexBuffer[6] = screen0.x;
	gVertexBuffer[7] = screen1.y;
	gVertexBuffer[8] = 0.0f;
	gVertexBuffer[9] = screen1.x;
	gVertexBuffer[10] = screen1.y;
	gVertexBuffer[11] = 0.0f;
	vglVertexPointerMapped(3, gVertexBuffer);
	gVertexBuffer += 12;
	
	uint32_t *p_vertices = gColorBuffer;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = 0xFFFFFFFF;
	gColorBuffer += 4;

	SetNegativeViewport();
	
	RenderUsingCurrentBlendMode(mScreenToDevice.mRaw, p_vertices, 4, GL_TRIANGLE_STRIP, gRDPOtherMode.depth_source ? false : true, false);
}

void RendererLegacy::FillRect(const v2 & xy0, const v2 & xy1, u32 color)
{
	//if ((gRDPOtherMode.L & 0xFFFFFF00) == 0x0C184200) CDebugConsole::Get()->Msg(1, "FillRect: L: 0x%08X", gRDPOtherMode.L);
	v2 screen0;
	v2 screen1;
	ScaleN64ToScreen( xy0, screen0 );
	ScaleN64ToScreen( xy1, screen1 );
	
	gVertexBuffer[0] = screen0.x;
	gVertexBuffer[1] = screen0.y;
	gVertexBuffer[2] = 0.0f;
	gVertexBuffer[3] = screen1.x;
	gVertexBuffer[4] = screen0.y;
	gVertexBuffer[5] = 0.0f;
	gVertexBuffer[6] = screen0.x;
	gVertexBuffer[7] = screen1.y;
	gVertexBuffer[8] = 0.0f;
	gVertexBuffer[9] = screen1.x;
	gVertexBuffer[10] = screen1.y;
	gVertexBuffer[11] = 0.0f;
	vglVertexPointerMapped(3, gVertexBuffer);
	gVertexBuffer += 12;
	
	uint32_t *p_vertices = gColorBuffer;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = color;
	gColorBuffer += 4;
	
	SetNegativeViewport();
	
	RenderUsingCurrentBlendMode(mScreenToDevice.mRaw, p_vertices, 4, GL_TRIANGLE_STRIP, true, false);
}

void RendererLegacy::DoGamma(float gamma)
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
	
	gVertexBuffer[0] = 0.0f;
	gVertexBuffer[1] = 0.0f;
	gVertexBuffer[2] = 0.0f;
	gVertexBuffer[3] = SCR_WIDTH;
	gVertexBuffer[4] = 0.0f;
	gVertexBuffer[5] = 0.0f;
	gVertexBuffer[6] = 0.0f;
	gVertexBuffer[7] = SCR_HEIGHT;
	gVertexBuffer[8] = 0.0f;
	gVertexBuffer[9] = SCR_WIDTH;
	gVertexBuffer[10] = SCR_HEIGHT;
	gVertexBuffer[11] = 0.0f;
	vglVertexPointerMapped(3, gVertexBuffer);
	gVertexBuffer += 12;
	
	// Hack to use float colors without having to use a temporary buffer
	gVertexBuffer[0] = gVertexBuffer[1] = gVertexBuffer[2] = 1.0f;
	gVertexBuffer[3] = gamma;
	gVertexBuffer[4] = gVertexBuffer[5] = gVertexBuffer[6] = 1.0f;
	gVertexBuffer[7] = gamma;
	gVertexBuffer[8] = gVertexBuffer[9] = gVertexBuffer[10] = 1.0f;
	gVertexBuffer[11] = gamma;
	gVertexBuffer[12] = gVertexBuffer[13] = gVertexBuffer[14] = 1.0f;
	gVertexBuffer[15] = gamma;
	
	vglColorPointerMapped(GL_FLOAT, gVertexBuffer);
	gVertexBuffer += 16;
	
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((float*)mScreenToDevice.mRaw);
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
	glDisable(GL_BLEND);
}

void RendererLegacy::DrawUITexture()
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	gVertexBuffer[0] = 0.0f;
	gVertexBuffer[1] = 0.0f;
	gVertexBuffer[2] = 0.0f;
	gVertexBuffer[3] = SCR_WIDTH;
	gVertexBuffer[4] = 0.0f;
	gVertexBuffer[5] = 0.0f;
	gVertexBuffer[6] = 0.0f;
	gVertexBuffer[7] = SCR_HEIGHT;
	gVertexBuffer[8] = 0.0f;
	gVertexBuffer[9] = SCR_WIDTH;
	gVertexBuffer[10] = SCR_HEIGHT;
	gVertexBuffer[11] = 0.0f;
	vglVertexPointerMapped(3, gVertexBuffer);
	gVertexBuffer += 12;
	
	gTexCoordBuffer[0] = 0.0f;
	gTexCoordBuffer[1] = 0.0f;
	gTexCoordBuffer[2] = 1.0f;
	gTexCoordBuffer[3] = 0.0f;
	gTexCoordBuffer[4] = 0.0f;
	gTexCoordBuffer[5] = 1.0f;
	gTexCoordBuffer[6] = 1.0f;
	gTexCoordBuffer[7] = 1.0f;
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gTexCoordBuffer += 8;

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
}

void RendererLegacy::Draw2DTexture(f32 x0, f32 y0, f32 x1, f32 y1,
								f32 u0, f32 v0, f32 u1, f32 v1)
{
	const CNativeTexture *texture = mBoundTexture[0];
	if (!texture) return;
	
	//if ((gRDPOtherMode.L & 0xFFFFFF00) == 0x0C184200) CDebugConsole::Get()->Msg(1, "Draw2DTexture: L: 0x%08X", gRDPOtherMode.L);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((float*)mScreenToDevice.mRaw);
	
	// Enable or Disable ZBuffer test
	if ((mTnL.Flags.Zbuffer && gRDPOtherMode.z_cmp) || gRDPOtherMode.z_upd)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
	
	glDepthMask(gRDPOtherMode.z_upd ? GL_TRUE : GL_FALSE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	float scale_x = texture->GetScaleX();
	float scale_y = texture->GetScaleY();
	
	float sx0 = LightN64ToScreenX(x0);
	float sy0 = LightN64ToScreenY(y0);

	float sx1 = LightN64ToScreenX(x1);
	float sy1 = LightN64ToScreenY(y1);
	
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	gVertexBuffer[0] = sx0;
	gVertexBuffer[1] = sy0;
	gVertexBuffer[2] = 0.0f;
	gVertexBuffer[3] = sx1;
	gVertexBuffer[4] = sy0;
	gVertexBuffer[5] = 0.0f;
	gVertexBuffer[6] = sx0;
	gVertexBuffer[7] = sy1;
	gVertexBuffer[8] = 0.0f;
	gVertexBuffer[9] = sx1;
	gVertexBuffer[10] = sy1;
	gVertexBuffer[11] = 0.0f;
	gTexCoordBuffer[0] = u0 * scale_x;
	gTexCoordBuffer[1] = v0 * scale_y;
	gTexCoordBuffer[2] = u1 * scale_x;
	gTexCoordBuffer[3] = v0 * scale_y;
	gTexCoordBuffer[4] = u0 * scale_x;
	gTexCoordBuffer[5] = v1 * scale_y;
	gTexCoordBuffer[6] = u1 * scale_x;
	gTexCoordBuffer[7] = v1 * scale_y;
	vglVertexPointerMapped(3, gVertexBuffer);
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gVertexBuffer += 12;
	gTexCoordBuffer += 8;
	
	SetNegativeViewport();
	
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
}

void RendererLegacy::Draw2DTextureR(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2,
								 f32 y2, f32 x3, f32 y3, f32 s, f32 t)
{
	const CNativeTexture *texture = mBoundTexture[0];
	if (!texture) return;
	
	//if ((gRDPOtherMode.L & 0xFFFFFF00) == 0x0C184200) CDebugConsole::Get()->Msg(1, "Draw2DTextureR: L: 0x%08X", gRDPOtherMode.L);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((float*)mScreenToDevice.mRaw);
	
	// Enable or Disable ZBuffer test
	if ((mTnL.Flags.Zbuffer && gRDPOtherMode.z_cmp) || gRDPOtherMode.z_upd)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
	
	glDepthMask(gRDPOtherMode.z_upd ? GL_TRUE : GL_FALSE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	float scale_x = texture->GetScaleX();
	float scale_y = texture->GetScaleY();

	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	gVertexBuffer[0] = LightN64ToScreenX(x0);
	gVertexBuffer[1] = LightN64ToScreenY(y0);
	gVertexBuffer[2] = 0.0f;
	gVertexBuffer[3] = LightN64ToScreenX(x1);
	gVertexBuffer[4] = LightN64ToScreenY(y1);
	gVertexBuffer[5] = 0.0f;
	gVertexBuffer[6] = LightN64ToScreenX(x2);
	gVertexBuffer[7] = LightN64ToScreenY(y2);
	gVertexBuffer[8] = 0.0f;
	gVertexBuffer[9] = LightN64ToScreenX(x3);
	gVertexBuffer[10] = LightN64ToScreenY(y3);
	gVertexBuffer[11] = 0.0f;
	gTexCoordBuffer[0] = 0.0f;
	gTexCoordBuffer[1] = 0.0f;
	gTexCoordBuffer[2] = s * scale_x;
	gTexCoordBuffer[3] = 0.0f;
	gTexCoordBuffer[4] = s * scale_x;
	gTexCoordBuffer[5] = t * scale_y;
	gTexCoordBuffer[6] = 0.0f;
	gTexCoordBuffer[7] = t * scale_y;
	vglVertexPointerMapped(3, gVertexBuffer);
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gVertexBuffer += 12;
	gTexCoordBuffer += 8;
	
	SetNegativeViewport();
	
	vglDrawObjects(GL_TRIANGLE_FAN, 4, GL_TRUE);
}

uint32_t RendererLegacy::PrepareTrisUnclipped(uint32_t **clr)
{
	const u32		num_vertices = mNumIndices;

	//
	//	Now we just shuffle all the data across directly (potentially duplicating verts)
	//
	vglVertexPointerMapped(3, gVertexBuffer);
	*clr = gColorBuffer;
	if (mTnL.Flags.Texture) {
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		vglTexCoordPointerMapped(gTexCoordBuffer);
		
		if (g_ROM.TEXELS_HACK && ((gRDPOtherMode.L >= 0x0C184000 && gRDPOtherMode.L <= 0x0C184FFF) || gRDPOtherMode.L == 0xC8104A50)) {
			UpdateTileSnapshots( mTextureTile + 1 );
		} else UpdateTileSnapshots( mTextureTile );
		
		CNativeTexture *texture = mBoundTexture[0];
		
		//if ((gRDPOtherMode.L & 0xFFFFFF00) == 0x0C184200) CDebugConsole::Get()->Msg(1, "RenderTriangles: L: 0x%08X", gRDPOtherMode.L);
		
		if( texture && (mTnL.Flags._u32 & (TNL_LIGHT|TNL_TEXGEN)) != (TNL_LIGHT|TNL_TEXGEN) )
		{
			float scale_x = texture->GetScaleX();
			float scale_y = texture->GetScaleY();
				
			// Hack to fix the sun in Zelda OOT/MM
			if (g_ROM.ZELDA_HACK && (gRDPOtherMode.L == 0x0C184241))
			{
				scale_x *= 0.5f;
				scale_y *= 0.5f;
			}

			for( u32 i = 0; i < num_vertices; ++i )
			{
				u32 index = mIndexBuffer[ i ];
		
				sceClibMemcpy(gVertexBuffer, mVtxProjected[ index ].TransformedPos.f, sizeof(float) * 3);
				gTexCoordBuffer[0] = (mVtxProjected[ index ].Texture.x * scale_x - (mTileTopLeft[ 0 ].s  / 4.f * scale_x));
				gTexCoordBuffer[1] = (mVtxProjected[ index ].Texture.y * scale_y - (mTileTopLeft[ 0 ].t  / 4.f * scale_y));
				gColorBuffer[i] = c32(mVtxProjected[ index ].Colour).GetColour();
				gVertexBuffer += 3;
				gTexCoordBuffer += 2;
			}
		} else {
			for( u32 i = 0; i < num_vertices; ++i )
			{
				u32 index = mIndexBuffer[ i ];
		
				sceClibMemcpy(gVertexBuffer, mVtxProjected[ index ].TransformedPos.f, sizeof(float) * 3);
				sceClibMemcpy(gTexCoordBuffer, mVtxProjected[ index ].Texture.f, sizeof(float) * 2);
				gColorBuffer[i] = c32(mVtxProjected[ index ].Colour).GetColour();
				gVertexBuffer += 3;
				gTexCoordBuffer += 2;
			}
		}
	} else {
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		for( u32 i = 0; i < num_vertices; ++i )
		{
			u32 index = mIndexBuffer[ i ];
		
			sceClibMemcpy(gVertexBuffer, mVtxProjected[ index ].TransformedPos.f, sizeof(float) * 3);
			gColorBuffer[i] = c32(mVtxProjected[ index ].Colour).GetColour();
			gVertexBuffer += 3;
		}
	}
	gColorBuffer += num_vertices;
	return num_vertices;
}

bool CreateRendererLegacy()
{
	gRendererLegacy = new RendererLegacy();
	gRenderer    = gRendererLegacy;
	return true;
}

void DestroyRendererLegacy()
{
	delete gRendererLegacy;
	gRendererLegacy = nullptr;
	gRenderer    = nullptr;
}
