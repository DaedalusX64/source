/*
Copyright (C) 2005 StrmnNrmn

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
#include "Utility/Timing.h"

#include <vitasdk.h>

namespace NTiming {

bool GetPreciseFrequency( u64 * p_freq )
{
	*p_freq = ::sceRtcGetTickResolution();
	return true;
}

bool GetPreciseTime( u64 * p_time )
{
	SceRtcTick tick;
	if(::sceRtcGetCurrentTick( &tick ) == 0)
	{
		*p_time = tick.tick;
		return true;
	}

	*p_time = 0;
	return false;
}

u64 ToMilliseconds( u64 ticks )
{
	static u64 tick_resolution = sceRtcGetTickResolution();
	return (ticks*1000LL) / tick_resolution;
}

} // NTiming
