.arm

//The top two defines needs to be adjusted depending on how gCPUState struct is formated in CPU.h!! //Corn
//
#define _C0_Count	(0x100 + 9 * 4)	//CPU_Control_base + 9*8(64bit regs) or 9*4(32bit regs)
#define _AuxBase	0x280	//Base pointer to Aux regs
#define _CurrentPC	(_AuxBase + 0x00)
#define _TargetPC	(_AuxBase + 0x04)
#define _Delay		(_AuxBase + 0x08)
#define _StuffToDo	(_AuxBase + 0x0c)
#define _Temp1		(_AuxBase + 0x20)
#define _Temp2		(_AuxBase + 0x24)
#define _Temp3		(_AuxBase + 0x28)
#define _Temp4		(_AuxBase + 0x2C)
#define _Events		(_AuxBase + 0x30)

.extern CPU_UpdateCounter
.extern CPU_HANDLE_COUNT_INTERRUPT
.extern g_MemoryLookupTableRead
.extern g_MemoryLookupTableWrite
.extern HandleException_extern
.extern Write32BitsForDynaRec
.extern Write16BitsForDynaRec
.extern Write8BitsForDynaRec
	
.align 4
.global _EnterDynaRec
.global _DirectExitCheckNoDelay
.global _DirectExitCheckDelay
.global _IndirectExitCheck

	.global _ReadBitsDirect_u8
	.global _ReadBitsDirect_s8
	.global _ReadBitsDirect_u16
	.global _ReadBitsDirect_s16
	.global _ReadBitsDirect_u32

	.global _ReadBitsDirectBD_u8
	.global _ReadBitsDirectBD_s8
	.global _ReadBitsDirectBD_u16
	.global _ReadBitsDirectBD_s16
	.global _ReadBitsDirectBD_u32

	.global _WriteBitsDirect_u32
	.global _WriteBitsDirect_u16
	.global _WriteBitsDirect_u8
	.global _WriteBitsDirectBD_u32
	.global _WriteBitsDirectBD_u16
	.global _WriteBitsDirectBD_u8
	
	
.type _EnterDynaRec, %function
.type _DirectExitCheckNoDelay, %function
.type _DirectExitCheckDelay, %function
.type _IndirectExitCheck, %function

