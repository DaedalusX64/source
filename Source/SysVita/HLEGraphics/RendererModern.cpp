#include "stdafx.h"
#include "RendererModern.h"

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

#include "SysVita/HLEGraphics/FragmentShader.h"
#include "SysVita/UI/Menu.h"

#define MODERN_SHADER_MAGIC 1

#define NORMALIZE_C1842XX(x) ((x) > 16.5f ? ((x) / ((x) / 16.0f)) : (x))

void *vertex_shader_bin = nullptr;
int vertex_shader_size = 0;
extern BaseRenderer *gRenderer;
RendererModern  *gRendererModern = nullptr;

extern float *gVertexBuffer;
extern uint32_t *gColorBuffer;
extern float *gTexCoordBuffer;
extern u32 aux_draws;
extern u32 aux_discard;

static const u32 kNumTextures = 2;
static bool use_texture_scale = false;

extern bool gUseMipmaps;

char cache_name[256];

struct ScePspFMatrix4
{
	float m[16];
};


extern ScePspFMatrix4		gProjection;
extern void sceGuSetMatrix(int type, const ScePspFMatrix4 * mtx);

// This defines all the state that is expressed by a given shader.
// If any of these fields change, it requires building a different shader.
struct ShaderConfiguration
{
	u64		Mux;
	u32		CycleType : 2;
	u32		HasFog : 1;
	float	AlphaThreshold;
};


inline bool operator==(const ShaderConfiguration & a, const ShaderConfiguration & b)
{
	return
		a.Mux            == b.Mux &&
		a.CycleType      == b.CycleType &&
		a.HasFog         == b.HasFog &&
		a.AlphaThreshold == b.AlphaThreshold;
}

struct ShaderProgram
{
	ShaderConfiguration config;
	GLuint 				program;

	GLint				uloc_primcol;
	GLint				uloc_envcol;
	GLint				uloc_primlodfrac;
	GLint				uloc_fognear;
	GLint				uloc_fogfar;
	GLint				uloc_fogcolor;
	GLint				uloc_texscale_x;
	GLint				uloc_texscale_y;
};
static std::vector<ShaderProgram *>		gShaders;

/* Creates a shader object of the specified type using the specified text
 */
static GLuint make_shader(GLenum type, const char** lines, size_t num_lines)
{
	GLuint shader = glCreateShader(type);
	if (shader != 0)
	{
		int32_t body_size = 0;
		for (int i = 0; i < num_lines; i++) {
			body_size += strlen(lines[i]);
		}
		char *body = (char*)malloc(body_size + 1);
		memset(body, 0, body_size + 1);
		strcpy(body, lines[0]);
		for (int i = 1; i < num_lines; i++) {
			strcat(body, lines[i]);
		}
		
		body_size = strlen(body) - 1;
		glShaderSource(shader, 1, &body, &body_size);
		glCompileShader(shader);
		
		GLint shader_ok;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
		if (shader_ok != GL_TRUE)
		{
			DBGConsole_Msg(0, "ERROR: Failed to compile %s shader.\n", (type == GL_FRAGMENT_SHADER) ? "fragment" : "vertex");
			glDeleteShader(shader);
			shader = 0;
		}
		
		free(body);
	}
	return shader;
}

/* Creates a program object using the specified vertex and fragment text
 */
static GLuint make_shader_program(const char ** vertex_lines, size_t num_vertex_lines,
								  const char ** fragment_lines, size_t num_fragment_lines)
{
	GLuint program = 0u;
	GLuint vertex_shader = 0u;
	GLuint fragment_shader = 0u;

	if (!vertex_shader_bin) {
		vertex_shader = make_shader(GL_VERTEX_SHADER, vertex_lines, num_vertex_lines);
		vertex_shader_bin = malloc(16 * 1024);
		vglGetShaderBinary(vertex_shader, 16 * 1024, &vertex_shader_size, vertex_shader_bin);
		FILE *f = fopen(DAEDALUS_VITA_PATH("ShaderCache/vert.gxp"), "wb");
		fwrite(vertex_shader_bin, 1, vertex_shader_size, f);
		fclose(f);
	} else {
		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderBinary(1, &vertex_shader, 0, vertex_shader_bin, vertex_shader_size);
	}

	fragment_shader = make_shader(GL_FRAGMENT_SHADER, fragment_lines, num_fragment_lines);

	void *bin = malloc(16 * 1024);
	GLsizei bin_len;
	vglGetShaderBinary(fragment_shader, 16 * 1024, &bin_len, bin);
	FILE *f = fopen(cache_name, "wb");
	fwrite(bin, 1, bin_len, f);
	fclose(f);
	free(bin);
			
	/* make the program that connect the two shader and link it */
	program = glCreateProgram();

	/* attach both shader and link */
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
				
	vglBindAttribLocation(program, 0, "in_pos",  3,         GL_FLOAT);
	vglBindAttribLocation(program, 1, "in_uv" ,  2,         GL_FLOAT);
	vglBindAttribLocation(program, 2, "in_col",  4, GL_UNSIGNED_BYTE);

	glLinkProgram(program);

	return program;
}

