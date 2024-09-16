/*
Copyright (C) 2006 StrmnNrmn

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
#include "Utility/IO.h"

#include <psp2/io/fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>

namespace IO
{
	struct SceIoDirentGuarded
	{
		SceIoDirent Dirent;
		char Guard[256];			// Not sure, but it looks like SceIoDirent is somehow mis-declared in the pspsdk, so it ends up scribbling over stack?
	};
	SceIoDirentGuarded	gDirEntry;

	const char gPathSeparator( '/' );
	namespace File
	{
		bool	Move( const char * p_existing, const char * p_new )
		{
			return sceIoRename( p_existing, p_new ) >= 0;
		}

		bool	Delete( const char * p_file )
		{
			return sceIoRemove( p_file ) >= 0;
		}

		bool	Exists( const char * p_path )
		{
			FILE * fh = fopen( p_path, "rb" );
			if ( fh )
			{
				fclose( fh );
				return true;
			}
			else
			{
				return false;
			}
		}

		int	Stat( const char *p_file, SceIoStat *stat )
		{
			return sceIoGetstat ( p_file, stat );
		}
	}
	namespace Directory
	{
		bool	Create( const char * p_path )
		{
			return sceIoMkdir( p_path, 0777 ) == 0;
		}

		bool	EnsureExists( const char * p_path )
		{
			if ( IsDirectory(p_path) )
				return true;

			// Make sure parent exists,
			IO::Filename	p_path_parent;
			IO::Path::Assign( p_path_parent, p_path );
			IO::Path::RemoveBackslash( p_path_parent );
			if( IO::Path::RemoveFileSpec( p_path_parent ) )
			{
				//
				//	Recursively create parents. Need to be careful of stack overflow
				//
				if( !EnsureExists( p_path_parent ) )
					return false;
			}

			return Create( p_path );
		}

		bool	IsDirectory( const char * p_path )
		{
			struct stat		s;

			if(stat( p_path, &s ) == 0)
			{
				if(s.st_mode & _IFDIR)
				{
					return true;
				}
			}

			return false;
		}
	}

	namespace Path
	{
		char *	Combine( char * p_dest, const char * p_dir, const char * p_file )
		{
			strcpy( p_dest, p_dir );
			Append( p_dest, p_file );
			return p_dest;
		}

		bool	Append( char * p_path, const char * p_more )
		{
			u32		len( strlen(p_path) );

			if(len > 0)
			{
				if(p_path[len-1] != gPathSeparator)
				{
					p_path[len] = gPathSeparator;
					p_path[len+1] = '\0';
					len++;
				}
			}
			strcat( p_path, p_more );
			return true;
		}

		const char *  FindExtension( const char * p_path )
		{
			return strrchr( p_path, '.' );
		}

		const char *	FindFileName( const char * p_path )
		{
			char * p_last_slash = strrchr( p_path, gPathSeparator );
			if ( p_last_slash )
			{
				return p_last_slash + 1;
			}
			else
			{
				return p_path;
			}
		}

		char *	RemoveBackslash( char * p_path )
		{
			u32 len = strlen( p_path );
			if ( len > 0 )
			{
				char * p_last_char( &p_path[ len - 1 ] );
				if ( *p_last_char == gPathSeparator )
				{
					*p_last_char = '\0';
				}
				return p_last_char;
			}
			return nullptr;
		}

		bool	RemoveFileSpec( char * p_path )
		{
			char * last_slash = strrchr( p_path, gPathSeparator );
			if ( last_slash )
			{
				*last_slash = '\0';
				return true;
			}
			return false;
		}

		void	RemoveExtension( char * p_path )
		{
			char * ext = const_cast< char * >( FindExtension( p_path ) );
			if ( ext )
			{
				*ext = '\0';
			}
		}

		void	AddExtension( char * p_path, const char * p_ext )
		{
			strcat( p_path, p_ext );
		}


		int DeleteRecursive(const char * p_path, const char * p_extension)
		{
			SceUID fh;

			IO::Filename file;
			fh = sceIoDopen(p_path);

			if( fh )
			{
				while(sceIoDread( fh, &gDirEntry.Dirent ))
				{
					SceIoStat stat;
					IO::Path::Combine(file, p_path, gDirEntry.Dirent.d_name);

					sceIoGetstat( file, &stat );
					if( (stat.st_mode & 0x1000) == 0x1000 )
					{
						if(strcmp(gDirEntry.Dirent.d_name, ".") && strcmp(gDirEntry.Dirent.d_name, ".."))
						{
							//printf("Found directory\n");
						}
					}
					else
					{
						if (_strcmpi(FindExtension( file ), p_extension) == 0)
						{
							//DBGConsole_Msg(0, "Deleting [C%s]",file);
							sceIoRemove( file );
						}

					}
				}
				sceIoDclose( fh );
			}
			else
			{
				//DBGConsole_Msg(0, "Couldn't open the directory");
			}

			return 0;
		}
	}

	bool	FindFileOpen( const char * path, FindHandleT * handle, FindDataT & data )
	{
		*handle = sceIoDopen( path );
		if( *handle >= 0 )
		{
			// To support findfirstfile() API we must return the first result immediately
			if( FindFileNext( *handle, data ) )
			{
				return true;
			}

			// Clean up
			sceIoDclose( *handle );
		}

		return false;
	}

	bool	FindFileNext( FindHandleT handle, FindDataT & data )
	{
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( handle >= 0, "Cannot search with invalid directory handle" );
		#endif

		if( sceIoDread( handle, &gDirEntry.Dirent ) > 0 )
		{
			IO::Path::Assign( data.Name, gDirEntry.Dirent.d_name );
			return true;
		}

		return false;
	}

	bool	FindFileClose( FindHandleT handle )
	{
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( handle >= 0, "Trying to close an invalid directory handle" );
		#endif
		return ( sceIoDclose( handle ) >= 0 );
	}

}
