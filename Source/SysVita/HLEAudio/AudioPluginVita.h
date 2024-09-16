/*
Copyright (C) 2007 StrmnNrmn

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

#ifndef SYSPSP_HLEAUDIO_AUDIOPLUGINVITA_H_
#define SYSPSP_HLEAUDIO_AUDIOPLUGINVITA_H_

#include "Plugins/AudioPlugin.h"

class AudioOutput;

class CAudioPluginVita : public CAudioPlugin
{
private:
	CAudioPluginVita();
public:
	static CAudioPluginVita *		Create();


	virtual ~CAudioPluginVita();
	virtual bool			StartEmulation();
	virtual void			StopEmulation();

	virtual void			DacrateChanged( int SystemType );
	virtual void			LenChanged();
	virtual u32				ReadLength();
	virtual EProcessResult	ProcessAList();

//			void			SetAdaptFrequecy( bool adapt );

private:
	AudioOutput *			mAudioOutput;
};


#endif // SYSPSP_HLEAUDIO_AUDIOPLUGINVITA_H_