static const char * kRGBParams32[] =
{
	"combined.rgb",  "tex0.rgb",
	"tex1.rgb",      "prim.rgb",
	"shade.rgb",     "env.rgb",
	"one.rgb",       "combined.a",
	"tex0.a",        "tex1.a",
	"prim.a",        "shade.a",
	"env.a",         "lod_frac",
	"prim_lod_frac", "k5",
	"?",             "?",
	"?",             "?",
	"?",             "?",
	"?",             "?",
	"?",             "?",
	"?",             "?",
	"?",             "?",
	"?",             "zero.rgb",
};

static const char * kRGBParams16[] = {
	"combined.rgb", "tex0.rgb",
	"tex1.rgb",     "prim.rgb",
	"shade.rgb",    "env.rgb",
	"one.rgb",      "combined.a",
	"tex0.a",       "tex1.a",
	"prim.a",       "shade.a",
	"env.a",        "lod_frac",
	"prim_lod_frac", "zero.rgb",
};

static const char * kRGBParams8[8] = {
	"combined.rgb", "tex0.rgb",
	"tex1.rgb",     "prim.rgb",
	"shade.rgb",    "env.rgb",
	"one.rgb",      "zero.rgb",
};

static const char * kAlphaParams8[8] = {
	"combined.a", "tex0.a",
	"tex1.a",     "prim.a",
	"shade.a",    "env.a",
	"one.a",      "zero.a"
};

static const char* default_vertex_shader =
"void main(\n"
"	float3 in_pos,\n"
"	float2 in_uv,\n"
"	float4 in_col,\n"
"	uniform float4x4 gl_ModelViewProjectionMatrix,\n"
"	uniform float tex_scale_x,\n"
"	uniform float tex_scale_y,\n"
"	float2 out sti : TEXCOORD0,\n"
"	float4 out v_col : COLOR0,\n"
"	float4 out v_pos : POSITION)\n"
"{\n"
"	sti = float2(in_uv.x * tex_scale_x, in_uv.y * tex_scale_y);\n"
"	v_col = in_col;\n"
"	v_pos = mul(gl_ModelViewProjectionMatrix, float4(in_pos, 1.0));\n"
"}\n";

// FIXME(strmnnrmn): texel fetch filter changes between cycles.
static const char* default_fragment_shader_fmt =
"void main(float2 sti : TEXCOORD0,\n"
"	float4 v_col : COLOR0,\n"
"	float4 coords : WPOS,\n"
"	uniform sampler2D uTexture0 : TEXUNIT0,\n"
"	uniform sampler2D uTexture1 : TEXUNIT1,\n"
"	uniform float fog_near,\n"
"	uniform float fog_far,\n"
"	uniform float4 fog_color,\n"
"	uniform float4 uPrimColour,\n"
"	uniform float4 uEnvColour,\n"
"	uniform float uPrimLODFrac,\n"
"	float4 out fragcol : COLOR)\n"
"{\n"
"	float4 shade = v_col;\n"
"	float4 prim  = uPrimColour;\n"
"	float4 env   = uEnvColour;\n"
"	float4 one   = float4(1,1,1,1);\n"
"	float4 zero  = float4(0,0,0,0);\n"
"	float4 col;\n"
"	float4 combined = float4(0,0,0,1);\n"
"	float lod_frac      = 0.0;		// FIXME\n"
"	float prim_lod_frac = uPrimLODFrac;\n"
"	float k5            = 0.0;		// FIXME\n"
"%s		// Body is injected here\n"
"	fragcol = col;\n"
"}\n";

