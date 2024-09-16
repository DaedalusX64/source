/*
Copyright (C) 2001,2006,2007 StrmnNrmn

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


#include "stdafx.h"

#include "BlendModes.h"
#include "HLEGraphics/BaseRenderer.h"

#include "Graphics/NativeTexture.h"
#include "Graphics/ColourValue.h"

#include "Core/ROM.h"

#include "Utility/Preferences.h"

#include <vitaGL.h>

/* Define to handle first cycle */

//#define CHECK_FIRST_CYCLE

/* To Devs,
 Placeholder for blendmode maker guide
 */


//*****************************************************************************
// Basic generic blendmode
//*****************************************************************************
// This should handle most inexact blends :)
//
inline void BlendMode_Generic( BLEND_MODE_ARGS ){	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);	}



// Start Blends

/*
 //##
 */

/*
 //#A
 */


/*
 //#B
 */
// Banjo Kazooie - StrmnNrmn *** (N64 Logo, characters etc)
//case 0x001298043f15ffffLL:
//aRGB0: (Texel0       - Primitive   ) * Env          + Primitive
//aA0  : (Texel0       - 0           ) * Shade        + 0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Combined     - 0           ) * Env          + 0

void BlendMode_0x001298043f15ffffLL( BLEND_MODE_ARGS )
{

	// Leave RGB shade untouched
	details.ColourAdjuster.ModulateA( details.EnvColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

}

// Banjo Kazooie -- Backdrop // StrmnNrmn
//case 0x0062fe043f15f9ffLL
//aRGB0: (1            - Primitive   ) * Env          + Primitive
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Combined     - 0           ) * Env          + 0
void BlendMode_0x0062fe043f15f9ffLL( BLEND_MODE_ARGS )
{
	// Won't work unless witout install texture..
	details.InstallTexture = false;
}

// Banjo Kazooie - Paths
//case 0x002698041f14ffffLL:
//aRGB0: (Texel1       - Texel0      ) * LOD_Frac     + Texel0
//aA0  : (Texel0       - 0           ) * Shade        + 0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Combined     - 0           ) * Env          + 0
void BlendMode_0x002698041f14ffffLL( BLEND_MODE_ARGS )
{
	details.ColourAdjuster.ModulateA( details.EnvColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
 //#C
 */

//Conker mouth/tail
//case 0x00fffe8ff517f8ffLL:
//aRGB0: (0            - 0           ) * 0            + 0
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (Shade        - Env         ) * K5           + Primitiv
//aA1  : (Combined     - 0           ) * Env          + 0
void BlendMode_0x00fffe8ff517f8ffLL (BLEND_MODE_ARGS)
{
	float envcolor[4] = {details.EnvColour.GetRf(), details.EnvColour.GetGf(), details.EnvColour.GetBf(), details.EnvColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Conker - Chainsaw Smoke
// Ogre Battle 64 - Intro Dust
// case 0x003432685566ff7fLL:
//aRGB0: (Primitive    - Env         ) * Texel0_Alp   + Env
//aA0  : (Primitive    - 0           ) * Texel0       + 0
//aRGB1: (Primitive    - Env         ) * Texel0_Alp   + Env
//aA1  : (Primitive    - 0           ) * Texel0       + 0
void BlendMode_0x003432685566ff7fLL (BLEND_MODE_ARGS)
{
	//Properly fixes both Ogre Battle and Conker.
	details.ColourAdjuster.SetA(details.PrimColour);
	float envcolor[4] = {details.EnvColour.GetRf(), details.EnvColour.GetGf(), details.EnvColour.GetBf(), details.EnvColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Command & Conquer - Water
//case 0x001147fffffffe38LL:
//aRGB0: (Texel0       - 0           ) * Texel1       + 0
//aA0  : (Shade        - 0           ) * Primitive    + 0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x001147fffffffe38LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
}

//Command $ Conquer - Smoke
//case 0x0041c2835587dfefLL:
//aRGB0: (Shade        - Env         ) * Primitive    + 0
//aA0  : (Shade        - Env         ) * Texel0       + 0
//aRGB1: (Shade        - Env         ) * Primitive    + 0
//aA1  : (Shade        - Env         ) * Texel0       + 0
void BlendMode_0x0041c2835587dfefLL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Command $ Conquer - Everything
//case 0x0015982bff327f3fLL:
//aRGB0: (Texel0       - 0           ) * Shade_Alpha  + Shade
//aA0  : (Texel0       - 0           ) * Shade        + 0
//aRGB1: (Texel0       - 0           ) * Shade_Alpha  + Shade
//aA1  : (Texel0       - 0           ) * Shade        + 0
void BlendMode_0x0015982bff327f3fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

/*
 //#D
 */
//DOOM64 Weapons
//case 0x00671603fffcff78LL:
//aRGB0: (1            - 0           ) * PrimLODFrac  + Texel0
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Combined     - 0           ) * Primitive    + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00671603fffcff78LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Duke 3D menu text and Mario 64 : Mario's 3D head
//case 0x0030b26144664924LL:
//aRGB0: (Primitive    - Shade       ) * Texel0       + Shade
//aA0  : (Primitive    - Shade       ) * Texel0       + Shade
//aRGB1: (Primitive    - Shade       ) * Texel0       + Shade
//aA1  : (Primitive    - Shade       ) * Texel0       + Shade
void BlendMode_0x0030b26144664924LL (BLEND_MODE_ARGS)
{
	//This blend only partially fixes Duke 32
	//Complete fix interferes with Mario head
	//Combiner needs debugging
	// Need to modulate the texture*shade for RGBA for Duke
	//
	// This makes Mario's 3D head shiny as supposed to be.

	details.ColourAdjuster.SetA( details.EnvColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Duke Nukem 64 - Menu Text and HUD
// case 0x0050fea144fe7339LL:
//aRGB0: (Env          - Shade       ) * Texel0       + Shade
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Env          - Shade       ) * Texel0       + Shade
//aA1  : (0            - 0           ) * 0            + Texel0
void BlendMode_0x0050fea144fe7339LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Doubutsu no Mori - Player Shadow
// case 0x00ff95ffff0dfe3fLL:
//aRGB0: (0            - 0           ) * 0            + Primitive
//aA0  : (Texel0       - 0           ) * Texel1       + 0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00ff95ffff0dfe3fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Doubutsu no Mori - Buried Gold
// case 0x00149460f50fff7fLL:
//aRGB0: (Texel0       - 0           ) * Texel1_Alp   + 0
//aA0  : (Texel0       - 0           ) * Texel1       + 0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00149460f50fff7fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Doubutsu no Mori - Intro Leaves
// case 0x0017166045fe7f78LL:
//aRGB0: (Texel0       - Shade       ) * PrimLODFrac  + Shade
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0017166045fe7f78LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA(details.PrimColour);
	float envcolor[4] = {details.PrimColour.GetRf(), details.PrimColour.GetGf(), details.PrimColour.GetBf(), details.PrimColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Doubutsu no Mori - River
// case 0x0030e2045f1af47bLL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (1            - 0           ) * Texel0       + Texel1
//aRGB1: (Combined     - 0           ) * Shade        + Texel0
//aA1  : (Combined     - 0           ) * 1            + Primitive
void BlendMode_0x0030e2045f1af47bLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Doubutsu no Mori - Running Smoke
// case 0x00ffac80ff0d93ffLL:
//aRGB0: (0            - 0           ) * 0            + Primitive
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Shade        - 0           ) * Combined     + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00ffac80ff0d93ffLL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Diddy kong Racing plane streamers
//case 0x001218acf00ffe3fLL:
//aRGB0: (Texel0       - 0           ) * Shade        + 0
//aA0  : (Texel0       - 0           ) * Shade        + 0
//aRGB1: (Env          - Combined    ) * Env_Alpha    + Combined
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x001218acf00ffe3fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.ModulateA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Diddy kong Racing car slids
//case 0x00567e034f0e79ffLL:
//aRGB0: (Env          - Shade       ) * Env_Alpha    + Shade
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (Combined     - 0           ) * Primitive    + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00567e034f0e79ffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.ModulateA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Diddy Kong Racing - Diddy Kong Intro / Taj / Clock Guy
// case 0x001596a430fdfe38LL:
//aRGB0: (Texel0       - Primitive   ) * Shade_Alpha  + Primitive
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Env          - Combined    ) * Shade        + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x001596a430fdfe38LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Diddy Kong Racing - Intro River
// case 0x002266ac1010923fLL:
//aRGB0: (Texel1       - Texel0      ) * Shade        + Texel0
//aA0  : (1            - Texel0      ) * Primitive    + Texel0
//aRGB1: (Env          - Combined    ) * Env_Alpha    + Combined
//aA1  : (Combined     - 0           ) * Shade        + 0
void BlendMode_0x002266ac1010923fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour.ReplicateAlpha());
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Diddy Kong Racing - Turtle Shell
// case 0x00567e034f0e77ffLL:
//aRGB0: (Env          - Shade       ) * Env_Alpha    + Shade
//aA0  : (0            - 0           ) * 0            + Primitiv
//aRGB1: (Combined     - 0           ) * Primitive    + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00567e034f0e77ffLL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Diddy Kong Racing - Dialog Text
// case 0x005616ac112cfe7fLL:
//aRGB0: (Env          - Texel0      ) * Env_Alpha    + Texel0
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Env          - Texel0      ) * Env_Alpha    + Texel0
//aA1  : (Texel0       - 0           ) * Primitive    + 0
void BlendMode_0x005616ac112cfe7fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour.ReplicateAlpha());
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
 //#E
 */
//Extreme-G2
//case 0x00127ffffffff438LL:
//aRGB0: (Texel0       - 0           ) * Shade        + 0
//aA0  : (0            - 0           ) * 0            + Texel1
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00127ffffffff438LL( BLEND_MODE_ARGS )
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
 //#F
 */

//F1 World GP Wheels
//case 0x0027fe041ffcfdfeLL:
//aRGB0: (Texel1       - Texel0      ) * K5           + Texel0
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + 1
void BlendMode_0x0027fe041ffcfdfeLL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//F1 World GP Sky
//case 0x0055a68730fd923eLL:
//aRGB0: (Env          - Primitive   ) * Shade_Alpha  + Primitive
//aA0  : (Texel1       - Texel0      ) * Primitive    + Texel0
//aRGB1: (Shade        - Combined    ) * CombAlp      + Combined
//aA1  : (0            - 0           ) * 0            + 1
void BlendMode_0x0055a68730fd923eLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}


