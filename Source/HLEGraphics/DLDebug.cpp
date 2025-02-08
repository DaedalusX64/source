
#include "Base/Types.h"
#include "HLEGraphics/DLDebug.h"

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
#include <stdarg.h>
#if defined(GCC_LEGACY)
#include <fmt/core.h> 
#else 
#include <format>
#endif
#include <filesystem>
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "Debug/Dump.h"
#include "HLEGraphics/RDP.h"
#include "Ultra/ultra_gbi.h"

#include "Base/Macros.h"


DLDebugOutput * gDLDebugOutput = nullptr;

void DLDebug_SetOutput(DLDebugOutput * output)
{
	gDLDebugOutput = output;
}

DLDebugOutput::~DLDebugOutput()
{
}

void DLDebugOutput::PrintLine(const char * fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	// I've never been confident that this returns a sane value across platforms.
	/*len = */vsnprintf( mBuffer, kBufferLen, fmt, va );
	va_end(va);

	// This should be guaranteed...
	mBuffer[kBufferLen-1] = 0;
	size_t len = strlen(mBuffer);

	// Append a newline, if there's space in the buffer.
	if (len < kBufferLen)
	{
		mBuffer[len] = '\n';
		++len;
	}

	Write(mBuffer, len);
}

void DLDebugOutput::Print(const char * fmt, ...)
{
	//char buffer[kBufferLen];

	va_list va;
	va_start(va, fmt);
	// I've never been confident that this returns a sane value across platforms.
	/*len = */vsnprintf( mBuffer, kBufferLen, fmt, va );
	va_end(va);


	// This should be guaranteed...
	mBuffer[kBufferLen-1] = 0;
	size_t len = strlen(mBuffer);

	Write(mBuffer, len);
}

static const char * const kMulInputRGB[32] =
{
	"Combined    ", "Texel0      ",
	"Texel1      ", "Primitive   ",
	"Shade       ", "Env         ",
	"KeyScale    ", "CombinedAlph",
	"Texel0_Alpha", "Texel1_Alpha",
	"Prim_Alpha  ", "Shade_Alpha ",
	"Env_Alpha   ", "LOD_Frac    ",
	"PrimLODFrac ", "K5          ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ",	"0           "
};

static const char * const kSubAInputRGB[16] =
{
	"Combined    ", "Texel0      ",
	"Texel1      ", "Primitive   ",
	"Shade       ", "Env         ",
	"1           ", "Noise       ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
};

static const char * const kSubBInputRGB[16] =
{
	"Combined    ", "Texel0      ",
	"Texel1      ", "Primitive   ",
	"Shade       ", "Env         ",
	"KeyCenter   ", "K4          ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
	"0           ", "0           ",
};

static const char * const kAddInputRGB[8] =
{
	"Combined    ", "Texel0      ",
	"Texel1      ", "Primitive   ",
	"Shade       ", "Env         ",
	"1           ", "0           ",
};

static const char * const kSubInputAlpha[8] =
{
	"Combined    ", "Texel0      ",
	"Texel1      ", "Primitive   ",
	"Shade       ", "Env         ",
	"1           ", "0           ",
};

static const char * const kMulInputAlpha[8] =
{
	"LOD_Frac    ", "Texel0      ",
	"Texel1      ", "Primitive   ",
	"Shade       ", "Env         ",
	"PrimLOD_Frac", "0           ",
};

static const char * const kAddInputAlpha[8] =
{
	"Combined    ", "Texel0      ",
	"Texel1      ", "Primitive   ",
	"Shade       ", "Env         ",
	"1           ", "0           ",
};