static void SprintShader(char (&frag_shader)[2048], const ShaderConfiguration & config)
{
	u32 mux0 = (u32)(config.Mux>>32);
	u32 mux1 = (u32)(config.Mux);

	u32 aRGB0  = (mux0>>20)&0x0F;	// c1 c1		// a0
	u32 bRGB0  = (mux1>>28)&0x0F;	// c1 c2		// b0
	u32 cRGB0  = (mux0>>15)&0x1F;	// c1 c3		// c0
	u32 dRGB0  = (mux1>>15)&0x07;	// c1 c4		// d0

	u32 aA0    = (mux0>>12)&0x07;	// c1 a1		// Aa0
	u32 bA0    = (mux1>>12)&0x07;	// c1 a2		// Ab0
	u32 cA0    = (mux0>>9 )&0x07;	// c1 a3		// Ac0
	u32 dA0    = (mux1>>9 )&0x07;	// c1 a4		// Ad0

	u32 aRGB1  = (mux0>>5 )&0x0F;	// c2 c1		// a1
	u32 bRGB1  = (mux1>>24)&0x0F;	// c2 c2		// b1
	u32 cRGB1  = (mux0    )&0x1F;	// c2 c3		// c1
	u32 dRGB1  = (mux1>>6 )&0x07;	// c2 c4		// d1

	u32 aA1    = (mux1>>21)&0x07;	// c2 a1		// Aa1
	u32 bA1    = (mux1>>3 )&0x07;	// c2 a2		// Ab1
	u32 cA1    = (mux1>>18)&0x07;	// c2 a3		// Ac1
	u32 dA1    = (mux1    )&0x07;	// c2 a4		// Ad1

	char body[2048];

	u32 cycle_type = config.CycleType;

	if (cycle_type == CYCLE_FILL)
	{
		strcpy(body, "\tcol = shade;\n");
	}
	else if (cycle_type == CYCLE_COPY)
	{
		strcpy(body, "\tcol = fetchTexel(sti, uTexture0);\n");
	}
	else if (cycle_type == CYCLE_1CYCLE)
	{
		sprintf(body, "\tfloat4 tex0 = fetchTexel(sti, uTexture0);\n"
					  "\tfloat4 tex1 = fetchTexel(sti, uTexture1);\n"
					  "\tcol.rgb = (%s - %s) * %s + %s;\n"
					  "\tcol.a   = (%s - %s) * %s + %s;\n",
					  kRGBParams16[aRGB0], kRGBParams16[bRGB0], kRGBParams32[cRGB0], kRGBParams8[dRGB0],
					  kAlphaParams8[aA0],  kAlphaParams8[bA0],  kAlphaParams8[cA0],  kAlphaParams8[dA0]);
	}
	else
	{
		sprintf(body, "\tfloat4 tex0 = fetchTexel(sti, uTexture0);\n"
					  "\tfloat4 tex1 = fetchTexel(sti, uTexture1);\n"
					  "\tcol.rgb = (%s - %s) * %s + %s;\n"
					  "\tcol.a   = (%s - %s) * %s + %s;\n"
					  "\tcombined = col;\n"
					  "\ttex0 = tex1;\n"		// NB: tex0 becomes tex1 on the second cycle - see mame.
					  "\tcol.rgb = (%s - %s) * %s + %s;\n"
					  "\tcol.a   = (%s - %s) * %s + %s;\n",
					  kRGBParams16[aRGB0], kRGBParams16[bRGB0], kRGBParams32[cRGB0], kRGBParams8[dRGB0],
					  kAlphaParams8[aA0],  kAlphaParams8[bA0],  kAlphaParams8[cA0],  kAlphaParams8[dA0],
					  kRGBParams16[aRGB1], kRGBParams16[bRGB1], kRGBParams32[cRGB1], kRGBParams8[dRGB1],
					  kAlphaParams8[aA1],  kAlphaParams8[bA1],  kAlphaParams8[cA1],  kAlphaParams8[dA1]);
	}

	if (config.AlphaThreshold > 0)
	{
		char * p = body + strlen(body);
		sprintf(p, "\tif (col.a < %f) discard;\n", config.AlphaThreshold);
	}
	
	if (config.HasFog)
	{
		char * p = body + strlen(body);
		sprintf(p, "%s", "\tfloat vFog = clamp((fog_far - coords.z) / (fog_far - fog_near), 0.0, 1.0);\n\tcol.rgb = lerp(fog_color.rgb, col.rgb, vFog);\n");
	}

	sprintf(frag_shader, default_fragment_shader_fmt, body);
}

