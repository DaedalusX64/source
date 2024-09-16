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

#include "stdafx.h"

#include <stdio.h>
#include <stdarg.h>
#include "Debug/DaedalusAssert.h"

#ifdef DAEDALUS_ENABLE_ASSERTS

extern void log2file(const char *format, ...);

EAssertResult DaedalusAssert(const char * expression, const char * file, unsigned int line, const char * msg, ...);

DaedalusAssertHook gAssertHook = &DaedalusAssert;
//
//	Return -1 to ignore once, 0 to ignore permenantly, 1 to break
//
EAssertResult DaedalusAssert(const char * expression, const char * file, unsigned int line, const char * msg, ...)
{
	char buffer[512];
	__gnuc_va_list va;
	va_start(va, msg);
	vsnprintf( buffer, 512, msg, va );
	buffer[1023] = 0;
	va_end(va);
	
	log2file( "************************************************************\n" );
	log2file( "Assert Failed: %s\n", expression );
	log2file( "Location: %s(%d)\n", file, line );
	log2file( "\n" );
	log2file( "%s\n", buffer );
	log2file( "\n" );
	
	return AR_IGNORE;
}

#endif //DAEDALUS_ENABLE_ASSERTS
