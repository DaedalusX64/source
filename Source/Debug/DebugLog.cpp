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


#include "Base/Types.h"

#include "Debug/DebugLog.h"
#include "Debug/Dump.h"
#include "Debug/DBGConsole.h"


#include <fstream>
#if defined(GCC_LEGACY)
#include <fmt/core.h> 
#else 
#include <format>
#endif
#include <iostream> 
#include "Utility/Paths.h"
#ifdef DAEDALUS_LOG


static bool			g_bLog = true;
std::ofstream	g_hOutputLog;;


bool Debug_InitLogging()
{
	const std::filesystem::path log_filename = "daedalus.txt";
	std::filesystem::path path = setBasePath(log_filename);
	std::cout << "Creating Dump File: " << path << std::endl;
	g_hOutputLog.open( path);
	// Is always going to return true
	return true;
}


void Debug_FinishLogging() {}


// void Debug_Print(const char* format, ...)
// {
//         va_list args;
//         va_start(args, format);
//         std::string formattedString = std::vformat(format, std::make_format_args(args));
//         va_end(args);
		
//         g_hOutputLog << formattedString << '\n';
// }



#endif // DAEDALUS_LOG