static void InitShaderProgram(ShaderProgram * program, const ShaderConfiguration & config, GLuint shader_program)
{
	program->config            = config;
	program->program           = shader_program;

	program->uloc_primcol      = glGetUniformLocation(shader_program, "uPrimColour");
	program->uloc_envcol       = glGetUniformLocation(shader_program, "uEnvColour");
	program->uloc_primlodfrac  = glGetUniformLocation(shader_program, "uPrimLODFrac");
	program->uloc_fognear      = glGetUniformLocation(shader_program, "fog_near");
	program->uloc_fogfar       = glGetUniformLocation(shader_program, "fog_far");
	program->uloc_fogcolor     = glGetUniformLocation(shader_program, "fog_color");
	program->uloc_texscale_x   = glGetUniformLocation(shader_program, "tex_scale_x");
	program->uloc_texscale_y   = glGetUniformLocation(shader_program, "tex_scale_y");
}

float RendererModern::CalculateAlphaThreshold() const
{
	// Initiate Alpha test
	if( (gRDPOtherMode.alpha_compare == G_AC_THRESHOLD) && !gRDPOtherMode.alpha_cvg_sel )
	{
		u8 alpha_threshold = mBlendColour.GetA();
		return (alpha_threshold || g_ROM.ALPHA_HACK) ? mBlendColour.GetAf() - 0.001f : mBlendColour.GetAf();
	}
	else if (gRDPOtherMode.cvg_x_alpha)
	{
		return 0.4392f;
	}
	else
	{
		// Use CVG for pixel alpha
		return 0.0f;
	}
}

void RendererModern::MakeShaderConfigFromCurrentState(ShaderConfiguration * config) const
{
	config->CycleType = gRDPOtherMode.cycle_type;
	
	switch (config->CycleType) {
	case CYCLE_FILL:
		config->Mux = 0;
		config->AlphaThreshold = 0;
		break;
	case CYCLE_COPY:
		config->Mux = 0;
		config->AlphaThreshold = CalculateAlphaThreshold();
		break;
	default:
		config->Mux = mMux;
		config->AlphaThreshold = CalculateAlphaThreshold();
		break;
	}

	config->HasFog = mTnL.Flags.Fog;
}

static ShaderProgram * GetShaderForConfig(const ShaderConfiguration & config)
{
	for (u32 i = 0; i < gShaders.size(); ++i)
	{
		ShaderProgram * program = gShaders[i];
		if (program->config == config)
			return program;
	}

	sprintf(cache_name, "%s%016llX%08X%08X%008X-%d.gxp",
		DAEDALUS_VITA_PATH("ShaderCache/"), config.Mux, config.CycleType, config.HasFog, *(uint32_t *)(&config.AlphaThreshold), MODERN_SHADER_MAGIC);
	GLuint shader_program;
	FILE *f = fopen(cache_name, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		int bin_size = ftell(f);
		void *bin = malloc(bin_size);
		fseek(f, 0, SEEK_SET);
		fread(bin, 1, bin_size, f);
		fclose(f);
		GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderBinary(1, &fragment_shader, 0, bin, bin_size);
		free(bin);
		
		GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		if (!vertex_shader_bin) {
			f = fopen(DAEDALUS_VITA_PATH("ShaderCache/vert.gxp"), "rb");
			fseek(f, 0, SEEK_END);
			vertex_shader_size = ftell(f);
			vertex_shader_bin = malloc(vertex_shader_size);
			fseek(f, 0, SEEK_SET);
			fread(vertex_shader_bin, 1, vertex_shader_size, f);
			fclose(f);
		}
		glShaderBinary(1, &vertex_shader, 0, vertex_shader_bin, vertex_shader_size);
		shader_program = glCreateProgram();
		glAttachShader(shader_program, vertex_shader);
		glAttachShader(shader_program, fragment_shader);
				
		vglBindAttribLocation(shader_program, 0, "in_pos",  3,         GL_FLOAT);
		vglBindAttribLocation(shader_program, 1, "in_uv" ,  2,         GL_FLOAT);
		vglBindAttribLocation(shader_program, 2, "in_col",  4, GL_UNSIGNED_BYTE);

		glLinkProgram(shader_program);
	} else {
		char frag_shader[2048];
		SprintShader(frag_shader, config);

		const char * vertex_lines[] = { default_vertex_shader };
		const char * fragment_lines[] = { gN64FragmentLibrary, frag_shader };

		shader_program = make_shader_program(
			vertex_lines, ARRAYSIZE(vertex_lines),
			fragment_lines, ARRAYSIZE(fragment_lines));
	}

	glUniform1i(glGetUniformLocation(shader_program, "uTexture0"), 0);
	glUniform1i(glGetUniformLocation(shader_program, "uTexture1"), 1);

	ShaderProgram * program = new ShaderProgram;
	InitShaderProgram(program, config, shader_program);
	gShaders.push_back(program);

	return program;
}

