
#include "Base/Types.h"

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
#include "HLEGraphics/DisplayListDebugger.h"

#include "SysPosix/Debug/WebDebug.h"
#include "SysPosix/Debug/WebDebugTemplate.h"

#include "Graphics/GraphicsContext.h"
#include "Graphics/NativeTexture.h"
#include "Graphics/PngUtil.h"

#include "HLEGraphics/BaseRenderer.h"
#include "HLEGraphics/DLDebug.h"
#include "HLEGraphics/DLParser.h"
#include "HLEGraphics/RDP.h"
#include "HLEGraphics/RDPStateManager.h"
#include "HLEGraphics/TextureCache.h"

#include "System/Condition.h"
#include "Utility/StringUtil.h"
#include "System/Thread.h"
#include "System/Mutex.h"

#if defined(DAEDALUS_GL)
#include "SysGL/GL.h"
#elif defined(DAEDALUS_GLES)
#include "SysGLES/GL.h"
#endif

static bool gDebugging = false;


// These coordinate requests between the web threads and the main thread (only the main thread can call OpenGL functions)
static Cond * gMainThreadCond   = NULL;
static Mutex * gMainThreadMutex = NULL;

enum DebugTask
{
	kTaskUndefined,
	kBreakExecution,
	kResumeExecution,
	kTaskScrub,
	kTaskTakeScreenshot,
	kTaskDumpDList,
};

static WebDebugConnection * gActiveConnection = NULL;
static DebugTask			gDebugTask        = kTaskUndefined;
static u32					gInstructionCountLimit = kUnlimitedInstructionCount;

static void Base64Encode(const void * data, size_t len, DataSink * sink)
{
	const char * table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	const u8 * b = static_cast<const u8 *>(data);
	const u8 * e = b + len;

	size_t out_len = ((len+2)/3) * 4;
	char * buffer = static_cast<char *>(malloc( out_len ));
	char * dst = buffer;

	const u8 * src = b;
	for ( ; src + 2 < e; src += 3)
	{
		u32 v = (src[0] << 16) | (src[1] << 8) | src[2];

		dst[0] = table[(v >> 18) & 0x3f];
		dst[1] = table[(v >> 12) & 0x3f];
		dst[2] = table[(v >>  6) & 0x3f];
		dst[3] = table[(v >>  0) & 0x3f];

		dst += 4;
	}

	if (src < e)
	{
		u32 c0 = src[0];
		u32 c1 = src+1 < e ? src[1] : 0;
		u32 c2 = src+2 < e ? src[2] : 0;

		u32 v = (c0 << 16) | (c1 << 8) | c2;

		dst[0] =             table[(v >> 18) & 0x3f];
		dst[1] =             table[(v >> 12) & 0x3f];
		dst[2] = src+1 < e ? table[(v >>  6) & 0x3f] : '=';
		dst[3] = src+2 < e ? table[(v >>  0) & 0x3f] : '=';

		dst += 4;
	}

	DAEDALUS_ASSERT(dst == buffer+out_len, "Oops");
	sink->Write(buffer, out_len);
	free(buffer);
}

class HTMLDebugOutput : public DLDebugOutput
{
public:
	explicit HTMLDebugOutput(WebDebugConnection * connection)
	:	Connection(connection)
	{
	}

	virtual size_t Write(const void * p, size_t len)
	{
		return Connection->Write(p, len);
	}

	virtual void BeginInstruction(u32 idx, u32 cmd0, u32 cmd1, u32 depth, const char * name)
	{
		Print("<span class=\"hle-instr\" id=\"I%d\">", idx);
		Print("%05d %08x%08x %*s%-10s\n", idx, cmd0, cmd1, depth*2, "", name);
		Print("<span class=\"hle-detail\" style=\"display:none\">");
	}

	virtual void EndInstruction()
	{
		Print("</span></span>");
	}

	WebDebugConnection * Connection;
};

bool DLDebugger_IsDebugging()
{
	return gDebugging;
}

void DLDebugger_RequestDebug()
{
	gDebugging = true;
}


static void EncodeTexture(const std::shared_ptr<CNativeTexture> texture, DataSink * sink)
{
	u32 width  = texture->GetWidth();
	u32 height = texture->GetHeight();
	size_t num_bytes = width * 4 * height;
	u8 * bytes = static_cast<u8 * >(malloc(num_bytes));

	FlattenTexture(texture, bytes, num_bytes);

	Base64Encode(bytes, num_bytes, sink);
	free(bytes);
}