_DirectExitCheckNoDelay:
    ldr r4, [r12, #_C0_Count]	// COUNT register
    ldr r5, [r12, #_Events]		// Events[0].mCount
	
	add r4, r4, r0				// COUNT + ops_executed
	str r4, [r12, #_C0_Count]	// COUNT = COUNT + ops_executed
	
	str r1, [r12, #_CurrentPC]	// Current PC
	mov r1, #0
	str r1, [r12, #_Delay]		// Delay = NO_DELAY
	
	sub r5, r5, r0				// Events[0].mCount - ops_executed
	cmp r5, #0
	str r5, [r12, #_Events]
	ble	_DirectExitCheckCheckCount
	bx  lr

_DirectExitCheckDelay:
    ldr r4, [r12, #_C0_Count]	// COUNT register
    ldr r5, [r12, #_Events]		// Events[0].mCount
	
	add r4, r4, r0				// COUNT + ops_executed
	str r4, [r12, #_C0_Count]	// COUNT = COUNT + ops_executed
	
	str r1, [r12, #_CurrentPC]	// Current PC
	str r2, [r12, #_TargetPC]	// Target PC
	mov r1, #1					// EXEC_DELAY
	str r1, [r12, #_Delay]		// Delay = NO_DELAY
	
	sub r5, r5, r0				// Events[0].mCount - ops_executed
	cmp r5, #0
	str r5, [r12, #_Events]
	ble	_DirectExitCheckCheckCount
	bx  lr

#######################################################################################
#	Utility routine for _DirectExitCheckXX.
#
_DirectExitCheckCheckCount:
	mov		r4, lr		// Keep track of return addresss
	mov		r5, r12
	bl		CPU_HANDLE_COUNT_INTERRUPT
	mov		lr,  r4
	mov		r12, r5
	ldr		r0, [r12, #_StuffToDo]		// StuffToDo
	cmp		r0, #0
	vpopne	{d0-d15}
	popne	{r4-r12, pc}		// Exit the DynaRec
	bx		lr					// Return back to caller

#######################################################################################
#	Update counter. If StuffToDo flags is clear on return,
#	r0 - instructions executed
#	r1 - CIndirectExitMap pointer
#	r2 - exit pc (exit delay is always NO_DELAY)
_IndirectExitCheck:
	mov		r4, r1		// Keep track of map pointer
	mov		r5, r2		// and the exit pc
	mov		r6, r12
	# Can avoid these until Return From DynaRec (pop)?
	str		r2, [r12,#_CurrentPC] 	// CurrentPC
	bl		CPU_UpdateCounter		// a0 holds instructions executed
	mov		r0, #0
	str		r0, [r6, #_Delay]		// Delay (NO_DELAY)

	ldr		r0, [r6, #_StuffToDo]	//  StuffToDo
	cmp		r0, #0
	vpopne	{d0-d15}
	popne	{r4-r12,pc}				// Exit the DynaRec

	mov		r0, r4					//	 p_map
	mov		r1, r5					// exit_pc
	bl		IndirectExitMap_Lookup


	# r0 holds pointer to indirect target. If it's 0, it means it's not compiled yet
	cmp		r0, #0
	vpopeq  {d0-d15}
	popeq   {r4-r12,pc} 			// Exit the DynaRec
	mov     r12,r6					// Restore the CPUState pointer
	bx		r0						// branch to the looked up fragment

_ReturnFromDynaRecAndHandleException:
	bl    HandleException_extern
	vpop  {d0-d15}
	pop   {r4-r12,pc}
	
.macro READ_BITS	function, load_instruction
\function:
	// look up memread function
	lsr		r2, r0, #0x12
	ldr		r3, =g_MemoryLookupTableRead
	add		r2, r3, r2, lsl #3 // * 8 to index the two pointer struct
	ldr		r2, [r2,#4]	//offset 4 to get the second pointer
	
	str		r1, [r12,#_CurrentPC] 	// CurrentPC
	
	// execute memread function
	mov		r5, r12
	mov		r6, lr
	blx		r2
	mov		lr, r6
	mov		r12, r5
	
	// check exceptions
	ldr     r1, [r12, #_StuffToDo]
	cmp     r1, #0
	bne     _ReturnFromDynaRecAndHandleException
	
	//execute the load
	\load_instruction	r0, [r0]		// e.g. lbu, lhu, lw etc
	
	// return
	bx lr
.endm

.macro READ_BITS_BD	function, load_instruction
\function:
	// look up memread function
	lsr		r2, r0, #0x12
	ldr		r3, =g_MemoryLookupTableRead
	add		r2, r3, r2, lsl #3 // * 8 to index the two pointer struct
	ldr		r2, [r2,#4]	//offset 4 to get the second pointer
	
	str		r1, [r12,#_CurrentPC] 	// CurrentPC
	
	mov		r3, #1 // EXEC_DELAY
	str		r3, [r12, #_Delay]
	
	// execute memread function
	mov		r5, r12
	mov		r6, lr
	blx		r2
	mov		lr, r6
	mov		r12, r5
	
	// check exceptions
	ldr     r1, [r12, #_StuffToDo]
	cmp     r1, #0
	bne     _ReturnFromDynaRecAndHandleException

	mov     r1, #0
	str     r1, [r12, #_Delay]
	
	// execute the load
	\load_instruction	r0, [r0]		// e.g. lbu, lhu, lw etc
	
	// return
	bx lr
.endm

	READ_BITS _ReadBitsDirect_u8, ldrb
	READ_BITS _ReadBitsDirect_s8, ldrsb
	READ_BITS _ReadBitsDirect_u16, ldrh
	READ_BITS _ReadBitsDirect_s16, ldrsh
	READ_BITS _ReadBitsDirect_u32, ldr
	
	READ_BITS_BD _ReadBitsDirectBD_u8, ldrb
	READ_BITS_BD _ReadBitsDirectBD_s8, ldrsb
	READ_BITS_BD _ReadBitsDirectBD_u16, ldrh
	READ_BITS_BD _ReadBitsDirectBD_s16, ldrsh
	READ_BITS_BD _ReadBitsDirectBD_u32, ldr

.macro WRITE_BITS	function, store_func
\function:
	str		r2, [r12,#_CurrentPC] 	// CurrentPC
	
	// do the write 
	mov  r5, r12
	mov  r6, lr
	bl		\store_func
	mov  lr, r6
	mov	  r12, r5
	
	// check exceptions
	ldr     r1, [r12, #_StuffToDo]
	cmp     r1, #0
	bne     _ReturnFromDynaRecAndHandleException

	bx lr
.endm

.macro WRITE_BITS_BD	function, store_func
\function:
	str		r2, [r12,#_CurrentPC] 	// CurrentPC
	
	mov		r2, #1 // EXEC_DELAY
	str		r2, [r12, #_Delay]
	
	// do the write 
	mov  r5, r12
	mov  r6, lr
	bl		\store_func
	mov  lr, r6
	mov	  r12, r5
	
	
	// check exceptions
	ldr     r1, [r12, #_StuffToDo]
	cmp     r1, #0
	bne     _ReturnFromDynaRecAndHandleException
	
	mov     r0, #0
	str     r0, [r12, #_Delay]
	
	bx		lr
.endm



	WRITE_BITS _WriteBitsDirect_u8, Write8BitsForDynaRec
	WRITE_BITS _WriteBitsDirect_u16, Write16BitsForDynaRec
	WRITE_BITS _WriteBitsDirect_u32, Write32BitsForDynaRec

	WRITE_BITS_BD _WriteBitsDirectBD_u8, Write8BitsForDynaRec
	WRITE_BITS_BD _WriteBitsDirectBD_u16, Write16BitsForDynaRec
	WRITE_BITS_BD _WriteBitsDirectBD_u32, Write32BitsForDynaRec
	
_EnterDynaRec:
    push {r4-r12, lr}
    vpush {d0-d15}
    mov r12, r1
    mov r10, r2
    mov r9,  r3
    bx  r0