void RendererModern::RestoreRenderStates()
{
	// Initialise the device to our default state

	// No fog
	glDisable(GL_FOG);

	// We do our own culling
	glDisable(GL_CULL_FACE);

	u32 width, height;
	CGraphicsContext::Get()->GetScreenSize(&width, &height);

	glScissor(0,0, width, height);
	glEnable(GL_SCISSOR_TEST);

	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable( GL_BLEND );

	// Default is ZBuffer disabled
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_DEPTH_TEST);

	// Enable this for rendering decals (glPolygonOffset).
	glEnable(GL_POLYGON_OFFSET_FILL);
}

void RendererModern::RenderDaedalusVtxStreams(int prim, const float * positions, const float * uvs, const u32 * colours, int count)
{
	vglVertexAttribPointerMapped(0, positions);
	if (uvs) vglVertexAttribPointerMapped(1, uvs);
	vglVertexAttribPointerMapped(2, colours);
	vglDrawObjects(prim, count, GL_TRUE);
}

/*
Possible Blending Inputs:
    In  -   Input from color combiner
    Mem -   Input from current frame buffer
    Fog -   Fog generator
    BL  -   Blender
Possible Blending Factors:
    A-IN    -   Alpha from color combiner
    A-MEM   -   Alpha from current frame buffer
    (1-A)   -
    A-FOG   -   Alpha of fog color
    A-SHADE -   Alpha of shade
    1   -   1
    0   -   0
*/

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

inline u32 MakeMask(u32 m)
{
	return m ? ((1<<m)-1) : 0xffffffff;
}

inline u32 MakeMirror(u32 mirror, u32 m)
{
	return (mirror && m) ? (1<<m) : 0;
}