void DLDebugger_ProcessDebugTask()
{
	// Check if a web request is waiting for a screenshot.
	// FIXME: MutexLock
	gMainThreadMutex->Lock();
	if (WebDebugConnection * connection = gActiveConnection)
	{
		bool handled = false;
		switch (gDebugTask)
		{
			case kTaskUndefined:
				break;
			case kBreakExecution:
			{
				u32 num_ops = DLParser_Process(kUnlimitedInstructionCount, NULL);
				gInstructionCountLimit = num_ops;

				connection->BeginResponse(200, -1, kApplicationJSON);
				connection->WriteF("{\"num_ops\":%d}", num_ops);
				connection->EndResponse();
				gDebugging = true;
				handled = true;
				break;
			}
			case kResumeExecution:
			{
				gInstructionCountLimit = kUnlimitedInstructionCount;
				connection->BeginResponse(200, -1, kTextPlain);
				connection->WriteString("ok");
				connection->EndResponse();
				gDebugging = false;
				handled = true;
				break;
			}
			case kTaskScrub:
			{
				DLParser_Process(gInstructionCountLimit, NULL);

				u64 mux = gRenderer->GetMux();
				u32 mux_hi = mux >> 32;
				u32 mux_lo = mux & 0xffffffff;

				connection->BeginResponse(200, -1, kApplicationJSON);
				connection->WriteString("{\n");
				connection->WriteF("\t\"combine\": {\"hi\": \"0x%08x\", \"lo\": \"0x%08x\"},\n",
					mux_hi, mux_lo);
				connection->WriteF("\t\"rdpOtherModeH\": \"0x%08x\",\n", gRDPOtherMode.H);
				connection->WriteF("\t\"rdpOtherModeL\": \"0x%08x\",\n", gRDPOtherMode.L);
				connection->WriteF("\t\"fillColor\": \"0x%08x\",\n", gRenderer->GetFillColour());	// FIXME: this is usually 16-bit
				connection->WriteF("\t\"envColor\": \"0x%08x\",\n", gRenderer->GetEnvColour().GetColour());
				connection->WriteF("\t\"primColor\": \"0x%08x\",\n", gRenderer->GetPrimitiveColour().GetColour());
				connection->WriteF("\t\"blendColour\": \"0x%08x\",\n", gRenderer->GetBlendColour().GetColour());
				connection->WriteF("\t\"fogColor\": \"0x%08x\",\n", gRenderer->GetFogColour().GetColour());
				connection->WriteString("\t\"tiles\": [\n");
				for (u32 i = 0; i < 8; ++i)
				{
					if (i > 0)
						connection->WriteString(",\n");

					const RDP_Tile &     tile      = gRDPStateManager.GetTile(i);
					const RDP_TileSize & tile_size = gRDPStateManager.GetTileSize(i);

					connection->WriteString("\t{\n");
					connection->WriteF("\t\t\"format\": %d,\n",   tile.format);
					connection->WriteF("\t\t\"size\": %d,\n",     tile.size);
					connection->WriteF("\t\t\"line\": %d,\n",     tile.line);
					connection->WriteF("\t\t\"tmem\": %d,\n",     tile.tmem);
					connection->WriteF("\t\t\"palette\": %d,\n",  tile.palette);
					connection->WriteF("\t\t\"clamp_s\": %d,\n",  tile.clamp_s);
					connection->WriteF("\t\t\"mirror_s\": %d,\n", tile.mirror_s);
					connection->WriteF("\t\t\"mask_s\": %d,\n",   tile.mask_s);
					connection->WriteF("\t\t\"shift_s\": %d,\n",  tile.shift_s);
					connection->WriteF("\t\t\"clamp_t\": %d,\n",  tile.clamp_t);
					connection->WriteF("\t\t\"mirror_t\": %d,\n", tile.mirror_t);
					connection->WriteF("\t\t\"mask_t\": %d,\n",   tile.mask_t);
					connection->WriteF("\t\t\"shift_t\": %d,\n",  tile.shift_t);
					connection->WriteF("\t\t\"left\": %d,\n",     tile_size.left);
					connection->WriteF("\t\t\"top\": %d,\n",      tile_size.top);
					connection->WriteF("\t\t\"right\": %d,\n",    tile_size.right);
					connection->WriteF("\t\t\"bottom\": %d,\n",   tile_size.bottom);

					bool wrote_data = false;
					if ((tile.format != 0 || tile.size != 0) &&
						tile_size.GetWidth() > 0 &&
						tile_size.GetHeight() > 0)
					{
						const TextureInfo &  ti = gRDPStateManager.GetUpdatedTextureDescriptor(i);
						std::shared_ptr<CNativeTexture> texture = CTextureCache::Get()->GetOrCreateTexture(ti);
						if (texture)
						{
							connection->WriteString("\t\t\"texture\": {\n");
							connection->WriteF("\t\t\t\"width\": %d,\n", texture->GetWidth());
							connection->WriteF("\t\t\t\"height\": %d,\n", texture->GetHeight());
							connection->WriteString("\t\t\t\"data\": \"");
							EncodeTexture(texture, connection);
							connection->WriteString("\"\n");
							connection->WriteString("\t\t}\n");
							wrote_data = true;
						}
					}

					if (!wrote_data)
					{
						connection->WriteString("\t\t\"texture\": {}\n");
					}


					connection->WriteString("\t}");
				}
				connection->WriteString("]");
				connection->WriteString("}");
				connection->EndResponse();
				handled = true;
				break;
			}
			case kTaskTakeScreenshot:
			{
				connection->BeginResponse(200, -1, kImagePng);

				u32 width;
				u32 height;
				CGraphicsContext::Get()->GetScreenSize(&width, &height);

				// Make the BYTE array, factor of 3 because it's RBG.
				void * pixels = malloc( 4 * width * height );

				glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

				// NB, pass a negative pitch, to render the screenshot the right way up.
				s32 pitch = -static_cast<s32>(width * 4);

				PngSaveImage(connection, pixels, NULL, TexFmt_8888, pitch, width, height, false);

				free(pixels);

				connection->EndResponse();
				handled = true;
				break;
			}
			case kTaskDumpDList:
			{
				connection->BeginResponse(200, -1, kTextPlain);

				HTMLDebugOutput		dl_output(connection);

				DLParser_Process(kUnlimitedInstructionCount, &dl_output);
				connection->EndResponse();
				handled = true;
				break;
			}
		}

		if (!handled)
		{
			Generate500(connection, "Unhandled DebugTask");
		}
		gActiveConnection = NULL;
		gDebugTask = kTaskUndefined;

	}
	gMainThreadMutex->Unlock();
	CondSignal(gMainThreadCond);
}