// F- Zero Tracks / Mario 64 Penguin / Owl / Canon dude Eyes / Face /
//case 0x00147e2844fe793cLL:
//aRGB0: (Texel0       - Shade       ) * Texel0_Alp   + Shade
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (Texel0       - Shade       ) * Texel0_Alp   + Shade
//aA1  : (0            - 0           ) * 0            + Shade
void BlendMode_0x00147e2844fe793cLL( BLEND_MODE_ARGS )
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}

//F-Zero X - Texture Cars second layer.
//case 0x0030fe045ffefbf8LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (0            - 0           ) * 0            + Env
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0030fe045ffefbf8LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA( details.EnvColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// F-Zero X - Texture for cars.
//case 0x00147e045ffefbf8LL:
//aRGB0: (Texel0       - Env         ) * Texel0_Alp   + Env
//aA0  : (0            - 0           ) * 0            + Env
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined

void BlendMode_0x00147e045ffefbf8LL( BLEND_MODE_ARGS )
{
	// RGB = Blend(E,T0,T0a)*Shade	~= Blend(SE, ST0, T0a)
	// A   = Env

	// XXXX needs to modulate again by shade
	details.ColourAdjuster.SetRGBA( details.EnvColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

/*
 //#G
 */

//GoldenEye 007 - Sky
//case 0x0040fe8155fef97cLL:
//aRGB0: (Shade        - Env         ) * Texel0       + Env
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (Shade        - Env         ) * Texel0       + Env
//aA1  : (0            - 0           ) * 0            + Shade
void BlendMode_0x0040fe8155fef97cLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB( details.EnvColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD); // ADD looks better than BLEND for the sky
}

/*
//#H
*/

// Hexen - Arms and Weapons
// case 0x00129bfffffffe38LL:
//aRGB0: (Texel0       - 0           ) * Env          + 0
//aA0  : (Texel0       - 0           ) * Env          + 0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00129bfffffffe38LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
 #I
 */

/*
 #K
 */

// Killer Instinct Gold - Character Shadows
// case 0x00f517eaff2fffffLL:
//aRGB0: (0            - 0           ) * Prim_Alpha   + 0
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (0            - 0           ) * Prim_Alpha   + 0
//aA1  : (Texel0       - 0           ) * Primitive    + 0
void BlendMode_0x00f517eaff2fffffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA(details.PrimColour);
	details.ColourAdjuster.ModulateA(details.PrimColour);
	float envcolor[4] = {details.EnvColour.GetRf(), details.EnvColour.GetGf(), details.EnvColour.GetBf(), details.EnvColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Killer Instinct Gold - Characters and HUD
// case 0x00fffe6af5fcf438LL:
//aRGB0: (0            - 0           ) * 0            + Texel0
//aA0  : (0            - 0           ) * 0            + Texel1
//aRGB1: (Primitive    - Env         ) * Prim_Alpha   + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00fffe6af5fcf438LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Kirby 64 - Ground
//case 0x0030fe045ffefdf8LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0030fe045ffefdf8LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	//Below blend fixes the ground on the first level, but breaks many other things. Reverted.
	//details.ColourAdjuster.SetRGB(details.EnvColour);
	//sceGuTexFunc(GU_TFX_BLEND,GU_TCC_RGB);
}

//Kirby 64 - some parts of the Ground
//case 0x00309e045ffefdf8LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (Texel0       - 0           ) * 0            + 1
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00309e045ffefdf8LL (BLEND_MODE_ARGS)
{
	//details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}


// Kirby - Air seeds, Ridge Racer 64 menu text
//case 0x0030b2615566db6dLL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (Primitive    - Env         ) * Texel0       + Env
//aRGB1: (Primitive    - Env         ) * Texel0       + Env
//aA1  : (Primitive    - Env         ) * Texel0       + Env
void BlendMode_0x0030b2615566db6dLL( BLEND_MODE_ARGS )
{

	//Proper fix for Kirby 64 seeds without breaking RR64.
	details.ColourAdjuster.SetRGBA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

}

/*
 //#M
 */

// Mario Kart 64
//case 0x0060b2c15565feffLL:
//aRGB0: (1            - Env         ) * Texel0       + Primitive
//aA0  : (Primitive    - 0           ) * Texel0       + 0
//aRGB1: (1            - Env         ) * Texel0       + Primitive
//aA1  : (Primitive    - 0           ) * Texel0       + 0
void BlendMode_0x0060b2c15565feffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB( details.PrimColour );
	details.ColourAdjuster.ModulateA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
}

// MRC - Car Windows
// case 0x0030fe045ffef7f8LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (0            - 0           ) * 0            + Primitive
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0030fe045ffef7f8LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}

//MRC - Waterfall
// case 0x00272c031f0c93ffLL:
//aRGB0: (Texel1       - Texel0      ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Combined     - 0           ) * Primitive    + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00272c031f0c93ffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

// Mario Party - River
// case 0x00127624ffef93c9LL:
//aRGB0: (Texel0       - 0           ) * Shade        + 0
//aA0  : (0            - Texel0      ) * Primitive    + Texel0
//aRGB1: (Texel0       - 0           ) * Shade        + 0
//aA1  : (0            - Texel0      ) * Primitive    + Texel0
void BlendMode_0x00127624ffef93c9LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA(details.PrimColour);
	details.ColourAdjuster.ModulateA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Mortal Kombat 4 - Text
// case 0x0011fe2344fe7339LL:
//aRGB0: (Texel0       - Shade       ) * Primitive    + Shade
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Texel0       - Shade       ) * Primitive    + Shade
//aA1  : (0            - 0           ) * 0            + Texel0
void BlendMode_0x0011fe2344fe7339LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Mortal Kombat 4 - Character Selection Background / Tower
// case 0x0011fe2355fefd7eLL:
//aRGB0: (Texel0       - Env         ) * Primitive    + Env
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Texel0       - Env         ) * Primitive    + Env
//aA1  : (0            - 0           ) * 0            + 1
void BlendMode_0x0011fe2355fefd7eLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Mega Man 64 - Explosion / Electric Barrier
// case 0x00ffb3ffff00fe3fLL:
//aRGB0: (0            - 0           ) * 0            + Texel0
//aA0  : (Primitive    - 0           ) * Texel0       + 0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (Combined     - 0           ) * Combined     + 0
void BlendMode_0x00ffb3ffff00fe3fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	details.ColourAdjuster.SetA(details.PrimColour);
	details.ColourAdjuster.ModulateA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}