void RendererModern::PrepareRenderState(const float (&mat_project)[16], bool disable_zbuffer)
{
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
		if ( (mTnL.Flags.Zbuffer & gRDPOtherMode.z_cmp) | gRDPOtherMode.z_upd )
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		glDepthMask(gRDPOtherMode.z_upd ? GL_TRUE : GL_FALSE);
	}


	u32 cycle_mode = gRDPOtherMode.cycle_type;

	// Initiate Blender
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
		glDisable(GL_BLEND);
	}

	ShaderConfiguration config;
	MakeShaderConfigFromCurrentState(&config);

	const ShaderProgram * program = GetShaderForConfig(config);
	if (program == NULL)
	{
		// There must have been some failure to compile the shader. Abort!
		DBGConsole_Msg(0, "Couldn't generate a shader for mux %llx, cycle %d, alpha %d\n", config.Mux, config.CycleType, config.AlphaThreshold);
		return;
	}

	glUseProgram(program->program);
	
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((float*)mat_project);

	glUniform4f(program->uloc_primcol, mPrimitiveColour.GetRf(), mPrimitiveColour.GetGf(), mPrimitiveColour.GetBf(), mPrimitiveColour.GetAf());
	glUniform4f(program->uloc_envcol,  mEnvColour.GetRf(),       mEnvColour.GetGf(),       mEnvColour.GetBf(),       mEnvColour.GetAf());
	glUniform1f(program->uloc_primlodfrac, mPrimLODFraction);
	
	if (mTnL.Flags.Fog) {
		glUniform1f(program->uloc_fognear, mFogNear);
		glUniform1f(program->uloc_fogfar, mFogFar);
		glUniform4f(program->uloc_fogcolor, mFogColour.GetRf(), mFogColour.GetGf(), mFogColour.GetBf(), mFogColour.GetAf());
	}
	
	CNativeTexture * texture = mBoundTexture[0];
	if (use_texture_scale && texture != NULL) {
		
		float scale_x = texture->GetScaleX();
		float scale_y = texture->GetScaleY();
		
		//printf("PrepareRenderState: %X\n", gRDPOtherMode.L);
		if (g_ROM.ZELDA_HACK && (gRDPOtherMode.L == 0x0C184241)) { // Hack to fix the sun in Zelda OOT/MM
			scale_x *= 0.5f;
			scale_y *= 0.5f;
		} else if (g_ROM.TEXELS_HACK && (gRDPOtherMode.L == 0x0C184244)) { // Hack to fix texts on Rayman 2/Donald Duck Quack Attack
			scale_x *= 0.125f;
			scale_y *= 0.125f;
		}
		
		glUniform1f(program->uloc_texscale_x, scale_x);
		glUniform1f(program->uloc_texscale_y, scale_y);
	} else {
		glUniform1f(program->uloc_texscale_x, 1.0f);
		glUniform1f(program->uloc_texscale_y, 1.0f);
	}
	
	// Second texture is sampled in 2 cycle mode if text_lod is clear (when set,
	// gRDPOtherMode.text_lod enables mipmapping, but we just set lod_frac to 0.
	bool use_t1 = cycle_mode == CYCLE_2CYCLE;

	bool install_textures[] = { true, use_t1 };

	for (u32 i = 0; i < kNumTextures; ++i)
	{
		if (!install_textures[i])
			continue;

		texture = mBoundTexture[i];

		if (texture != NULL)
		{
			glActiveTexture(GL_TEXTURE0 + i);

			texture->InstallTexture();

			if (((gRDPOtherMode.text_filt != G_TF_POINT) && cycle_mode != CYCLE_COPY) || (gGlobalPreferences.ForceLinearFilter))
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mTexWrap[i].u);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mTexWrap[i].v);
		}
	}
	
	glActiveTexture(GL_TEXTURE0);
}

// FIXME(strmnnrmn): for fill/copy modes this does more work than needed.
// It ends up copying colour/uv coords when not needed, and can use a shader uniform for the fill colour.
void RendererModern::RenderTriangles( uint32_t * p_vertices, u32 num_vertices, bool disable_zbuffer )
{
	PrepareRenderState(gProjection.m, disable_zbuffer);
	SetPositiveViewport();
	vglVertexAttribPointerMapped(2, p_vertices);
	vglDrawObjects(GL_TRIANGLES, num_vertices, GL_TRUE);
}

void RendererModern::TexRect( u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1 )
{
	if (g_ROM.GameHacks == POKEMON_STADIUM) {
		if (aux_draws) {
			aux_draws--;
			if ((aux_draws < aux_discard) && (xy0.x == 52.0f)) return;
			SetN64Viewport( aux_scale, aux_trans );
		}
	}
	
	use_texture_scale = true;

	UpdateTileSnapshots( tile_idx );

	PrepareTexRectUVs(&st0, &st1);
	PrepareRenderState(mScreenToDevice.mRaw, gRDPOtherMode.depth_source ? false : true);

	v2 uv0( (float)st0.s / 32.f, (float)st0.t / 32.f );
	v2 uv1( (float)st1.s / 32.f, (float)st1.t / 32.f );

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
	
	float *uvs = gTexCoordBuffer;
	//printf("TexRect: %X\n", gRDPOtherMode.L);
	if (g_ROM.TEXELS_HACK && (gRDPOtherMode.L == 0x0C184244)) {
		gTexCoordBuffer[0] = NORMALIZE_C1842XX(uv0.x);
		gTexCoordBuffer[1] = NORMALIZE_C1842XX(uv0.y);
		gTexCoordBuffer[2] = NORMALIZE_C1842XX(uv1.x);
		gTexCoordBuffer[3] = NORMALIZE_C1842XX(uv0.y);
		gTexCoordBuffer[4] = NORMALIZE_C1842XX(uv0.x);
		gTexCoordBuffer[5] = NORMALIZE_C1842XX(uv1.y);
		gTexCoordBuffer[6] = NORMALIZE_C1842XX(uv1.x);
		gTexCoordBuffer[7] = NORMALIZE_C1842XX(uv1.y);	
	} else {
		gTexCoordBuffer[0] = uv0.x;
		gTexCoordBuffer[1] = uv0.y;
		gTexCoordBuffer[2] = uv1.x;
		gTexCoordBuffer[3] = uv0.y;
		gTexCoordBuffer[4] = uv0.x;
		gTexCoordBuffer[5] = uv1.y;
		gTexCoordBuffer[6] = uv1.x;
		gTexCoordBuffer[7] = uv1.y;
	}
	gTexCoordBuffer += 8;
	
	float *positions = gVertexBuffer;
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
	gVertexBuffer += 12;
	
	uint32_t *colours = gColorBuffer;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = 0xFFFFFFFF;
	gColorBuffer += 4;
	
	SetNegativeViewport();
	
	RenderDaedalusVtxStreams(GL_TRIANGLE_STRIP, positions, uvs, colours, 4);
}