void DLDebug_DumpMux( u64 mux )
{
	u32 mux0 = (u32)(mux>>32);
	u32 mux1 = (u32)(mux);

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

	DL_PF("    Mux: 0x%08x%08x", mux0, mux1);

	DL_PF("    RGB0: (%s - %s) * %s + %s", kSubAInputRGB[aRGB0], kSubBInputRGB[bRGB0], kMulInputRGB[cRGB0], kAddInputRGB[dRGB0]);
	DL_PF("    A0  : (%s - %s) * %s + %s", kSubInputAlpha[aA0],  kSubInputAlpha[bA0],  kMulInputAlpha[cA0], kAddInputAlpha[dA0]);
	DL_PF("    RGB1: (%s - %s) * %s + %s", kSubAInputRGB[aRGB1], kSubBInputRGB[bRGB1], kMulInputRGB[cRGB1], kAddInputRGB[dRGB1]);
	DL_PF("    A1  : (%s - %s) * %s + %s", kSubInputAlpha[aA1],  kSubInputAlpha[bA1],  kMulInputAlpha[cA1], kAddInputAlpha[dA1]);
}

void DLDebug_PrintMux( FILE * fh, u64 mux )
{
	u32 mux0 = (u32)(mux>>32);
	u32 mux1 = (u32)(mux);

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

	fprintf(fh, "//case 0x%08x%08xLL:\n", mux0, mux1);
	fprintf(fh, "//aRGB0: (%s - %s) * %s + %s\n", kSubAInputRGB[aRGB0], kSubBInputRGB[bRGB0], kMulInputRGB[cRGB0], kAddInputRGB[dRGB0]);
	fprintf(fh, "//aA0  : (%s - %s) * %s + %s\n", kSubInputAlpha[aA0],  kSubInputAlpha[bA0],  kMulInputAlpha[cA0], kAddInputAlpha[dA0]);
	fprintf(fh, "//aRGB1: (%s - %s) * %s + %s\n", kSubAInputRGB[aRGB1], kSubBInputRGB[bRGB1], kMulInputRGB[cRGB1], kAddInputRGB[dRGB1]);
	fprintf(fh, "//aA1  : (%s - %s) * %s + %s\n", kSubInputAlpha[aA1],  kSubInputAlpha[bA1],  kMulInputAlpha[cA1], kAddInputAlpha[dA1]);
	fprintf(fh, "void BlendMode_0x%08x%08xLL( BLEND_MODE_ARGS )\n{\n}\n\n", mux0, mux1);
}

static const char * const kBlendCl[]				= { "In",  "Mem",  "Bl",     "Fog" };
static const char * const kBlendA1[]				= { "AIn", "AFog", "AShade", "0" };
static const char * const kBlendA2[]				= { "1-A", "AMem", "1",      "?" };

static const char * const kAlphaCompareValues[]		= {"None", "Threshold", "?", "Dither"};
static const char * const kDepthSourceValues[]		= {"Pixel", "Primitive"};

static const char * const kCvgDestValues [[maybe_unused]] [] 			= {"Clamp", "Wrap", "Full", "Save"};
static const char * const kZModeValues [[maybe_unused]] []			= {"Opa", "Inter", "XLU", "Decal"};

static const char * const kAlphaDitherValues[]		= {"Pattern", "NotPattern", "Noise", "Disable"};
static const char * const kRGBDitherValues[]		= {"MagicSQ", "Bayer", "Noise", "Disable"};
static const char * const kCombKeyValues[]			= {"None", "Key"};
static const char * const kTextureConvValues[]		= {"Conv", "?", "?", "?",   "?", "FiltConv", "Filt", "?"};
static const char * const kTextureFilterValues[]	= {"Point", "?", "Bilinear", "Average"};
static const char * const kTextureLUTValues[]		= {"None", "?", "RGBA16", "IA16"};
static const char * const kTextureLODValues[]		= {"Tile", "LOD"};
static const char * const kTextureDetailValues[]	= {"Clamp", "Sharpen", "Detail", "?"};
static const char * const kCycleTypeValues[]		= {"1Cycle", "2Cycle", "Copy", "Fill"};
static const char * const kPipelineValues[]			= {"NPrimitive", "1Primitive"};

static const char * const kOnOffValues[]            = {"Off", "On"};