/*
 //#N
 */

/*
 //#O
 */

/*
 //#P
 */
//Pilot Wings 64 sky
//case 0x00627fff1ffcfc38LL:
//aRGB0: (1            - Texel0      ) * Shade        + Texel0
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00627fff1ffcfc38LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Pilot Wings 64 sky
//case 0x0015fec4f0fff83cLL:
//aRGB0: (Texel0       - 0           ) * Shade_Alpha  + 0
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (1            - Combined    ) * Shade        + Combined
//aA1  : (0            - 0           ) * 0            + Shade
void BlendMode_0x0015fec4f0fff83cLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Paper Mario Lava Room - Mario & His Partner
//case 0x00117e80f5fff438LL:
//aRGB0: (Texel0       - 0           ) * Texel1       + 0
//aA0  : (0            - 0           ) * 0            + Texel1
//aRGB1: (Shade        - Env         ) * Combined     + Combined
//aA1  : (0            - 0           ) * 0            + Combined
/*void BlendMode_0x00117e80f5fff438LL (BLEND_MODE_ARGS)
{
	//Makes Mario & his partner appear as black boxes.( This game has this same problem everywhere.)
	//Seems like a core issue to me -Salvy

	details.ColourAdjuster.SetRGBA( details.EnvColour );
	sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGB);
}*/

// Paper Mario - Intro Water
// case 0x0020a203ff13ff7fLL:
//aRGB0: (Texel1       - 0           ) * Texel0       + 0
//aA0  : (Texel1       - 0           ) * Texel0       + 0
//aRGB1: (Combined     - 0           ) * Primitive    + Env
//aA1  : (Combined     - 0           ) * Shade        + 0
void BlendMode_0x0020a203ff13ff7fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Paper Mario - Intro Lighting
// case 0x0061a5ff1f10d23fLL:
//aRGB0: (1            - Texel0      ) * Primitive    + Texel0
//aA0  : (Texel1       - Env         ) * Texel1       + Texel0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (Combined     - 0           ) * Shade        + 0
void BlendMode_0x0061a5ff1f10d23fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Paper Mario - Dust When Characters Walk.
//case 0x00ffabffff0d92ffLL:
//aRGB0: (0            - 0           ) * 0            + Primitive
//aA0  : (Texel1       - Texel0      ) * Env          + Texel0
//aRGB1: (0            - 0           ) * 0            + Primitive
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00ffabffff0d92ffLL (BLEND_MODE_ARGS)
{
	//Copied from old blend file.
	details.ColourAdjuster.SetA( details.EnvColour );
	//details.ColourAdjuster.SetRGB( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
}