bool DLDebugger_Process()
{
	DLDebugger_ProcessDebugTask();

	while(gDebugging)
	{
		DLParser_Process(gInstructionCountLimit, NULL);
		DLDebugger_ProcessDebugTask();

		CGraphicsContext::Get()->UpdateFrame( false );

		// FIXME: shouldn't need to do this, just wake up when there's incoming WebDebug request.
		ThreadSleepMs(10);
	}

	return false;
}

static void DoTask(WebDebugConnection * connection, DebugTask task)
{
	// Request a screenshot from the main thread.
	MutexLock lock(gMainThreadMutex);

	gActiveConnection = connection;
	gDebugTask        = task;
	CondWait(gMainThreadCond, gMainThreadMutex, kTimeoutInfinity);
}

static void DLDebugHandler(void * arg [[maybe_unused]], WebDebugConnection * connection)
{
	const WebDebugConnection::QueryParams & params = connection->GetQueryParams();
	if (!params.empty())
	{
		bool ok = false;

		for (size_t i = 0; i < params.size(); ++i)
		{
			if (params[i].Key == "action")
			{
				if (params[i].Value == "break")
				{
					DoTask(connection, kBreakExecution);
					return;
				}
				else if (params[i].Value == "resume")
				{
					DoTask(connection, kResumeExecution);
					return;
				}
			}
			else if (params[i].Key == "scrub")
			{
				gInstructionCountLimit = ParseU32(params[i].Value, 10);
				DoTask(connection, kTaskScrub);
				return;
			}
			else if (params[i].Key == "screen")
			{
				//int cmd = atoi(params[i].Value);
				DoTask(connection, kTaskTakeScreenshot);
				return;
			}
			else if (params[i].Key == "dump")
			{
				//int cmd = atoi(params[i].Value);
				DoTask(connection, kTaskDumpDList);
				return;
			}
		}

		// Fallthrough for handlers that just return 'ok'.
		connection->BeginResponse(ok ? 200 : 400, -1, kTextPlain);
		connection->WriteString(ok ? "ok\n" : "fail\n");
		connection->EndResponse();
		return;
	}

	if (!ServeResource(connection, "/html/dldebugger.html"))
	{
		Generate500(connection, "Couldn't load html/debugger.html");
	}
}

bool DLDebugger_RegisterWebDebug()
{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	WebDebug_Register( "/dldebugger", &DLDebugHandler, NULL );
#endif
	gMainThreadCond = CondCreate();
	gMainThreadMutex = new Mutex();
	return true;
}

#endif // DAEDALUS_DEBUG_DISPLAYLIST