struct OtherModeData
{
	const char *			Name;
	u32						Bits;
	u32						Shift;
	const char * const *	Values;
	void					(*Fn)(u32);		// Custom function
};

static void DumpRenderMode(u32 data);
static void DumpBlender(u32 data);

static const OtherModeData kOtherModeLData[] = {
	{ "alpha_compare", 2, G_MDSFT_ALPHACOMPARE,		kAlphaCompareValues, nullptr  },
	{ "depth_source",  1, G_MDSFT_ZSRCSEL,			kDepthSourceValues, nullptr},

#if 0
	// G_MDSFT_RENDERMODE
	{ "aa_en",         1, 3,						kOnOffValues },
	{ "z_cmp",         1, 4,						kOnOffValues },
	{ "z_upd",         1, 5,						kOnOffValues },
	{ "im_rd",         1, 6,						kOnOffValues },
	{ "clr_on_cvg",    1, 7,						kOnOffValues },
	{ "cvg_dst",       2, 8,						kCvgDestValues },
	{ "zmode",         2, 10,						kZModeValues },
	{ "cvg_x_alpha",   1, 12,						kOnOffValues },
	{ "alpha_cvg_sel", 1, 13,						kOnOffValues },
	{ "force_bl",      1, 14,						kOnOffValues },
	{ "tex_edge",      1, 15,						kOnOffValues },
#else
	{ "render_mode",   13, G_MDSFT_RENDERMODE,		nullptr, &DumpRenderMode },
#endif

	{ "blender",      16, G_MDSFT_BLENDER,			nullptr, &DumpBlender }, // Custom output
};

static const OtherModeData kOtherModeHData[] = {
	{ "blend_mask",    4, G_MDSFT_BLENDMASK,		nullptr, nullptr },
	{ "alpha_dither",  2, G_MDSFT_ALPHADITHER,		kAlphaDitherValues, nullptr  },
	{ "rgb_dither",    2, G_MDSFT_RGBDITHER,		kRGBDitherValues, nullptr  },
	{ "comb_key",      1, G_MDSFT_COMBKEY,			kCombKeyValues, nullptr  },
	{ "text_conv",     3, G_MDSFT_TEXTCONV,			kTextureConvValues, nullptr  },
	{ "text_filt",     2, G_MDSFT_TEXTFILT,			kTextureFilterValues, nullptr  },
	{ "text_tlut",     2, G_MDSFT_TEXTLUT,			kTextureLUTValues, nullptr  },
	{ "text_lod",      1, G_MDSFT_TEXTLOD,			kTextureLODValues, nullptr  },
	{ "text_detail",   2, G_MDSFT_TEXTDETAIL,		kTextureDetailValues, nullptr  },
	{ "text_persp",    1, G_MDSFT_TEXTPERSP,		kOnOffValues, nullptr  },
	{ "cycle_type",    2, G_MDSFT_CYCLETYPE,		kCycleTypeValues, nullptr  },
	{ "color_dither",  1, G_MDSFT_COLORDITHER,		nullptr, nullptr  },
	{ "pipeline",      1, G_MDSFT_PIPELINE,			kPipelineValues, nullptr  },
};

static const u32 kOtherModeLabelWidth = 15;

static void DumpOtherMode(const OtherModeData * table, u32 table_len, u32 * mask_, u32 * data_)
{
	u32 mask = *mask_;
	u32 data = *data_;

	const char padstr[] = "                    ";

	for (u32 i = 0; i < table_len; ++i)
	{
		const OtherModeData & e = table[i];

		u32 mode_mask = ((1 << e.Bits)-1) << e.Shift;

		if ((mask & mode_mask) == mode_mask)
		{
			u32 val = (data & mode_mask) >> e.Shift;

			s32 pad = kOtherModeLabelWidth - (strlen(e.Name) + 1);
			if (e.Values)
			{
				DL_PF("    %s:%.*s%s", e.Name, pad, padstr, e.Values[val]);
			}
			else if (e.Fn)
			{
				e.Fn(data);	// NB pass unshifted value.
			}
			else
			{
				DL_PF("    %s:%.*s%d", e.Name, pad, padstr, val);
			}

			mask &= ~mode_mask;
			data &= ~mode_mask;
		}
	}

	*mask_ = mask;
	*data_ = data;
}

