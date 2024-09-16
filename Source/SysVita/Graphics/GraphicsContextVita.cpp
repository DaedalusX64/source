#include "stdafx.h"
#include "Graphics/GraphicsContext.h"

#include <psp2/gxm.h>
#include <psp2/display.h>

#include <vitaGL.h>

#include "Config/ConfigOptions.h"
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "Debug/Dump.h"
#include "Graphics/ColourValue.h"
#include "Graphics/PngUtil.h"
#include "Utility/IO.h"
#include "Utility/Preferences.h"
#include "Utility/Profiler.h"
#include "Utility/VolatileMem.h"
#include "SysVita/UI/Menu.h"

#include "HLEGraphics/BaseRenderer.h"

extern bool pause_emu;
bool gWaitRendering = false;

static int frame_reset_counter = 0;
#define FRAME_RESET_NUM 8

#define MAX_INDEXES 0xFFFF
uint16_t *gIndexes;
float *gVertexBuffer;
uint32_t *gColorBuffer;
float *gTexCoordBuffer;
float *gVertexBufferPtr;
uint32_t *gColorBufferPtr;
float *gTexCoordBufferPtr;
bool new_frame = true;

extern int time_unif;
extern float gamma_val;

static GLuint emu_fb = 0xDEADBEEF, emu_fb_tex, emu_depth_buf_tex;

class IGraphicsContext : public CGraphicsContext
{
public:
	IGraphicsContext();
	virtual ~IGraphicsContext();

	bool				Initialise();
	bool				IsInitialised() const { return mInitialised; }

	void				SwitchToChosenDisplay();
	void				SwitchToLcdDisplay();
	void				StoreSaveScreenData();

	void				ClearAllSurfaces();

	void				ClearToBlack();
	void				ClearZBuffer();
	void				ClearColBuffer(const c32 &colour);
	void				ClearColBufferAndDepth(const c32 &colour);

	void				BeginFrame();
	void				EndFrame();
	void				UpdateFrame(bool wait_for_vbl);
	void				GetScreenSize(u32 * width, u32 * height) const;

	void				SetDebugScreenTarget( ETargetSurface buffer );

	void				ViewportType(u32 *d_width, u32 *d_height) const;
	void				DumpScreenShot();
	void				DumpNextScreen()			{ mDumpNextScreen = 2; }

private:
	void				SaveScreenshot(const char* filename, s32 x, s32 y, u32 width, u32 height);

private:
	bool				mInitialised;

	u32					mDumpNextScreen;
};

