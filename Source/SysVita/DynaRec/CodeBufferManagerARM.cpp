/*
Copyright (C) 2020 MasterFeizz

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
#include <malloc.h>
#if 0
#include <kubridge.h>
#endif
#include "DynaRec/CodeBufferManager.h"
#include "Debug/DBGConsole.h"
#include "CodeGeneratorARM.h"

#define CODE_BUFFER_SIZE (8 * 1024 * 1024)

static SceUID dynarec_memblock;

void _DaedalusICacheInvalidate(const void * address, u32 length)
{
	if(length > 0)
#if 1
		sceKernelSyncVMDomain(dynarec_memblock, (void*)address, length);
#else
		kuKernelFlushCaches((void *)address, length);
#endif
}

class CCodeBufferManagerARM : public CCodeBufferManager
{
public:
	CCodeBufferManagerARM()
		:	mpBuffer( NULL )
		,	mBufferPtr( 0 )
		,	mBufferSize( 0 )
		,	mpSecondBuffer( NULL )
		,	mSecondBufferPtr( 0 )
		,	mSecondBufferSize( 0 )
	{
	}

	virtual bool			Initialise();
	virtual void			Reset();
	virtual void			Finalise();

	virtual CCodeGenerator *StartNewBlock();
	virtual u32				FinaliseCurrentBlock();

private:

	u8	*					mpBuffer;
	u32						mBufferPtr;
	u32						mBufferSize;

	u8 *					mpSecondBuffer;
	u32						mSecondBufferPtr;
	u32						mSecondBufferSize;

private:
	CAssemblyBuffer			mPrimaryBuffer;
	CAssemblyBuffer			mSecondaryBuffer;
};

//*****************************************************************************
//
//*****************************************************************************
CCodeBufferManager *	CCodeBufferManager::Create()
{
	return new CCodeBufferManagerARM;
}

//*****************************************************************************
//
//*****************************************************************************
bool	CCodeBufferManagerARM::Initialise()
{
	// mpSecondBuffer is currently unused
#if 1
	dynarec_memblock = sceKernelAllocMemBlockForVM("code", CODE_BUFFER_SIZE * 2);
#else
	SceKernelAllocMemBlockKernelOpt opt;
	memset(&opt, 0, sizeof(SceKernelAllocMemBlockKernelOpt));
	opt.size = sizeof(SceKernelAllocMemBlockKernelOpt);
	opt.attr = 0x1;
	opt.field_C = (SceUInt32)0x98000000;
	dynarec_memblock = kuKernelAllocMemBlock("code", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, CODE_BUFFER_SIZE * 2, &opt);
#endif
	sceKernelGetMemBlockBase(dynarec_memblock, (void**)&mpBuffer);

	mpSecondBuffer = mpBuffer + CODE_BUFFER_SIZE;
	if (mpBuffer == NULL || mpSecondBuffer == NULL)
		return false;
#if 1
	sceKernelOpenVMDomain();
#else
	kuKernelMemProtect(mpBuffer, CODE_BUFFER_SIZE * 2, KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_WRITE | KU_KERNEL_PROT_READ);
#endif
	mBufferPtr = 0;
	mBufferSize = 0;

	mSecondBufferPtr = 0;
	mSecondBufferSize = 0;

	return true;
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeBufferManagerARM::Reset()
{
	mBufferPtr = 0;
	mSecondBufferPtr = 0;
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeBufferManagerARM::Finalise()
{
	if (mpBuffer != NULL && mpSecondBuffer != NULL)
	{
		sceKernelFreeMemBlock(dynarec_memblock);

		mpBuffer = NULL;
		mpSecondBuffer = NULL;
	}
}

//*****************************************************************************
//
//*****************************************************************************
CCodeGenerator * CCodeBufferManagerARM::StartNewBlock()
{
	// Round up to 16 byte boundry
	u32 aligned_ptr( (mBufferPtr + 15) & (~15) );

	mBufferPtr = aligned_ptr;

	u32 aligned_ptr2((mSecondBufferPtr + 15) & (~15));
	mSecondBufferPtr = aligned_ptr2;

	mPrimaryBuffer.SetBuffer( mpBuffer + mBufferPtr );
	mSecondaryBuffer.SetBuffer( mpSecondBuffer + mSecondBufferPtr );

	return new CCodeGeneratorARM( &mPrimaryBuffer, &mSecondaryBuffer );
}

//*****************************************************************************
//
//*****************************************************************************
u32 CCodeBufferManagerARM::FinaliseCurrentBlock()
{
	const u32 primary_block_size( mPrimaryBuffer.GetSize() );
	const u8* p_primary_base( mPrimaryBuffer.GetStartAddress().GetTargetU8P() );

	const u32 secondary_block_size( mSecondaryBuffer.GetSize() );
	const u8* p_secondary_base( mSecondaryBuffer.GetStartAddress().GetTargetU8P() );

	mBufferPtr += primary_block_size;
	mSecondBufferPtr += secondary_block_size;

	#if 0 //Second buffer is currently unused
	mSecondBufferPtr += mSecondaryBuffer.GetSize();
	mSecondBufferPtr = ((mSecondBufferPtr - 1) & 0xfffffff0) + 0x10; // align to 16-byte boundary
	#endif

	_DaedalusICacheInvalidate(p_primary_base, primary_block_size);
	_DaedalusICacheInvalidate(p_secondary_base, secondary_block_size);

	return primary_block_size;
}