// Slightly nicer output of rendermode, as a single line
static void DumpRenderMode(u32 data)
{
	char buf[256] = "";
	if (data & AA_EN)               strcat(buf, "|AA_EN");
	if (data & Z_CMP)               strcat(buf, "|Z_CMP");
	if (data & Z_UPD)               strcat(buf, "|Z_UPD");
	if (data & IM_RD)               strcat(buf, "|IM_RD");
	if (data & CLR_ON_CVG)          strcat(buf, "|CLR_ON_CVG");

	u32 cvg = data & 0x0300;
		 if (cvg == CVG_DST_CLAMP)  strcat(buf, "|CVG_DST_CLAMP");
	else if (cvg == CVG_DST_WRAP)   strcat(buf, "|CVG_DST_WRAP");
	else if (cvg == CVG_DST_FULL)   strcat(buf, "|CVG_DST_FULL");
	else if (cvg == CVG_DST_SAVE)   strcat(buf, "|CVG_DST_SAVE");

	u32 zmode = data & 0x0c00;
		 if (zmode == ZMODE_OPA)    strcat(buf, "|ZMODE_OPA");
	else if (zmode == ZMODE_INTER)  strcat(buf, "|ZMODE_INTER");
	else if (zmode == ZMODE_XLU)    strcat(buf, "|ZMODE_XLU");
	else if (zmode == ZMODE_DEC)    strcat(buf, "|ZMODE_DEC");

	if (data & CVG_X_ALPHA)         strcat(buf, "|CVG_X_ALPHA");
	if (data & ALPHA_CVG_SEL)       strcat(buf, "|ALPHA_CVG_SEL");
	if (data & FORCE_BL)            strcat(buf, "|FORCE_BL");

	char * p = buf;
	if (*p)
		++p;		// Skip '|'
	else
		strcpy(p, "0");

	DL_PF("    render_mode:   %s", p);
}

static void DumpBlender(u32 data)
{
	u32 blender = data >> G_MDSFT_BLENDER;

	u32 m1a_1 = (blender >>14) & 0x3;
	u32 m1b_1 = (blender >>10) & 0x3;
	u32 m2a_1 = (blender >> 6) & 0x3;
	u32 m2b_1 = (blender >> 2) & 0x3;

	u32 m1a_2 = (blender >>12) & 0x3;
	u32 m1b_2 = (blender >> 8) & 0x3;
	u32 m2a_2 = (blender >> 4) & 0x3;
	u32 m2b_2 = (blender     ) & 0x3;

	DL_PF("    blender:       0x%04x - %s*%s + %s*%s | %s*%s + %s*%s",
		blender,
		kBlendCl[m1a_1], kBlendA1[m1b_1], kBlendCl[m2a_1], kBlendA2[m2b_1],
		kBlendCl[m1a_2], kBlendA1[m1b_2], kBlendCl[m2a_2], kBlendA2[m2b_2]);
}

void DLDebug_DumpRDPOtherMode(const RDP_OtherMode & mode)
{
	if (DLDebug_IsActive())
	{
		u32 mask = 0xffffffff;
		u32 data = mode.L;
		DumpOtherMode(kOtherModeLData, std::size(kOtherModeLData), &mask, &data);

		mask = 0xffffffff;
		data = mode.H;
		DumpOtherMode(kOtherModeHData, std::size(kOtherModeHData), &mask, &data);
	}
}