void RendererModern::TexRectFlip( u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1 )
{
	UpdateTileSnapshots( tile_idx );
	PrepareTexRectUVs(&st0, &st1);
	
	v2 uv0( (float)st0.s / 32.f, (float)st0.t / 32.f );
	v2 uv1( (float)st1.s / 32.f, (float)st1.t / 32.f );
	
	use_texture_scale = true;

	PrepareRenderState(mScreenToDevice.mRaw, gRDPOtherMode.depth_source ? false : true);

	v2 screen0;
	v2 screen1;
	ScaleN64ToScreen( xy0, screen0 );
	ScaleN64ToScreen( xy1, screen1 );

	float *uvs = gTexCoordBuffer;
	gTexCoordBuffer[0] = uv0.x;
	gTexCoordBuffer[1] = uv0.y;
	gTexCoordBuffer[2] = uv0.x;
	gTexCoordBuffer[3] = uv1.y;
	gTexCoordBuffer[4] = uv1.x;
	gTexCoordBuffer[5] = uv0.y;
	gTexCoordBuffer[6] = uv1.x;
	gTexCoordBuffer[7] = uv1.y;
	gTexCoordBuffer += 8;
	
	float *positions = gVertexBuffer;
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
	gVertexBuffer += 12;
	
	uint32_t *colours = gColorBuffer;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = 0xFFFFFFFF;
	gColorBuffer += 4;
	
	SetNegativeViewport();
	
	RenderDaedalusVtxStreams(GL_TRIANGLE_STRIP, positions, uvs, colours, 4);
}

void RendererModern::FillRect( const v2 & xy0, const v2 & xy1, u32 color )
{
	PrepareRenderState(mScreenToDevice.mRaw, gRDPOtherMode.depth_source ? false : true);

	v2 screen0;
	v2 screen1;
	ScaleN64ToScreen( xy0, screen0 );
	ScaleN64ToScreen( xy1, screen1 );
	
	use_texture_scale = false;

	float *positions = gVertexBuffer;
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
	gVertexBuffer += 12;

	uint32_t *colours = gColorBuffer;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = color;
	gColorBuffer += 4;

	RenderDaedalusVtxStreams(GL_TRIANGLE_STRIP, positions, nullptr, colours, 4);
}