//*************************************************************************************
//
//*************************************************************************************
template<> bool CSingleton< CGraphicsContext >::Create()
{
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT_Q(mpInstance == nullptr);
#endif
	mpInstance = new IGraphicsContext();
	return mpInstance->Initialise();
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IGraphicsContext::IGraphicsContext()
	:	mInitialised(false)
	,	mDumpNextScreen(false)
{	
	uint16_t i;
	gIndexes = (uint16_t*)malloc(sizeof(uint16_t)*MAX_INDEXES);
	for (i = 0; i < MAX_INDEXES; i++){
		gIndexes[i] = i;
	}
	gVertexBufferPtr = (float*)malloc(0x1000000);
	gColorBufferPtr = (uint32_t*)malloc(0xC00000);
	gTexCoordBufferPtr = (float*)malloc(0xC00000);
	gVertexBuffer = gVertexBufferPtr;
	gColorBuffer = gColorBufferPtr;
	gTexCoordBuffer = gTexCoordBufferPtr;
}

IGraphicsContext::~IGraphicsContext()
{
	vglEnd();
}

bool IGraphicsContext::Initialise()
{
	mInitialised = true;
	return true;
}

void IGraphicsContext::ClearAllSurfaces()
{
}

void IGraphicsContext::ClearToBlack()
{
	glDepthMask(GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth( 1.0f );
	glClear((GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
}

void IGraphicsContext::ClearZBuffer()
{
	glDepthMask(GL_TRUE);
	glClearDepth( 1.0f );
	glClear( GL_DEPTH_BUFFER_BIT );
}

void IGraphicsContext::ClearColBuffer(const c32 & colour)
{
	glClearColor( colour.GetRf(), colour.GetGf(), colour.GetBf(), colour.GetAf() );
	glClear( GL_COLOR_BUFFER_BIT );
}

void IGraphicsContext::ClearColBufferAndDepth(const c32 & colour)
{
	glDepthMask(GL_TRUE);
	glClearDepth( 1.0f );
	glClearColor( colour.GetRf(), colour.GetGf(), colour.GetBf(), colour.GetAf() );
	glClear((GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
}

void IGraphicsContext::BeginFrame()
{
	if (gPostProcessing) {
		if (emu_fb == 0xDEADBEEF) {
			glGenTextures(1, &emu_fb_tex);
			glBindTexture(GL_TEXTURE_2D, emu_fb_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			/*glActiveTexture(GL_TEXTURE1);
			glGenTextures(1, &emu_depth_buf_tex);
			glBindTexture(GL_TEXTURE_2D, emu_depth_buf_tex);
			vglTexImageDepthBuffer(GL_TEXTURE_2D);
			glActiveTexture(GL_TEXTURE0);*/
			glGenFramebuffers(1, &emu_fb);
			glBindFramebuffer(GL_FRAMEBUFFER, emu_fb);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, emu_fb_tex, 0);
		} else glBindFramebuffer(GL_FRAMEBUFFER, emu_fb);
	} else glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (g_ROM.CLEAR_SCENE_HACK) ClearColBuffer( c32(0xff000000) );
	glEnableClientState(GL_VERTEX_ARRAY);
	frame_reset_counter = (frame_reset_counter + 1) % FRAME_RESET_NUM;
	if (!frame_reset_counter) {
		gVertexBuffer = gVertexBufferPtr;
		gColorBuffer = gColorBufferPtr;
		gTexCoordBuffer = gTexCoordBufferPtr;
	}
	vglIndexPointerMapped(gIndexes);
	
	if (new_frame) {
		CGraphicsContext::Get()->ClearToBlack();
		new_frame = false;
	}
}

void IGraphicsContext::EndFrame()
{
	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	glScissor( 0, 0, SCR_WIDTH, SCR_HEIGHT);
	glUseProgram(0);
	if (gamma_val != 1.0f) gRenderer->DoGamma(gamma_val);
	if (!gPostProcessing) {
		if (gOverlay) {
			glBindTexture(GL_TEXTURE_2D, cur_overlay);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, 960, 544, 0, -1, 1);
			gRenderer->DrawUITexture();
		}
		DrawInGameMenu();
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	if (!pause_emu) glFlush();
	if (gWaitRendering) glFinish();
	DrawPendingDialog();
}

void IGraphicsContext::UpdateFrame(bool wait_for_vbl)
{
	if (gPostProcessing && emu_fb != 0xDEADBEEF) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, 960, 544, 0, -1, 1);
		glBindTexture(GL_TEXTURE_2D, emu_fb_tex);
		glUseProgram(cur_prog);
			
		int i = 0;
		while (prog_uniforms[i].idx != 0xDEADBEEF) {
			switch (prog_uniforms[i].type) {
			case UNIF_FLOAT:
				glUniform1f(prog_uniforms[i].idx, prog_uniforms[i].value[0]);
				break;
			case UNIF_COLOR:
				glUniform3fv(prog_uniforms[i].idx, 1, prog_uniforms[i].value);
				break;
			default:
				break;
			}
			i++;
		}
		if (time_unif != -1) {
			glUniform1f(time_unif, (float)sceKernelGetProcessTimeLow() / 1000000.0f);
		}
			
		vglVertexAttribPointerMapped(0, vflux_vertices);
		vglVertexAttribPointerMapped(1, vflux_texcoords);
		vglDrawObjects(GL_TRIANGLE_FAN, 4, true);
		glUseProgram(0);
		glEnableClientState(GL_VERTEX_ARRAY);
		if (gOverlay) {
			glBindTexture(GL_TEXTURE_2D, cur_overlay);
			gRenderer->DrawUITexture();
		}
		DrawInGameMenu();
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	vglSwapBuffers(GL_FALSE);
	new_frame = true;
	if (pause_emu) {
		BeginFrame();
		EndFrame();
		UpdateFrame(false);
	}
}

void IGraphicsContext::SetDebugScreenTarget(ETargetSurface buffer)
{
}

void IGraphicsContext::ViewportType(u32 *d_width, u32 *d_height) const
{
	GetScreenSize(d_width, d_height);
}

void IGraphicsContext::SaveScreenshot(const char* filename, s32 x, s32 y, u32 width, u32 height)
{
}

void IGraphicsContext::DumpScreenShot()
{
}

void IGraphicsContext::StoreSaveScreenData()
{
}

void IGraphicsContext::GetScreenSize(u32 * p_width, u32 * p_height) const
{
	// Note: Change these if you change SCR_WIDTH/SCR_HEIGHT
	switch (gAspectRatio) {
	case RATIO_4_3:
		*p_width  = 725;
		*p_height = SCR_HEIGHT;
		break;
	case RATIO_ORIG:
		*p_width  = 640;
		*p_height = 480;
		break;
	default:
		*p_width  = SCR_WIDTH;
		*p_height = SCR_HEIGHT;
		break;
	}
}