void DLDebug_DumpRDPOtherModeL(u32 mask, u32 data)
{
	if (DLDebug_IsActive())
	{
		DumpOtherMode(kOtherModeLData, std::size(kOtherModeLData), &mask, &data);

		// Just check we're not handling some unusual calls.
		DAEDALUS_ASSERT(mask == 0, "OtherModeL mask is non zero: %08x", mask);
		DAEDALUS_ASSERT(data == 0, "OtherModeL data is non zero: %08x", data);
	}
}

void DLDebug_DumpRDPOtherModeH(u32 mask, u32 data)
{
	if (DLDebug_IsActive())
	{
		DumpOtherMode(kOtherModeHData, std::size(kOtherModeHData), &mask, &data);

		// Just check we're not handling some unusual calls.
		DAEDALUS_ASSERT(mask == 0, "OtherModeH mask is non zero: %08x", mask);
		DAEDALUS_ASSERT(data == 0, "OtherModeH data is non zero: %08x", data);
	}
}

void DLDebug_DumpTaskInfo( const OSTask * pTask )
{
	DL_PF( "Task:         %08x",      pTask->t.type  );
	DL_PF( "Flags:        %08x",      pTask->t.flags  );
	DL_PF( "BootCode:     %08x", (u32)pTask->t.ucode_boot  );
	DL_PF( "BootCodeSize: %08x",      pTask->t.ucode_boot_size  );

	DL_PF( "uCode:        %08x", (u32)pTask->t.ucode );
	DL_PF( "uCodeSize:    %08x",      pTask->t.ucode_size );
	DL_PF( "uCodeData:    %08x", (u32)pTask->t.ucode_data );
	DL_PF( "uCodeDataSize:%08x",      pTask->t.ucode_data_size );

	DL_PF( "Stack:        %08x", (u32)pTask->t.dram_stack );
	DL_PF( "StackS:       %08x",      pTask->t.dram_stack_size );
	DL_PF( "Output:       %08x", (u32)pTask->t.output_buff );
	DL_PF( "OutputS:      %08x", (u32)pTask->t.output_buff_size );

	DL_PF( "Data( PC ):   %08x", (u32)pTask->t.data_ptr );
	DL_PF( "DataSize:     %08x",      pTask->t.data_size );
	DL_PF( "YieldData:    %08x", (u32)pTask->t.yield_data_ptr );
	DL_PF( "YieldDataSize:%08x",      pTask->t.yield_data_size );
}

class DLDebugOutputFile : public DLDebugOutput
{
public:
	DLDebugOutputFile() : Sink(new FileSink)
	{
	}
	~DLDebugOutputFile()
	{
		delete Sink;
	}

	bool Open(const std::filesystem::path filename)
	{
		return Sink->Open(filename.c_str(), "w");
	}

	virtual size_t Write(const void * p, size_t len)
	{
		return Sink->Write(p, len);
	}

	virtual void BeginInstruction(u32 idx, u32 cmd0, u32 cmd1, u32 depth [[maybe_unused]], const char * name)
	{
		Print("[%05d] %08x %08x %-10s\n", idx, cmd0, cmd1, name);
	}

	virtual void EndInstruction()
	{
	}

	FileSink * Sink;
};

DLDebugOutput * DLDebug_CreateFileOutput()
{
	static u32 count = 0;

	std::filesystem::path dumpdir = "DisplayLists";
	dumpdir /= g_ROM.settings.GameName.c_str();
		#if defined(GCC_LEGACY)
		std::string filepath = fmt::format("dl{}.txt", count++);
		#else 
		std::string filepath = std::format("dl{}.txt", count++);
		#endif

	dumpdir /= filepath;

	DLDebugOutputFile * output = new DLDebugOutputFile();
	if (!output->Open(dumpdir))
	{
		delete output;
		DBGConsole_Msg(0, "RDP: Couldn't create dumpfile %s", filepath.c_str());
		return nullptr;
	}

	DBGConsole_Msg(0, "RDP: Dumping Display List as %s", filepath.c_str());
	return output;
}


#endif // DAEDALUS_DEBUG_DISPLAYLIST
