/*
Copyright (C) 2012 Salvy6735
Copyright (C) 2012 StrmnNrmn

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


#include <vector>
#include <iostream>
#include <string>
#if defined(GCC_LEGACY)
#include <fmt/core.h> 
#else 
#include <format>
#endif
#include <fstream> 
#include <sstream>

#include "Utility/Paths.h"
#include "Utility/StringUtil.h"
#include "Utility/Translate.h"
#include "Utility/VolatileMem.h"



#include "Base/Macros.h"
#define TRANSLATE_DUMP_VALUE 0xDAEDDAED
//*****************************************************************************
//
//*****************************************************************************
struct pTranslate
{
	u32		hash;			// hash that corresponds to string
//	u32		len;			// lenght of the translated string
	char	*translated;	// Translated string
};

pTranslate				 text[180];
std::vector<std::string> gLanguage;
//
// Hash was taken from http://stackoverflow.com/questions/98153/whats-the-best-hashing-algorithm-to-use-on-a-stl-string-when-using-hash-map
//
//*****************************************************************************
//
//*****************************************************************************
u32 HashString(const std::string& s) {
    u32 hash = 0;
    for (char c : s) {
        hash = hash * 101 + c;
    }
    return hash;
}

//*****************************************************************************
//
//*****************************************************************************
const char * Translate_Strings(const std::string& original, u32 & len)
{
	u32 hash = HashString(original);
	if( hash == 0 )
		return original.data();

	for( u32 i=0; i < std::size(text); i++ )
	{
		// ToDo..
		//DAEDALUS_ASSERT( text[i].translated != original, " String already translated" );

		if( text[i].hash == hash )
		{
			if( text[i].translated )
			{
				len =  strlen( text[i].translated );
				return text[i].translated;
			}
			else
				return original.data();
		}
	}
	return original.data();
}

//*****************************************************************************
//
//*****************************************************************************
const char * Translate_String(const char *original)
{
	u32 dummy;
	return Translate_Strings( original, dummy );
}

//*****************************************************************************
//
//*****************************************************************************
void Translate_Unload()
{
	// Clear translations
	for( u32 i = 0; i < std::size(text); ++i )
	{
		if( text[i].translated != NULL )
		{
			free_volatile(text[i].translated);
			text[i].translated = NULL;
		}
	}
}
//*****************************************************************************
//
//*****************************************************************************
bool	Translate_Init()
{
	// Init translations if available
	Translate_Load("Languages/" );

	return /*gLanguage.empty() == 0*/ true;
}

//*****************************************************************************
//
//*****************************************************************************
void	Translate_Load( const std::filesystem::path& p_dir )
{
	// Set default language
	gLanguage.push_back("English");

	for (auto const& dir_entry : std::filesystem::directory_iterator(p_dir))
	{
		if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".lng")
		{
			std::string filename = dir_entry.path().stem().string();
			gLanguage.push_back(filename);
		}
	}
}

//*****************************************************************************
//
//*****************************************************************************
const char * Translate_Name(u32 idx)
{
	if( idx < gLanguage.size())
	{
		return gLanguage[ idx ].c_str();
	}

	return "?";
}

//*****************************************************************************
//
//*****************************************************************************
u32 Translate_Number()
{
	return gLanguage.size()-1;
}

//*****************************************************************************
//
//*****************************************************************************
u32	Translate_IndexFromName( const char * name )
{
	for( u32 i = 0; i < gLanguage.size(); ++i )
	{
		if( strcasecmp(  gLanguage[ i ].c_str(), name ) == 0 )
		{
			return i;
		}
	}

	// Default language (English)
	return 0;
}

//*****************************************************************************
//
//*****************************************************************************
const char * Translate_NameFromIndex( u32 idx )
{
	if( idx < gLanguage.size())
	{
		return gLanguage[ idx ].c_str();
	}

	return "?";
}

//*****************************************************************************
// Restores escape characters which were removed when parsing
// Which are needed by line-breaking and back-slash
//*****************************************************************************
std::string Restore(std::string s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\') {
            if (s[i + 1] == 'n') {
                s[i + 1] = '\b'; 
                s[i] = '\n';
                i++;
            } else if (s[i + 1] == '\\') {
                s[i + 1] = '\b'; 
                s[i] = '\\';
                i++;
            }
        }
    }
    return s;
}

//*****************************************************************************
//
//*****************************************************************************
void Translate_Dump(const std::string string, bool dump)
{
	if(dump)
	{
	std::fstream fh("hash.txt", std::ios::in);

	if (fh.is_open())
		{
		#if defined(GCC_LEGACY)
		fh << fmt::format("{:08x},{}\n", HashString(string), string);
		#else 
		fh << std::format("{:08x},{}\n", HashString(string), string);
		#endif
			
		}
	}
}

//*****************************************************************************
//
//*****************************************************************************
bool Translate_Read(u32 idx, const std::filesystem::path& dir)
{
	/// Always unload previous language file if available
	Translate_Unload();

	if( idx > gLanguage.size() )
		return false;

	std::string line;

	char *string;

	u32 count = 0;
	u32 hash  = 0;
	u32	len   = 0;

	// Build path where we'll load the translation file(s)
	
	const std::string languageFile = "Languages/" + gLanguage[idx] + ".lng";
	const std::filesystem::path& path = setBasePath(languageFile);
	

	std::cout << "Language Path: " << path << std::endl;
	std::fstream stream(path, std::ios::in);

	if (!stream.is_open())
	{
		return false;
	}


  while (std::getline(stream, line)) {
        // Strip spaces from end of lines
        line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

        // Handle comments
        if (line.empty() || line[0] == '/')
            continue;

        size_t commaPos = line.find(',');
        if (commaPos != std::string::npos) {
            std::string hashString = line.substr(0, commaPos);
            std::string string = line.substr(commaPos + 1);

            unsigned int hash;
            std::stringstream ss;
            ss << std::hex << hashString;
            ss >> hash;
			constexpr size_t TEXT_ARRAY_SIZE = 1024; 
            size_t len = string.length();
            if (count < TEXT_ARRAY_SIZE) {
                // Write translated string and hash to array
                text[count].hash = hash;
                Translate_Dump(string, hash == TRANSLATE_DUMP_VALUE);

                text[count].translated = (char*)malloc(len + 1); // Leave space for terminator
                strcpy(text[count].translated, Restore(string, len).c_str());
                count++;
            }
        }
    }

	return true;
}