// Pokemon Stadium - Balloons
// case 0x003096045ffefff8LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x003096045ffefff8LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Pokemon Stadium - Thunder
// case 0x00272c60150c937dLL:
//aRGB0: (Texel1       - Texel0      ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + Env
void BlendMode_0x00272c60150c937dLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Paper Mario - Intro Fireball
// case 0x00322bff5f0e923fLL:
//aRGB0: (Primitive    - Env         ) * Shade        + Env
//aA0  : (Texel1       - Texel0      ) * Env          + Texel0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00322bff5f0e923fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Pokemon Stadium - Surf
// case 0x00277e601510f77fLL:
//aRGB0: (Texel1       - Texel0      ) * PrimLODFrac  + Texel0
//aA0  : (0            - 0           ) * 0            + Primitive
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Shade        + 0
void BlendMode_0x00277e601510f77fLL (BLEND_MODE_ARGS)
{
	// The blend is colored correctly, but it looks as if it is compressed
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Pokemon Stadium - Fire Blast
// case 0x00277e60150cf37fLL:
//aRGB0: (Texel1       - Texel0      ) * PrimLODFrac  + Texel
//aA0  : (0            - 0           ) * 0            + Texel
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00277e60150cf37fLL (BLEND_MODE_ARGS)
{
	// Fixes most fire type attacks
	details.ColourAdjuster.SetRGB(details.EnvColour);
	details.ColourAdjuster.ModulateA(details.PrimColour);
	float envcolor[4] = {details.PrimColour.GetRf(), details.PrimColour.GetGf(), details.PrimColour.GetBf(), details.PrimColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Pokemos Stadium 2 - Intro
// case 0x0017666025fd7f78LL:
//aRGB0: (Texel0       - Texel1      ) * PrimLODFrac  + Texel1
//aA0  : (1            - 0           ) * Primitive    + 0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0017666025fd7f78LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	details.ColourAdjuster.SetA(details.PrimColour);
	float envcolor[4] = {details.PrimColour.GetRf(), details.PrimColour.GetGf(), details.PrimColour.GetBf(), details.PrimColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Pokemos Stadium 2 - Intro: Pichu
// case 0x0077666045fd7f78LL:
//aRGB0: (CombAlp      - Shade       ) * PrimLODFrac  + Texel1
//aA0  : (1            - 0           ) * Primitive    + 0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0077666045fd7f78LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	details.ColourAdjuster.SetA(details.PrimColour);
	float envcolor[4] = {details.PrimColour.GetRf(), details.PrimColour.GetGf(), details.PrimColour.GetBf(), details.PrimColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Pokemos Stadium 2 - Pokemon Selection Menu
// case 0x0071fffffffefc38LL:
//aRGB0: (CombAlp      - 0           ) * Primitive    + Env
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0071fffffffefc38LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	float envcolor[4] = {details.EnvColour.GetRf(), details.EnvColour.GetGf(), details.EnvColour.GetBf(), details.EnvColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Pokemon Stadium 2 - Text Dialog Box
// case 0x0071fee344fe793cLL:
//aRGB0: (CombAlp      - Shade       ) * Primitive    + Shade
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (CombAlp      - Shade       ) * Primitive    + Shade
//aA1  : (0            - 0           ) * 0            + Shade
void BlendMode_0x0071fee344fe793cLL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Pokemon Stadium 2 - Pokemon Selection Box
// case 0x0050d2a133a5b6dbLL:
//aRGB0: (Env          - Primitive   ) * Texel0       + Primitive
//aA0  : (Env          - Primitive   ) * Texel0       + Primitive
//aRGB1: (Env          - Primitive   ) * Texel0       + Primitive
//aA1  : (Env          - Primitive   ) * Texel0       + Primitive
void BlendMode_0x0050d2a133a5b6dbLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Pokemon Stadium 2 - Stadium Ground
// case 0x00457fff3ffcfe3fLL:
//aRGB0: (Shade        - 0           ) * Primitive    + 0
//aA0  : (0            - 0           ) * 0            + 0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + 0
void BlendMode_0x00457fff3ffcfe3fLL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Pokemon Stadium 2 - Stadium N64 Logo (Dangerous)
// case 0x00627fff3ffe7e3fLL:
//aRGB0: (Shade        - 0           ) * Primitive    + 0
//aA0  : (0            - 0           ) * 0            + 0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + 0
//void BlendMode_0x00627fff3ffe7e3fLL (BLEND_MODE_ARGS)
//{
//	sceGuTexFunc(GU_TFX_DECAL,GU_TCC_RGBA);
//}

// Pokemon Stadium 2 - Battle HUD (Breaks RR64)
// case 0x00119623ff2fffffLL:
//aRGB0: (Texel0       - 0           ) * Primitive    + 0
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Texel0       - 0           ) * Primitive    + 0
//aA1  : (Texel0       - 0           ) * Primitive    + 0
//void BlendMode_0x00119623ff2fffffLL (BLEND_MODE_ARGS)
//{
//	// Stops HUD ghosting effect
//	details.ColourAdjuster.SetRGB(details.EnvColour);
//	sceGuTexEnvColor(details.PrimColour.GetColour());
//	sceGuTexFunc(GU_TFX_BLEND,GU_TCC_RGBA);
//}

// Pokemon Stadium 2 - Pokeball Swirls
// case 0x00272c603510e37fLL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - 1           ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Shade        + 0
void BlendMode_0x00272c603510e37fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
}

/*
 //#Q
 */

// Quake 64 - Walls and ground
//case 0x00117ffffffefc38LL:
//aRGB0: (Texel0       - 0           ) * Texel1       + Env
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00117ffffffefc38LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

// Quest 64 - Bubbles
// case 0x00629bff1ffcfe38LL:
//aRGB0: (1            - Texel0      ) * Env          + Texel0
//aA0  : (Texel0       - 0           ) * Env          + 0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00629bff1ffcfe38LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

/*
 //#R
 */
//Road Rash 64 trees
//case 0x00129bfffffdf638LL:
//aRGB0: (Texel0       - 0           ) * Env          + Primitive
//aA0  : (Texel0       - 0           ) * Env          + Primitive
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00129bfffffdf638LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB( details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
 //#S
 */

//Star Wars Racer Ep1 shadows
//case 0x00fff3fffffdb638LL:
//aRGB0: (0            - 0           ) * 0            + Primitive
//aA0  : (0            - Primitive   ) * Texel0       + Primitive
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00fff3fffffdb638LL( BLEND_MODE_ARGS )
{
	details.ColourAdjuster.SetA( details.PrimColour );
	details.ColourAdjuster.SubtractRGB( details.PrimColour.ReplicateAlpha() );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//SSB Bomb Partial-Flashing Animation
//case 0x00127eacf0fff238LL:
//aRGB0: (Texel0       - 0           ) * Shade        + 0
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Env          - Combined    ) * Env_Alpha    + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00127eacf0fff238LL( BLEND_MODE_ARGS )
{
	// By Kreationz & Shinydude100
	details.ColourAdjuster.SetAOpaque();//Opaque still isn't doing much on the Alpha..?
	details.ColourAdjuster.SetRGB( details.EnvColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND); //Not sure if BLEND is doing anything?
}

// Super Mario 64 - Mario Face
//case 0x00147e2844fe7b3dLL:
//aRGB0: (Texel0       - Shade       ) * Texel0_Alp   + Shade
//aA0  : (0            - 0           ) * 0            + Env
//aRGB1: (Texel0       - Shade       ) * Texel0_Alp   + Shade
//aA1  : (0            - 0           ) * 0            + Env
void BlendMode_0x00147e2844fe7b3dLL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}

//Sin and Punishment Grass
//case 0x00541aa83335feffLL:
//aRGB0: (Env          - Primitive   ) * Texel0_Alp   + Primitive
//aA0  : (Texel0       - 0           ) * Env          + 0
//aRGB1: (Env          - Primitive   ) * Texel0_Alp   + Primitive
//aA1  : (Texel0       - 0           ) * Env          + 0
void BlendMode_0x00541aa83335feffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB( details.PrimColour);
	details.ColourAdjuster.SetA( details.EnvColour );
	float envcolor[4] = {details.EnvColour.GetRf(), details.EnvColour.GetGf(), details.EnvColour.GetBf(), details.EnvColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Sin and Punishment - Sky <----- Needs work
//case 0x001114a7f3fffef8LL:
//aRGB0: (Texel0       - 0           ) * Texel1       + 0
//aA0  : (Texel0       - 0           ) * Texel1       + 0
//aRGB1: (Env          - Primitive   ) * CombAlp      + Primitive
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x001114a7f3fffef8LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA( details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
}

//Sin and Punishment - Ground
//case 0x00547ea833fdf2f9LL:
//aRGB0: (Env          - Primitive   ) * Texel0_Alp   + Primitive
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Env          - Primitive   ) * Texel0_Alp   + Primitive
//aA1  : (0            - 0           ) * 0            + Texel0
void BlendMode_0x00547ea833fdf2f9LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB( details.PrimColour);

	// A Needs to be 1 or Shade to be Opaque
	//details.ColourAdjuster.SetAOpaque();
	float envcolor[4] = {details.EnvColour.GetRf(), details.EnvColour.GetGf(), details.EnvColour.GetBf(), details.EnvColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);

	 //T0Alpha = DECAL mode on PSP
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Sin and Punishment - blinds the screen
//case 0x00327e64fffff9fcLL:
//aRGB0: (Primitive    - 0           ) * Shade        + 0
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (Primitive    - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Shade
void BlendMode_0x00327e64fffff9fcLL( BLEND_MODE_ARGS )
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Sin and Punishment - Particles and Explosions
//case 0x00551aaa1134fe7fLL:
//aRGB0: (Env          - Texel0      ) * Prim_Alpha   + Texel0
//aA0  : (Texel0       - 0           ) * Env          + 0
//aRGB1: (Env          - Texel0      ) * Prim_Alpha   + Texel0
//aA1  : (Texel0       - 0           ) * Env          + 0
void BlendMode_0x00551aaa1134fe7fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA( details.PrimColour.ReplicateAlpha() );
	details.ColourAdjuster.SetA( details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

// SpiderMan - Waterfall Intro
//case 0x0017e2052ffd75f8LL:
//aRGB0: (Texel0       - Texel1      ) * K5           + Texel1
//aA0  : (1            - 0           ) * Texel0       + Texel1
//aRGB1: (Combined     - 0           ) * Env          + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0017e2052ffd75f8LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA( details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Space Station Silicon Valley - Electric fence
//case 0x0030986155feff79LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (Texel0       - 0           ) * Shade        + 0
//aRGB1: (Primitive    - Env         ) * Texel0       + Env
//aA1  : (0            - 0           ) * 0            + Texel0
void BlendMode_0x0030986155feff79LL( BLEND_MODE_ARGS )
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

//Space Station Silicon Valley - teleporter
//case 0x0021246015fc9378LL:
//aRGB0: (Texel1       - Texel0      ) * Texel1       + Texel0
//aA0  : (Texel1       - Texel0      ) * Texel1       + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0021246015fc9378LL( BLEND_MODE_ARGS )
{
	details.ColourAdjuster.SetRGB( details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Space Station Silicon Valley - smoke and pickups
//case 0x00272c6015fc9378LL:
//aRGB0: (Texel1       - Texel0      ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00272c6015fc9378LL( BLEND_MODE_ARGS )
{
	details.ColourAdjuster.SetRGB( details.EnvColour);
	float envcolor[4] = {details.PrimColour.GetRf(), details.PrimColour.GetGf(), details.PrimColour.GetBf(), details.PrimColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Space Station Silicon Valley - Fences and windshield
//case 0x0026a0041ffc93e0LL:
//aRGB0: (Texel1       - Texel0      ) * LOD_Frac     + Texel0
//aA0  : (Texel1       - Texel0      ) * Combined     + Texel0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - Shade       ) * 0            + Combined
void BlendMode_0x0026a0041ffc93e0LL( BLEND_MODE_ARGS )
{
	details.ColourAdjuster.SetAOpaque();
}

//
/*
 //#T
 */


//Tarzan birds wings, Marios drop shadows in SM64, Kirby64 Fence
//case 0x00121824ff33ffffLL:
//aRGB0: (Texel0       - 0           ) * Shade        + 0
//aA0  : (Texel0       - 0           ) * Shade        + 0
//aRGB1: (Texel0       - 0           ) * Shade        + 0
//aA1  : (Texel0       - 0           ) * Shade        + 0
void BlendMode_0x00121824ff33ffffLL( BLEND_MODE_ARGS )
{
	if( g_ROM.GameHacks == TARZAN ) details.ColourAdjuster.SetAOpaque();
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Tony Hawk's Pro Skater - Text
// case 0x00ffffffff09f63fLL:
//aRGB0: (0            - 0           ) * 0            + Primitive
//aA0  : (0            - 0           ) * 0            + Primitive
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (Combined     - 0           ) * Texel1       + 0
void BlendMode_0x00ffffffff09f63fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
 //#U
 */


/*
 //#V
 */
/*
 //#W
 */
//Wetrix backdrop
//case 0x00127e2433fdf8fcLL:
//aRGB0: (Texel0       - Primitive   ) * Shade        + Primitive
//aA0  : (0            - 0           ) * 0            + Shade
//aRGB1: (Texel0       - Primitive   ) * Shade        + Primitive
//aA1  : (0            - 0           ) * 0            + Shade
void BlendMode_0x00127e2433fdf8fcLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour.ReplicateAlpha());
	details.ColourAdjuster.SetA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Wave Racer - Sky
//case 0x0022ffff1ffcfa38LL:
//aRGB0: (Texel1       - Texel0      ) * Env          + Texel0
//aA0  : (0            - 0           ) * 0            + Env
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0022ffff1ffcfa38LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

/*
 //#X
 */

/*
 //#Y
 */


//Yoshi Story - Dust
//case 0x00161a6025fd2578LL:
//aRGB0: (Texel0       - Texel1      ) * Env_Alpha    + Texel1
//aA0  : (Texel0       - Texel1      ) * Env          + Texel1
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00161a6025fd2578LL (BLEND_MODE_ARGS)
{
	// Nice blend of brown and white ;)
	details.ColourAdjuster.SetRGB( details.EnvColour );
	details.ColourAdjuster.SetA( details.PrimColour  );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

/*
 //#Z
 */

// Majora's Mask - Mountain outside Clock Town
//case 0x0020ac04ff0f93ffLL:
//aRGB0: (Texel1       - 0           ) * Texel0       + 0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
 void BlendMode_0x0020ac04ff0f93ffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// OoT intro, MM Intro (N64 Logo)
//case 0x00167e6035fcff7eLL:
//aRGB0: (Texel0       - Primitive   ) * Env_Alpha    + Texel0
//aA0  : (0            - 0           ) * 0            + 0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + 1
void BlendMode_0x00167e6035fcff7eLL( BLEND_MODE_ARGS )
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// OOT Sky - Still patchy, due to lack of Second Texture
//case 0x002527ff1ffc9238LL:
//aRGB0: (Texel1       - Texel0      ) * Prim_Alpha   + Texel0
//aA0  : (Texel1       - Texel0      ) * Primitive    + Texel0
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined

void BlendMode_0x002527ff1ffc9238LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA(details.PrimColour.ReplicateAlpha());
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

// Zelda OoT logo / flames
// Uses the same blend as Pokemon Stadium 2 - Pokeball Exit Glowing Light
//case 0x00272c60350ce37fLL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - 1           ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00272c60350ce37fLL( BLEND_MODE_ARGS )
{

	// RGB = Blend(Env, Prim, (T0 + x(t1-Prim)))
	// A   = (T0+T1-1)*Prim
	details.ColourAdjuster.SetRGB( details.EnvColour );
	details.ColourAdjuster.SetA( details.PrimColour  );
	float envcolor[4] = {details.PrimColour.GetRf(), details.PrimColour.GetGf(), details.PrimColour.GetBf(), details.PrimColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);		// XXXX No T1
}

// Zelda Paths
//case 0x00121603ff5bfff8LL:
//aRGB0: (Texel0       - 0           ) * Shade        + 0
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Combined     - 0           ) * Primitive    + 0
//aA1  : (Texel1       - 0           ) * 1            + Combined
void BlendMode_0x00121603ff5bfff8LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.ModulateRGB( details.PrimColour );
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Zelda Fairies // Needs T1 to show attenuitive colors
//case 0x00262a60150c937fLL:
//aRGB0: (Texel1       - Texel0      ) * Env_Alpha    + Texel0
//aA0  : (Texel1       - Texel0      ) * Env          + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0

void BlendMode_0x00262a60150c937fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}


//Zelda Water
//case 0x00267e041f0cfdffLL:
//aRGB0: (Texel1       - Texel0      ) * Env_Alpha    + Texel0
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0

void BlendMode_0x00267e041f0cfdffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Oot - Big Tree in Kokiri Forest and Clouds on Ganondorf's castle
//case 0x00262a041f5893f8LL:
//aRGB0: (Texel1       - Texel0      ) * Env_Alpha    + Texel0
//aA0  : (Texel1       - Texel0      ) * Env          + Texel0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Texel1       - 0           ) * 1            + Combined
void BlendMode_0x00262a041f5893f8LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Zelda Chest Opening Light
// case 0x0020ac60350c937fLL:
//aRGB0: (Texel1       - Primitive   ) * Texel0       + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x0020ac60350c937fLL (BLEND_MODE_ARGS)
{
	float envcolor[4] = {details.EnvColour.GetRf(), details.EnvColour.GetGf(), details.EnvColour.GetBf(), details.EnvColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// Zelda Kokiri Sword Blade
//	case 0x00177e6035fcfd7eLL:
//aRGB0: (Texel0       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + 1
void BlendMode_0x00177e6035fcfd7eLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB (details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Zelda Kokiri Sword Handle
//	case 0x0030fe045ffefdfeLL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + 1
void BlendMode_0x0030fe045ffefdfeLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA (details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Zelda Sign Cut (sword), RR64 Screen overlay
//case 0x0030b3ff5ffeda38LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (Primitive    - Env         ) * Texel0       + Env
//aRGB1: (0            - 0           ) * 0            + Combined
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0030b3ff5ffeda38LL (BLEND_MODE_ARGS)
{
	if( g_ROM.GameHacks == ZELDA_OOT )
	{
		details.ColourAdjuster.SetRGB(details.EnvColour);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		details.ColourAdjuster.SetRGBA( details.PrimColour );
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	}
}

// Zelda Arrows
//case 0x0030ec045fdaedf6LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (1            - 1           ) * 1            + 1
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (1            - 1           ) * 1            + 1
void BlendMode_0x0030ec045fdaedf6LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Zelda OOT Deku Nut Core
//		case 0x00276c6035d8ed76LL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (1            - 1           ) * 1            + 1
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (1            - 1           ) * 1            + 1

void BlendMode_0x00276c6035d8ed76LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// OOT - Gold Skullata Badge placeholder
//case 0x00171c6035fd6578LL:
//aRGB0: (Texel0       - Primitive   ) * PrimLODFrac  + Texel1
//aA0  : (Texel0       - 1           ) * 1            + Texel1
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
//void BlendMode_0x00171c6035fd6578LL (BLEND_MODE_ARGS)
//{
//	sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
//}

// OOT - Gold Skulltula Eyes
//case 0x0030fe045f0ef3ffLL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x0030fe045f0ef3ffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// OOT
// Zora's Domain Water // Correct except T1
//case 0x00267e031f0cfdffLL:
//aRGB0: (Texel1       - Texel0      ) * Env_Alpha    + Texel0
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Combined     - 0           ) * Primitive    + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00267e031f0cfdffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

// OOT - Gold Skulltula Chin
//case 0x00177e6035fcfd78LL:
//aRGB0: (Texel0       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00177e6035fcfd78LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	details.ColourAdjuster.SetRGBA( c32::Gold );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//OOT Queen Spider Fog
//case 0x00262a041f0c93ffLL:
//aRGB0: (Texel1       - Texel0      ) * Env_Alpha    + Texel0
//aA0  : (Texel1       - Texel0      ) * Env          + Texel0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00262a041f0c93ffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA(details.EnvColour);
	details.ColourAdjuster.SetRGB(details.EnvColour.ReplicateAlpha());
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Zelda Mega Heart Container Frame
//case 0x00177e60350cf37fLL:
//aRGB0: (Texel0       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0

void BlendMode_0x00177e60350cf37fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Zelda Mega Heart Container Filling
//case 0x00272c60150c937fLL:
//aRGB0: (Texel1       - Texel0      ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0

void BlendMode_0x00272c60150c937fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Zelda Boss Portal
//case 0x0026a060150c937fLL:
//aRGB0: (Texel1       - Texel0      ) * LOD_Frac     + Texel0
//aA0  : (Texel1       - Texel0      ) * Combined     + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x0026a060150c937fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Zelda Goddess Explosion
//		case 0x00271860350cff7fLL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel0       - 0           ) * Shade        + 0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0

void BlendMode_0x00271860350cff7fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	details.ColourAdjuster.SetA(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}
//Zelda OOT Deku Tree Light Flash / Glass Bottle
//		case 0x0017166035fcff78LL:
//aRGB0: (Texel0       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined

void BlendMode_0x0017166035fcff78LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour.ReplicateAlpha());
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Zelda OOT - Hylian Shield Triforce Badge
// OOT - Gold Boss Key
//	case 0x00176c6035d8ed76LL:
//aRGB0: (Texel0       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (1            - 1           ) * 1            + 1
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (1            - 1           ) * 1            + 1
void BlendMode_0x00176c6035d8ed76LL (BLEND_MODE_ARGS)
{
	// Fix for Gold Boss Key
	// sceGuTexEnvColor(details.EnvColour.GetColour());
	// sceGuTexFunc(GU_TFX_BLEND, GU_TCC_RGB);
	// details.ColourAdjuster.SetRGB(c32::Gold);
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Pokemon Stadium 2 - Pokeball
// ZELDA - OOT - Spiritual Stone Gems
//case 0x00272c60350c937fLL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00272c60350c937fLL (BLEND_MODE_ARGS)
{
	if( g_ROM.GameHacks == ZELDA_OOT )
	{
		details.ColourAdjuster.SetRGB( details.EnvColour );
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	}
	else
	{
		details.ColourAdjuster.SetRGBA(details.EnvColour);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	}
}
//Zelda Triforce (Needs PrimLODFrac to be properly coloured and Shiny.
//case 0x00277e6035fcf778LL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (0            - 0           ) * 0            + Primitive
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00277e6035fcf778LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB( c32::Gold );
	details.ColourAdjuster.SetRGBA (details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// OoT ground texture and water temple walls
//case 0x00267e041ffcfdf8LL:
//aRGB0: (Texel1       - Texel0      ) * Env_Alpha    + Texel0
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00267e041ffcfdf8LL( BLEND_MODE_ARGS )
{
	// XXXX placeholder implementation - ok except t1?
	// RGB = Blend(T0, T1, EnvAlpha) * Shade
	// A   = 1
	details.ColourAdjuster.SetAOpaque();	// Alpha 1.0
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Zelda Bottled Water
//case 0x00272c6035fc9378LL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00272c6035fc9378LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}


// Zelda Poe
//case 0x00772c60f5fce378LL:
//aRGB0: (CombAlp      - 0           ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - 1           ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00772c60f5fce378LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Zelda Blue Fire Lamp
//	case 0x00272c6035fce378LL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - 1           ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00272c6035fce378LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

//Zelda Fairy Spirit
//case 0x00271c6035fcf378LL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel0       - 0           ) * 1            + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00271c6035fcf378LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}
// OOT - Lens of Truth
//case 0x00373c6e117b9fcfLL:
//aRGB0: (Primitive    - Texel0      ) * PrimLODFrac  + 0
//aA0  : (Primitive    - Texel0      ) * 1            + 0
//aRGB1: (Primitive    - Texel0      ) * PrimLODFrac  + 0
//aA1  : (Primitive    - Texel0      ) * 1            + 0
void BlendMode_0x00373c6e117b9fcfLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}
//Zelda Bottle Detail / Ridge Racer Fences
//case 0x0030fe045ffef3f8LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0030fe045ffef3f8LL (BLEND_MODE_ARGS)
{
	// details.EnvColour breaks stuff in RR64
	if( g_ROM.GameHacks == ZELDA_OOT ) details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//Zelda Cukuaan Egg
//case 0x0030ec6155daed76LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (1            - 1           ) * 1            + 1
//aRGB1: (Primitive    - Env         ) * Texel0       + Env
//aA1  : (1            - 1           ) * 1            + 1
void BlendMode_0x0030ec6155daed76LL (BLEND_MODE_ARGS)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

//OOT Hyrule Castle Wall Shadow
// OOT - Temple of Time Pedestal
// case 0x00267e031ffcfdf8LL:
//aRGB0: (Texel1       - Texel0      ) * Env_Alpha    + Texel0
//aA0  : (0            - 0           ) * 0            + 1
//aRGB1: (Combined     - 0           ) * Primitive    + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x00267e031ffcfdf8LL (BLEND_MODE_ARGS)
{
	//Fixes Temple of Time Pedestal
	//details.ColourAdjuster.SetRGB(details.PrimColour);
	//sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGB);
	details.ColourAdjuster.SetRGB(details.EnvColour.ReplicateAlpha());
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

//OOT Castle light
//case 0x00272c60340c933fLL:
//aRGB0: (Texel1       - Primitive   ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Primitive    - Shade       ) * Combined     + Shade
//aA1  : (Combined     - 0           ) * Primitive    + 0

void BlendMode_0x00272c60340c933fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.PrimColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// OOT - Song of Time
//		case 0x00262a603510937fLL:
//aRGB0: (Texel1       - Primitive   ) * Env_Alpha    + Texel0
//aA0  : (Texel1       - Texel0      ) * Env          + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (Combined     - 0           ) * Shade        + 0

void BlendMode_0x00262a603510937fLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGB(details.EnvColour);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}

// MM - Sky
//	case 0x0025266015fc9378LL:
//aRGB0: (Texel1       - Texel0      ) * Prim_Alpha   + Texel0
//aA0  : (Texel1       - Texel0      ) * Primitive    + Texel0
//aRGB1: (Primitive    - Env         ) * Combined     + Env
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0025266015fc9378LL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetRGBA( details.PrimColour.ReplicateAlpha() );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); //Wally says is REPLACE
}

// Super Smash Bros - Dream Land Water & MM Ground in town
// Majora's Mask - Water,bells,trees,ground,inside rooms and grass.
//case 0x00272c041f0c93ffLL:
//aRGB0: (Texel1       - Texel0      ) * PrimLODFrac  + Texel0
//aA0  : (Texel1       - Texel0      ) * 1            + Texel0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x00272c041f0c93ffLL (BLEND_MODE_ARGS)
{
	details.ColourAdjuster.SetA( details.PrimColour );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

// Xena - StrmnNrmn
// OOT Horse dust
//case 0x0030b2045ffefff8LL:
//aRGB0: (Primitive    - Env         ) * Texel0       + Env
//aA0  : (Primitive    - 0           ) * Texel0       + 0
//aRGB1: (Combined     - 0           ) * Shade        + 0
//aA1  : (0            - 0           ) * 0            + Combined
void BlendMode_0x0030b2045ffefff8LL(BLEND_MODE_ARGS)
{
	//Copied from old blend file (Possibly could be shorter?)
	details.ColourAdjuster.SetRGB( c32::White );
	float envcolor[4] = {details.PrimColour.GetRf(), details.PrimColour.GetGf(), details.PrimColour.GetBf(), details.PrimColour.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
}


// WWF Wreslemania 2000 - Wrestlers
// Command $ Conquer - Shades
//case 0x0015fe2bfffff3f9LL:
//aRGB0: (Texel0       - 0           ) * Shade_Alpha  + 0
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Texel0       - 0           ) * Shade_Alpha  + 0
//aA1  : (0            - 0           ) * 0            + Texel0
void BlendMode_0x0015fe2bfffff3f9LL( BLEND_MODE_ARGS )
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

// WWF Wreslemania 2000 - Menu & Text
//case 0x005196a3112cfe7fLL:
//aRGB0: (Env          - Texel0      ) * Primitive    + Texel0
//aA0  : (Texel0       - 0           ) * Primitive    + 0
//aRGB1: (Env          - Texel0      ) * Primitive    + Texel0
//aA1  : (Texel0       - 0           ) * Primitive    + 0
void BlendMode_0x005196a3112cfe7fLL( BLEND_MODE_ARGS )
{
	// ADD fixes the text when the game starts, but breaks the main menu errrg, we prefer to render the menu right instead..
	//sceGuTexFunc(GU_TFX_ADD,GU_TCC_RGBA);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

// WWF Wreslemania 2000 - Wrestlers Main Menu
//case 0x0015fea3f00ff23fLL:
//aRGB0: (Texel0       - 0           ) * Shade_Alpha  + 0
//aA0  : (0            - 0           ) * 0            + Texel0
//aRGB1: (Env          - Combined    ) * Primitive    + Combined
//aA1  : (Combined     - 0           ) * Primitive    + 0
void BlendMode_0x0015fea3f00ff23fLL( BLEND_MODE_ARGS )
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}


bool	IsInexactDefault( OverrideBlendModeFn Fn )
{
	return Fn == BlendMode_Generic;
}

//*****************************************************************************
// This only for hacks etc these are non-inexact blendmodes
// Be carefull when adding these since they can potentially break other games
//*****************************************************************************
OverrideBlendModeFn		LookupOverrideBlendModeForced( u64 mux )
{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	if(!gGlobalPreferences.CustomBlendModes) return nullptr;
#endif
	switch(mux)
	{
#define BLEND_MODE( x )		case (x):	return BlendMode_##x;
			//BLEND_MODE(0x00119623ff2fffffLL); // Pokemon Stadium 2 HUD //Ruins RR64
			BLEND_MODE(0x00121824ff33ffffLL); // Tarzan (also used by Paper Mario)
			BLEND_MODE(0x00127ffffffff438LL); // Extreme-G2
			BLEND_MODE(0x0030986155feff79LL); // SSV electric fence
			case(0x00327e64fffff9fcLL): return g_ROM.GameHacks==SIN_PUNISHMENT ? BlendMode_0x00327e64fffff9fcLL : nullptr; // Sin and Punishment - blinds the screen (breaks splash screen race flag in MK64)
			BLEND_MODE(0x00457fff3ffcfe3fLL); // Pokemon Stadium 2 Arena Floor
			BLEND_MODE(0x0050fea144fe7339LL); // Duke Nukem Menu and HUD
			BLEND_MODE(0x0060b2c15565feffLL); // Mario Kart 64
			//BLEND_MODE(0x00627fff3ffe7e3fLL); // Pokemon Stadium 2 N64 Logo //Dangerous!!
			BLEND_MODE(0x00ffffffff09f63fLL); // THPS Text
			BLEND_MODE(0x00129bfffffffe38LL); // Hexen - Arms
			BLEND_MODE(0x00fffe6af5fcf438LL); // Killer Instinct Characters and HUD
#undef BLEND_MODE
	}

	return nullptr;
}

//*****************************************************************************
// Inexact blendmodes, we find a match or try to guess a correct blending
//*****************************************************************************
OverrideBlendModeFn		LookupOverrideBlendModeInexact( u64 mux )
{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	if(!gGlobalPreferences.CustomBlendModes) return nullptr;
#endif
	switch(mux)
	{
#define BLEND_MODE( x )		case (x):	return BlendMode_##x;
			BLEND_MODE(0x001114a7f3fffef8LL); // Sin and Punishment - Sky <----- Needs work
			BLEND_MODE(0x001147fffffffe38LL); // Command & Conquer - Water
			//BLEND_MODE(0x00117e80f5fff438LL); // Paper Mario block texture partial fix
			BLEND_MODE(0x00117ffffffefc38LL); // Quake 64 - Walls and ground
			BLEND_MODE(0x0011fe2344fe7339LL); // Mortal Kombat 4 - Text
			BLEND_MODE(0x0011fe2355fefd7eLL); // Mortal Kombat 4 -Character Selection screen background / Tower
			BLEND_MODE(0x00121603ff5bfff8LL); // Zelda Paths
			BLEND_MODE(0x001218acf00ffe3fLL); // DKR plane streamers
			BLEND_MODE(0x00127624ffef93c9LL); // Mario Party - River
			BLEND_MODE(0x00127e2433fdf8fcLL); // Wetrix Background / Banjo Kazooie
			BLEND_MODE(0x00127eacf0fff238LL); // SSB Link bomb
			BLEND_MODE(0x001298043f15ffffLL); // Banjo Kazooie N64 Logo / Characters
			BLEND_MODE(0x00129bfffffdf638LL); // Road Rush64 trees
			BLEND_MODE(0x00147e2844fe7b3dLL); // Mario's Head
			BLEND_MODE(0x00147e045ffefbf8LL); // FZero other ships
			BLEND_MODE(0x00147e2844fe793cLL); // FZero tracks / Mario 64 penguin's eyes
			BLEND_MODE(0x00149460f50fff7fLL); // Animal Crossing Gold
			BLEND_MODE(0x001596a430fdfe38LL); // DKR Intro Plane
			BLEND_MODE(0x0015982bff327f3fLL); // Command $ Conquer - Everything
			BLEND_MODE(0x0015fe2bfffff3f9LL); // WWF Wreslemania 2000 - Wrestlers
			BLEND_MODE(0x0015fea3f00ff23fLL); // WWF Wreslemania 2000 - Wrestlers Main Menu
			BLEND_MODE(0x0015fec4f0fff83cLL); // Pilot Wings 64 sky
			BLEND_MODE(0x00161a6025fd2578LL); // Yoshi - Dust
			BLEND_MODE(0x00167e6035fcff7eLL); // OOT, MM Intro (N64 Logo)
			BLEND_MODE(0x0017166035fcff78LL); // OOT Deku tree Flash
			BLEND_MODE(0x0017166045fe7f78LL); // Animal Crossing Leaves
			BLEND_MODE(0x0017666025fd7f78LL); // Pokemon Stadium 2 Intro
			BLEND_MODE(0x00176c6035d8ed76LL); // Zelda Hylian Shield Triforce Badge
			BLEND_MODE(0x00177e60350cf37fLL); // Zelda Heart Container Frame
			BLEND_MODE(0x00177e6035fcfd7eLL); // Zelda Kokori Sword Blade
			BLEND_MODE(0x00177e6035fcfd78LL); // Gold Skulltula Chin
			BLEND_MODE(0x0017e2052ffd75f8LL); // SpiderMan - Waterfall Intro
			BLEND_MODE(0x0020ac04ff0f93ffLL); // Zelda MM : Mountain outside Clock Town
			BLEND_MODE(0x0020ac60350c937fLL); // Zelda Chest Opening Light
			BLEND_MODE(0x0020a203ff13ff7fLL); // Paper Mario -Intro Water
			BLEND_MODE(0x0021246015fc9378LL); // SSV teleporter
			BLEND_MODE(0x002266ac1010923fLL); // DKR River
			BLEND_MODE(0x0022ffff1ffcfa38LL); // Wave racer - sky
			BLEND_MODE(0x0025266015fc9378LL); // MM Sky
			BLEND_MODE(0x002527ff1ffc9238LL); // OOT Sky
			BLEND_MODE(0x0026a0041ffc93e0LL); // SSV Fences
			BLEND_MODE(0x00262a041f0c93ffLL); // OOT Fog in Deku Tree
			BLEND_MODE(0x00262a603510937fLL); // OOT - Song of Time
			BLEND_MODE(0x0026a060150c937fLL); // Zelda Boss Portal
			BLEND_MODE(0x00267e031ffcfdf8LL); // OOT Hyrule Castle Shadows
			BLEND_MODE(0x00262a041f5893f8LL); // Zelda Deku Tree
			BLEND_MODE(0x00262a60150c937fLL); // Zelda Fairies
			BLEND_MODE(0x00267e031f0cfdffLL); // Zelda OOT - Zora's Domain Water
			BLEND_MODE(0x00267e041f0cfdffLL); // Zelda OOT Water
			BLEND_MODE(0x00267e041ffcfdf8LL); // Zelda OOT Walls
			BLEND_MODE(0x002698041f14ffffLL); // Banjo Kazooie Paths
			BLEND_MODE(0x00271860350cff7fLL); // Deku Tree Light
			BLEND_MODE(0x00271c6035fcf378LL); // Zelda Fairy Spirit
			BLEND_MODE(0x00272c031f0c93ffLL); // MRC Waterfall
			BLEND_MODE(0x00272c041f0c93ffLL); // SSB Dreamland Water
			BLEND_MODE(0x00272c60150c937dLL); // Pokemon Thunder
			BLEND_MODE(0x00272c60340c933fLL); // Zelda Castle Light
			BLEND_MODE(0x00272c60150c937fLL); // Zelda Heart Container
			BLEND_MODE(0x00272c6015fc9378LL); // SSV smoke/pickups
			BLEND_MODE(0x00272c60350c937fLL); // OOT Spiritual Stones / Pokeball
			BLEND_MODE(0x00272c60350ce37fLL); // OOT Logo / Flames
			BLEND_MODE(0x00272c603510e37fLL); // Pokemon Stadium 2 - Pokeball Swirls
			BLEND_MODE(0x00272c6035fc9378LL); // Zelda Bottled Water
			BLEND_MODE(0x00272c6035fce378LL); // Zelda Blue Fire Lamp
			BLEND_MODE(0x00276c6035d8ed76LL); // OOT Deku Nut Core
			BLEND_MODE(0x00277e60150cf37fLL); // Pokemon Fire Blast
			BLEND_MODE(0x00277e601510f77fLL); // Pokemon Surf
			BLEND_MODE(0x00277e6035fcf778LL); // Zelda Triforce
			BLEND_MODE(0x0027fe041ffcfdfeLL); // F1 World GP Wheels
			BLEND_MODE(0x00309e045ffefdf8LL); // Kirby some parts of the Ground
			BLEND_MODE(0x0030b2045ffefff8LL); // OOT - Eponas Dust
			BLEND_MODE(0x0030b26144664924LL); // Duke 3D and Mario Head
			BLEND_MODE(0x0030b2615566db6dLL); // Kirby Air seeds, Ridge racer text
			BLEND_MODE(0x0030b3ff5ffeda38LL); // OOT Sign Cut (Sword)
			BLEND_MODE(0x0030e2045f1af47bLL); // Animal Crossing - River
			BLEND_MODE(0x0030ec6155daed76LL); // Cucukan Egg
			BLEND_MODE(0x0030ec045fdaedf6LL); // Zelda Arrows in Shop
			BLEND_MODE(0x0030fe045f0ef3ffLL); // Gold Skulltula Eyes
			BLEND_MODE(0x0030fe045ffef7f8LL); // MRC - Car Windows
			BLEND_MODE(0x0030fe045ffef3f8LL); // Zelda Bottle Decal / Ridge Racer Fences
			BLEND_MODE(0x0030fe045ffefbf8LL); // FZero main ship
			BLEND_MODE(0x0030fe045ffefdf8LL); // Kirby Ground
			BLEND_MODE(0x0030fe045ffefdfeLL); // Zelda Kokori Sword Handle
			BLEND_MODE(0x003096045ffefff8LL); // Pokemon Stadium - Balloons
			BLEND_MODE(0x00322bff5f0e923fLL); // Paper Mario Fireblast
			BLEND_MODE(0x003432685566ff7fLL); // Ogre Battle - Intro Dust / Conker - Chainsaw Smoke
			BLEND_MODE(0x00373c6e117b9fcfLL); // OOT - Lens of Truth
			BLEND_MODE(0x0040fe8155fef97cLL); // GoldenEye Sky
			BLEND_MODE(0x0041c2835587dfefLL); // Command $ Conquer - Smoke
			//BLEND_MODE(0x0040fe8155fefd7eLL); // Kirby Far Terrain -> Breaks Rocket-robot on wheels
			BLEND_MODE(0x0050d2a133a5b6dbLL); // Pokemon Stadium 2 Pokemon Select Box
			BLEND_MODE(0x005196a3112cfe7fLL); // WWF Wreslemania 2000 - Menu
			BLEND_MODE(0x00541aa83335feffLL); // Sin and Punishment Grass
			BLEND_MODE(0x00547ea833fdf2f9LL); // Sin and Punishment - Ground
			BLEND_MODE(0x00551aaa1134fe7fLL); // Sin and Punishment - Particles and Explosions
			BLEND_MODE(0x0055a68730fd923eLL); // F1 World GP Sky
			BLEND_MODE(0x005616ac112cfe7fLL); // DKR Dialog Text
			BLEND_MODE(0x00567e034f0e77ffLL); // DKR Turtle Shell
			BLEND_MODE(0x00567e034f0e79ffLL); // DKR car slids
			BLEND_MODE(0x0061a5ff1f10d23fLL); // Paper Mario - Intro Lighting
			BLEND_MODE(0x00627fff1ffcfc38LL); // Pilot Wings 64 sky
			BLEND_MODE(0x00629bff1ffcfe38LL); // Quest 64 - Bubbles
			BLEND_MODE(0x0062fe043f15f9ffLL); // Banjo Kazooie Backdrop
			BLEND_MODE(0x00671603fffcff78LL); // DOOM64 weapons
			BLEND_MODE(0x0071fee344fe793cLL); // Pokemon Stadium 2 Text box
			BLEND_MODE(0x0071fffffffefc38LL); // Pokemon Stadium 2 Pokemon Select Menu
			BLEND_MODE(0x00772c60f5fce378LL); // Zelda Poe
			BLEND_MODE(0x0077666045fd7f78LL); // Pokemon Stadium 2 Intro Pichu
			BLEND_MODE(0x00f517eaff2fffffLL); // Killer Instinct Shadows
			BLEND_MODE(0x00ff95ffff0dfe3fLL); // Animal Crossing Player Shadow
			BLEND_MODE(0x00ffabffff0d92ffLL); // Paper Mario - Walking Dust
			BLEND_MODE(0x00ffac80ff0d93ffLL); // Animal Crossing - Running Smoke
			BLEND_MODE(0x00ffb3ffff00fe3fLL); // Mega Man 64 Explosion
			BLEND_MODE(0x00fff3fffffdb638LL); // SW racer EP1 shadows
			BLEND_MODE(0x00fffe8ff517f8ffLL); // Conker Mouth/Tail
			default:
				return BlendMode_Generic;	  // Basic generic blenmode

	#undef BLEND_MODE
	}

	return nullptr;
}