void RendererModern::DoGamma(float gamma)
{
	glUseProgram(0);
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

void RendererModern::DrawUITexture()
{
	glUseProgram(0);
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
	
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
}

void RendererModern::Draw2DTexture(f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1)
{
	const CNativeTexture * texture = mBoundTexture[0];
	if (!texture) return;
	
	gRDPOtherMode.cycle_type = CYCLE_COPY;

	PrepareRenderState(mScreenToDevice.mRaw, false);

	glEnable(GL_BLEND);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	use_texture_scale = true;
	
	float sx0 = LightN64ToScreenX(x0);
	float sy0 = LightN64ToScreenY(y0);

	float sx1 = LightN64ToScreenX(x1);
	float sy1 = LightN64ToScreenY(y1);

	const f32 depth = 0.0f;
	
	float *positions = gVertexBuffer;
	float *uvs = gTexCoordBuffer;
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
	gTexCoordBuffer[0] = u0;
	gTexCoordBuffer[1] = v0;
	gTexCoordBuffer[2] = u1;
	gTexCoordBuffer[3] = v0;
	gTexCoordBuffer[4] = u0;
	gTexCoordBuffer[5] = v1;
	gTexCoordBuffer[6] = u1;
	gTexCoordBuffer[7] = v1;
	gVertexBuffer += 12;
	gTexCoordBuffer += 8;

	uint32_t *colours = gColorBuffer;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = 0xFFFFFFFF;
	gColorBuffer += 4;
	
	SetNegativeViewport();

	RenderDaedalusVtxStreams(GL_TRIANGLE_STRIP, positions, uvs, colours, 4);
}

void RendererModern::Draw2DTextureR(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, f32 s, f32 t)
{
	const CNativeTexture * texture = mBoundTexture[0];
	if (!texture) return;
	
	gRDPOtherMode.cycle_type = CYCLE_COPY;

	PrepareRenderState(mScreenToDevice.mRaw, false);

	glEnable(GL_BLEND);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	use_texture_scale = true;

	float *positions = gVertexBuffer;
	float *uvs = gTexCoordBuffer;
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
	gTexCoordBuffer[2] = s;
	gTexCoordBuffer[3] = 0.0f;
	gTexCoordBuffer[4] = s;
	gTexCoordBuffer[5] = t;
	gTexCoordBuffer[6] = 0.0f;
	gTexCoordBuffer[7] = t;

	uint32_t *colours = gColorBuffer;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = 0xFFFFFFFF;
	gColorBuffer += 4;
	
	SetNegativeViewport();

	RenderDaedalusVtxStreams(GL_TRIANGLE_FAN, positions, uvs, colours, 4);
}

uint32_t RendererModern::PrepareTrisUnclipped(uint32_t **clr)
{
	const u32		num_vertices = mNumIndices;

	//
	//	Now we just shuffle all the data across directly (potentially duplicating verts)
	//
	vglVertexAttribPointerMapped(0, gVertexBuffer);
	*clr = gColorBuffer;
	if (mTnL.Flags.Texture) {
		vglVertexAttribPointerMapped(1, gTexCoordBuffer);
		
		//printf("PrepareTrisUnclipped: %X\n", gRDPOtherMode.L);
		if (g_ROM.TEXELS_HACK && ((gRDPOtherMode.L >= 0x0C184000 && gRDPOtherMode.L <= 0x0C184FFF) || gRDPOtherMode.L == 0xC8104A50)) {
			UpdateTileSnapshots( mTextureTile + 1 );
		} else UpdateTileSnapshots( mTextureTile );
		
		CNativeTexture *texture = mBoundTexture[0];
		
		//if ((gRDPOtherMode.L & 0xFFFFFF00) == 0x0C184200) CDebugConsole::Get()->Msg(1, "RenderTriangles: L: 0x%08X", gRDPOtherMode.L);
		
		if (texture && (mTnL.Flags._u32 & (TNL_LIGHT|TNL_TEXGEN)) != (TNL_LIGHT|TNL_TEXGEN) ) {
			use_texture_scale = true;

			for( u32 i = 0; i < num_vertices; ++i )
			{
				u32 index = mIndexBuffer[ i ];
				
				sceClibMemcpy(gVertexBuffer, mVtxProjected[ index ].TransformedPos.f, sizeof(float) * 3);
				gTexCoordBuffer[0] = (mVtxProjected[ index ].Texture.x - (mTileTopLeft[ 0 ].s  / 4.f));
				gTexCoordBuffer[1] = (mVtxProjected[ index ].Texture.y - (mTileTopLeft[ 0 ].t  / 4.f));
				gColorBuffer[i] = c32(mVtxProjected[ index ].Colour).GetColour();
				gVertexBuffer += 3;
				gTexCoordBuffer += 2;
			}
		} else {
			use_texture_scale = false;
			
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

bool CreateRendererModern()
{
	gRendererModern = new RendererModern();
	gRenderer   = gRendererModern;
	return true;
}
void DestroyRendererModern()
{
	delete gRendererModern;
	gRendererModern = NULL;
	gRenderer   = NULL;
}
