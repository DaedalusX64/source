#include "stdafx.h"
#include "Input/InputManager.h"

#include <stack>
#include <string>
#include <vector>

#include <vitasdk.h>

#include "Config/ConfigOptions.h"
#include "Debug/DBGConsole.h"
#include "Math/Math.h"	// VFPU Math
#include "Math/MathUtil.h"
#include "Utility/IniFile.h"
#include "Utility/IO.h"
#include "Utility/Macros.h"
#include "Utility/Preferences.h"
#include "Utility/Stream.h"
#include "Utility/Synchroniser.h"

// Using uninterceptable buttons as aliases for right stick
#define SCE_CTRL_RUP    SCE_CTRL_HEADPHONE
#define SCE_CTRL_RDOWN  SCE_CTRL_VOLUP
#define SCE_CTRL_RLEFT  SCE_CTRL_VOLDOWN
#define SCE_CTRL_RRIGHT SCE_CTRL_POWER

extern bool gUseRearpad;

v2	ApplyDeadzone( const v2 & in, f32 min_deadzone, f32 max_deadzone );

namespace
{
	static const f32					DEFAULT_MIN_DEADZONE = 0.25f;
	static const f32					DEFAULT_MAX_DEADZONE = 1.f;

	static const s32					N64_ANALOGUE_STICK_RANGE( 80 );
	static const s32					VITA_ANALOGUE_STICK_RANGE( 128 );


class CButtonMapping
{
	public:
		virtual ~CButtonMapping() {}
		virtual bool	Evaluate( u32 vita_buttons ) const = 0;
};

class CButtonMappingMask : public CButtonMapping
{
	public:
		CButtonMappingMask( u32 mask ) : mMask( mask ) {}

		virtual bool	Evaluate( u32 vita_buttons ) const		{ return ( vita_buttons & mMask ) != 0; }

	private:
		u32					mMask;
};

class CButtonMappingNegate : public CButtonMapping
{
	public:
		CButtonMappingNegate( CButtonMapping * p_mapping )
			:	mpMapping( p_mapping )
		{
		}

		~CButtonMappingNegate()
		{
			delete mpMapping;
		}

		virtual bool	Evaluate( u32 vita_buttons ) const
		{
			return !mpMapping->Evaluate( vita_buttons );
		}

	private:
		CButtonMapping *	mpMapping;
};

class CButtonMappingAnd : public CButtonMapping
{
	public:
		CButtonMappingAnd( CButtonMapping * p_a, CButtonMapping * p_b )
			:	mpMappingA( p_a )
			,	mpMappingB( p_b )
		{
		}

		~CButtonMappingAnd()
		{
			delete mpMappingA;
			delete mpMappingB;
		}

		virtual bool	Evaluate( u32 vita_buttons ) const
		{
			return mpMappingA->Evaluate( vita_buttons ) &&
				   mpMappingB->Evaluate( vita_buttons );
		}

	private:
		CButtonMapping *	mpMappingA;
		CButtonMapping *	mpMappingB;
};


class CButtonMappingOr : public CButtonMapping
{
	public:
		CButtonMappingOr( CButtonMapping * p_a, CButtonMapping * p_b )
			:	mpMappingA( p_a )
			,	mpMappingB( p_b )
		{
		}

		~CButtonMappingOr()
		{
			delete mpMappingA;
			delete mpMappingB;
		}

		virtual bool	Evaluate( u32 vita_buttons ) const
		{
			return mpMappingA->Evaluate( vita_buttons ) ||
				   mpMappingB->Evaluate( vita_buttons );
		}

	private:
		CButtonMapping *	mpMappingA;
		CButtonMapping *	mpMappingB;
};

enum EN64Button
{
	N64Button_A = 0,
	N64Button_B,
	N64Button_Z,
	N64Button_L,
	N64Button_R,
	N64Button_Up,
	N64Button_Down,
	N64Button_Left,
	N64Button_Right,
	N64Button_CUp,
	N64Button_CDown,
	N64Button_CLeft,
	N64Button_CRight,
	N64Button_Start,
	NUM_N64_BUTTONS
};

const u32 gN64ButtonMasks[NUM_N64_BUTTONS] =
{
	A_BUTTON,		//N64Button_A = 0,
	B_BUTTON,		//N64Button_B,
	Z_TRIG,			//N64Button_Z,
	L_TRIG,			//N64Button_L,
	R_TRIG,			//N64Button_R,
	U_JPAD,			//N64Button_Up,
	D_JPAD,			//N64Button_Down,
	L_JPAD,			//N64Button_Left,
	R_JPAD,			//N64Button_Right,
	U_CBUTTONS,		//N64Button_CUp,
	D_CBUTTONS,		//N64Button_CDown,
	L_CBUTTONS,		//N64Button_CLeft,
	R_CBUTTONS,		//N64Button_CRight,
	START_BUTTON	//N64Button_Start,
};

const char * const gN64ButtonNames[NUM_N64_BUTTONS] =
{
	"N64.A",			//N64Button_A = 0,
	"N64.B",			//N64Button_B,
	"N64.Z",			//N64Button_Z,
	"N64.LTrigger",		//N64Button_L,
	"N64.RTrigger",		//N64Button_R,
	"N64.Up",			//N64Button_Up,
	"N64.Down",			//N64Button_Down,
	"N64.Left",			//N64Button_Left,
	"N64.Right",		//N64Button_Right,
	"N64.CUp",			//N64Button_CUp,
	"N64.CDown",		//N64Button_CDown,
	"N64.CLeft",		//N64Button_CLeft,
	"N64.CRight",		//N64Button_CRight,
	"N64.Start"			//N64Button_Start,
};

const char * GetN64ButtonName( EN64Button button )
{
	return gN64ButtonNames[ button ];
}

//*****************************************************************************
//	A controller configuration
//*****************************************************************************
class CControllerConfig
{
	public:
		CControllerConfig();
		~CControllerConfig();

	public:
		void				SetName( const char * name )		{ mName = name; }
		void				SetDescription( const char * desc )	{ mDescription = desc; }
		void				SetJoySwap( const char * swap )		{ mJoySwap = swap[0]; }	//Save only first char which should be A|B|C for the joystick swapping

		const char *		GetName() const						{ return mName.c_str(); }
		const char *		GetDescription() const				{ return mDescription.c_str(); }
		char				GetJoySwap() const					{ return mJoySwap; }

		void				SetButtonMapping( EN64Button button, CButtonMapping * p_mapping );
		u32					GetN64ButtonsState( u32 vita_button_mask ) const;

	private:
		std::string			mName;
		std::string			mDescription;
		char				mJoySwap;

		CButtonMapping *	mButtonMappings[ NUM_N64_BUTTONS ];
};

CControllerConfig::CControllerConfig()
{
	for( u32 i = 0; i < NUM_N64_BUTTONS; ++i )
	{
		mButtonMappings[ i ] = NULL;
	}
}

CControllerConfig::~CControllerConfig()
{
	for( u32 i = 0; i < NUM_N64_BUTTONS; ++i )
	{
		delete mButtonMappings[ i ];
	}
}

void	CControllerConfig::SetButtonMapping( EN64Button button, CButtonMapping * p_mapping )
{
	delete mButtonMappings[ button ];
	mButtonMappings[ button ] = p_mapping;
}

u32		CControllerConfig::GetN64ButtonsState( u32 vita_button_mask ) const
{
	u32	state = 0;

	for( u32 i = 0; i < NUM_N64_BUTTONS; ++i )
	{
		if( mButtonMappings[ i ] != NULL )
		{
			if( mButtonMappings[ i ]->Evaluate( vita_button_mask ) )
			{
				state |= gN64ButtonMasks[ i ];
			}
		}
	}

	return state;
}

}

//*****************************************************************************
//
//*****************************************************************************

class IInputManager : public CInputManager
{
	public:
		IInputManager();
		virtual ~IInputManager();

		virtual bool						Initialise();
		virtual void						Finalise();

		virtual void						GetState( OSContPad pPad[4] );

		virtual u32							GetNumConfigurations() const;
		virtual const char *				GetConfigurationName( u32 configuration_idx ) const;
		virtual const char *				GetConfigurationDescription( u32 configuration_idx ) const;
		virtual void						SetConfiguration( u32 configuration_idx );

		virtual u32							GetConfigurationFromName( const char * name ) const;

	private:
		void								SwapJoyStick(OSContPad *pPad, SceCtrlData *pad);
		void								LoadControllerConfigs( const char * p_dir );
		CControllerConfig *					BuildDefaultConfig();
		CControllerConfig *					BuildControllerConfig( const char * filename );

	private:
		CControllerConfig *					mpControllerConfig;
		std::vector<CControllerConfig*>		mControllerConfigs;
};

//*****************************************************************************
//
//*****************************************************************************
IInputManager::IInputManager()
:	mpControllerConfig( NULL )
{
}

//*****************************************************************************
//
//*****************************************************************************
IInputManager::~IInputManager()
{
}

//*****************************************************************************
//
//*****************************************************************************
void	IInputManager::Finalise()
{
	mControllerConfigs.clear();
}

//*****************************************************************************
//
//*****************************************************************************
bool	IInputManager::Initialise()
{
	Finalise();
	CControllerConfig * p_default_config( BuildDefaultConfig() );

	mControllerConfigs.push_back( p_default_config );

	// Parse all the ini files here
	LoadControllerConfigs( DAEDALUS_VITA_PATH( "ControllerConfigs/" ) );

	mpControllerConfig = mControllerConfigs[gControllerIndex];

	return true;
}

void IInputManager::SwapJoyStick(OSContPad *pPad, SceCtrlData *pad)
{
	switch( mpControllerConfig->GetJoySwap() )
	{
		case 'A':	//No swap (default)
			break;

		case 'B':	//Swap Joystick with VITA Dpad
			{
			u32 tmp = pad->buttons;	//Save a copy

			// Make a digital version of the Analogue stick that can be mapped to N64 buttons //Corn
			pad->buttons &= ~(SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_RIGHT | SCE_CTRL_LEFT);	//Clear out original DPAD
			if (pPad->stick_x > 40) pad->buttons |= SCE_CTRL_RIGHT;
			else if (pPad->stick_x < -40) pad->buttons |= SCE_CTRL_LEFT;

			if (pPad->stick_y > 40) pad->buttons |= SCE_CTRL_UP;
			else if (pPad->stick_y < -40) pad->buttons |= SCE_CTRL_DOWN;

			//Set Stick
			if ( tmp & SCE_CTRL_RIGHT ) pPad->stick_x = s8(N64_ANALOGUE_STICK_RANGE);
			else if ( tmp & SCE_CTRL_LEFT ) pPad->stick_x = -s8(N64_ANALOGUE_STICK_RANGE);
			else pPad->stick_x = 0;

			if ( tmp & SCE_CTRL_UP ) pPad->stick_y = s8(N64_ANALOGUE_STICK_RANGE);
			else if ( tmp & SCE_CTRL_DOWN ) pPad->stick_y = -s8(N64_ANALOGUE_STICK_RANGE);
			else pPad->stick_y = 0;
			}
			break;

		case 'C':	//Swap Joystick with VITA buttons
			{
			u32 tmp = pad->buttons;	//Save a copy

			// Make a digital version of the Analogue stick that can be mapped to N64 buttons //Corn
			pad->buttons &= ~(SCE_CTRL_TRIANGLE | SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_SQUARE);	//Clear out original buttons
			if (pPad->stick_x > 40) pad->buttons |= SCE_CTRL_CIRCLE;
			else if (pPad->stick_x < -40) pad->buttons |= SCE_CTRL_SQUARE;

			if (pPad->stick_y > 40) pad->buttons |= SCE_CTRL_TRIANGLE;
			else if (pPad->stick_y < -40) pad->buttons |= SCE_CTRL_CROSS;

			//Set Stick
			if ( tmp & SCE_CTRL_CIRCLE ) pPad->stick_x = s8(N64_ANALOGUE_STICK_RANGE);
			else if ( tmp & SCE_CTRL_SQUARE ) pPad->stick_x = -s8(N64_ANALOGUE_STICK_RANGE);
			else pPad->stick_x = 0;

			if ( tmp & SCE_CTRL_TRIANGLE ) pPad->stick_y = s8(N64_ANALOGUE_STICK_RANGE);
			else if ( tmp & SCE_CTRL_CROSS ) pPad->stick_y = -s8(N64_ANALOGUE_STICK_RANGE);
			else pPad->stick_y = 0;
			}
			break;

		default:
			break;
	}
}
//*****************************************************************************
//
//*****************************************************************************
void IInputManager::GetState( OSContPad pPad[4] )
{
	// Clear the initial state of the four controllers
	for(u32 cont = 0; cont < 4; cont++)
	{
		pPad[cont].button = 0;
		pPad[cont].stick_x = 0;
		pPad[cont].stick_y = 0;
	}

	for (int i = 0; i < 4; i++) {	
		SceCtrlData pad;
		if (sceCtrlPeekBufferPositiveExt2(i ? (i+1) : 0, &pad, 1) < 0) continue; //Get VITA button inputs

		if (gUseRearpad) {
			SceTouchData touch;
			sceTouchPeek(SCE_TOUCH_PORT_BACK, &touch, 1);
			for (uint32_t j = 0; j < touch.reportNum; j++) {
				if (touch.report[j].x < 960) {
					if (touch.report[j].y < 544) pad.buttons |= SCE_CTRL_L2;
					else pad.buttons |= SCE_CTRL_L3;
				} else {
					if (touch.report[j].y < 544) pad.buttons |= SCE_CTRL_R2;
					else pad.buttons |= SCE_CTRL_R3;
				}
			}
		}
		
		//	'Normalise' from 0..255 -> -128..+127
		//
		s32 normalised_x( pad.lx - VITA_ANALOGUE_STICK_RANGE );
		s32 normalised_y( pad.ly - VITA_ANALOGUE_STICK_RANGE );

		//
		//	Now scale from -128..+127 -> -80..+80 (y is inverted from VITA coords)
		//
		v2 stick( f32( normalised_x ) / VITA_ANALOGUE_STICK_RANGE, f32( normalised_y ) / VITA_ANALOGUE_STICK_RANGE );

		stick = ApplyDeadzone( stick, gGlobalPreferences.StickMinDeadzone, gGlobalPreferences.StickMaxDeadzone );

		//Smoother joystick sensitivity //Corn
		
		stick.x = (0.5f * stick.x) * (1 + stick.x * stick.x);
		stick.y = (0.5f * stick.y) * (1 + stick.y * stick.y);

		pPad[i].stick_x =  s8(stick.x * N64_ANALOGUE_STICK_RANGE);
		pPad[i].stick_y = -s8(stick.y * N64_ANALOGUE_STICK_RANGE);

		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( mpControllerConfig != NULL, "We should always have a valid controller" );
		#endif

		SwapJoyStick(&pPad[i], &pad);
		
		// PSVita Right Stick
		if (pad.rx > 170) pad.buttons |= SCE_CTRL_RRIGHT;
		if (pad.rx < 85) pad.buttons |= SCE_CTRL_RLEFT;

		if (pad.ry > 170) pad.buttons |= SCE_CTRL_RDOWN;
		if (pad.ry < 85) pad.buttons |= SCE_CTRL_RUP;

		pPad[i].button = mpControllerConfig->GetN64ButtonsState( pad.buttons );
	}
	
#ifdef DAEDALUS_ENABLE_SYNCHRONISATION
	// Synchronise the input - this will overwrite the real pad data when playing back input
	for(u32 cont = 0; cont < 4; cont++)
	{
		SYNCH_DATA( pPad[cont] );
	}
#endif
}

//*****************************************************************************
//
//*****************************************************************************
template<> bool	CSingleton< CInputManager >::Create()
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT_Q(mpInstance == NULL);
	#endif

	IInputManager * manager( new IInputManager() );

	if(manager->Initialise())
	{
		mpInstance = manager;
		return true;
	}

	delete manager;
	return false;
}

//*****************************************************************************
//	Some utility classes for handling our expression evaluation
//*****************************************************************************
namespace
{

struct SButtonNameMapping
{
	const char * ButtonName;
	u32			 ButtonMask;
};

const SButtonNameMapping	gButtonNameMappings[] =
{
	{ "VITA.Start",		SCE_CTRL_START },
	{ "VITA.Cross",		SCE_CTRL_CROSS },
	{ "VITA.Square",	SCE_CTRL_SQUARE },
	{ "VITA.Triangle",	SCE_CTRL_TRIANGLE },
	{ "VITA.Circle",	SCE_CTRL_CIRCLE },
	{ "VITA.LTrigger",	SCE_CTRL_L1 },
	{ "VITA.RTrigger",	SCE_CTRL_R1 },
	{ "VITA.Up",		SCE_CTRL_UP },
	{ "VITA.Down",		SCE_CTRL_DOWN },
	{ "VITA.Left",		SCE_CTRL_LEFT },
	{ "VITA.Right",		SCE_CTRL_RIGHT },
	{ "VITA.Select",	SCE_CTRL_SELECT },
	{ "VITA.RUp",	    SCE_CTRL_RUP },
	{ "VITA.RDown",	    SCE_CTRL_RDOWN },
	{ "VITA.RLeft",	    SCE_CTRL_RLEFT },
	{ "VITA.RRight",	SCE_CTRL_RRIGHT },
	{ "VITA.L2",		SCE_CTRL_L2 },
	{ "VITA.R2",		SCE_CTRL_R2 },
	{ "VITA.L3",		SCE_CTRL_L3 },
	{ "VITA.R3",		SCE_CTRL_R3 }
};

u32 GetOperatorPrecedence( char op )
{
	switch ( op )
	{
	case '!':		return 2;
	case '&':		return 1;
	case '|':		return 0;
	default:
	#ifdef DAEDALUS_DEBUG_CONSOLE
		DAEDALUS_ERROR( "Unhandled operator" );
		#endif
		return 0;
	}
}

bool IsOperatorChar( char c )
{
	return c == '&' || c == '|' || c == '!';
}

bool IsIdentifierChar( char c )
{
	return isalpha( c ) || isdigit( c ) || c == '.';
}

u32 GetMaskFromIdentifier( const char * identifier )
{
	for( u32 i = 0; i < ARRAYSIZE( gButtonNameMappings ); ++i )
	{
		if( strcmp( gButtonNameMappings[ i ].ButtonName, identifier ) == 0 )
		{
			return gButtonNameMappings[ i ].ButtonMask;
		}
	}

	// Error - unknown button name!
	// This is handled by the parser below
	return 0;
}

class CButtonMappingExpressionEvaluator
{
	public:
		CButtonMappingExpressionEvaluator();

		CButtonMapping *			Parse( const char * expression );
		u32							GetNumErrors() const				{ return mNumErrors; }
		const char *				GetErrorString() const				{ return mErrorStream.c_str(); }

	private:
		static	void				HandleOperator( char op, std::stack<CButtonMapping*> & operands );
				COutputStream &		ReportError();

	private:
		u32							mNumErrors;
		COutputStringStream			mErrorStream;

};

CButtonMappingExpressionEvaluator::CButtonMappingExpressionEvaluator()
:	mNumErrors( 0 )
{
}

COutputStream &	CButtonMappingExpressionEvaluator::ReportError()
{
	mNumErrors++;
	return mErrorStream;
}

void	CButtonMappingExpressionEvaluator::HandleOperator( char op, std::stack<CButtonMapping*> & operands )
{
	switch( op )
	{
	case '!':
		if( !operands.empty() )
		{
			CButtonMapping *	p_arg( operands.top() );
			operands.pop();
			operands.push(  new CButtonMappingNegate( p_arg ));
		}
		break;
	case '&':
		if( !operands.empty() )
		{
			CButtonMapping * p_arg_b( operands.top() );
			operands.pop();
			if( !operands.empty() )
			{
				CButtonMapping * p_arg_a( operands.top() );
				operands.pop();

				operands.push( new CButtonMappingAnd( p_arg_a, p_arg_b ) );
			}
		}
		break;
	case '|':
		if( !operands.empty() )
		{
			CButtonMapping * p_arg_b( operands.top() );
			operands.pop();
			if( !operands.empty() )
			{
				CButtonMapping * p_arg_a( operands.top() );
				operands.pop();

				operands.push( new CButtonMappingOr( p_arg_a, p_arg_b ) );
			}
		}
		break;
		#ifdef DAEDALUS_ENABLE_ASSERTS
	default:
		DAEDALUS_ERROR( "Unhandled operator" );
		break;
		#endif
	}
}

CButtonMapping *	CButtonMappingExpressionEvaluator::Parse( const char * expression )
{
	mNumErrors = 0;
	mErrorStream.Clear();

	std::stack<char>					operator_stack;
	std::stack<CButtonMapping *>		operand_stack;

	const char * p_cur( expression );
	const char * p_end( expression + strlen( expression ) );
	for( ; p_cur < p_end; ++p_cur )
	{
		char c( *p_cur );

		if( isspace( c ) )
		{
			// Ignore
		}
		else if( c == '(' )
		{
			// Start of a set of parenthesis - push onto the token stack
			operator_stack.push( c );
		}
		else if( c == ')' )
		{
			// We've reached a closing parenthesis - pop operands from the stack
			bool found_opening = false;
			while( !found_opening )
			{
				if( operator_stack.empty() )
				{
					break;
				}

				char	op( operator_stack.top() );
				operator_stack.pop();

				// This must be an operand. Pop the arguments from the mapping stack and construct outputs
				switch( op )
				{
				case '(':
					// We've matched the opening parenthesis - can discard it and continue parsing
					found_opening = true;
					break;
				case '!':
				case '&':
				case '|':
					HandleOperator( op, operand_stack );
					break;
				default:
					ReportError() << "Unknown operator '" << op << "'\n";
					break;
				}
			}

			if( !found_opening )
			{
				ReportError() << "Couldn't match closing parenthesis at char " << u32(p_cur - expression) << ", '..." << p_cur << "'\n";
			}
		}
		else if( IsOperatorChar( c ) )
		{
			// Currently we only support single-character operators

			if( !operator_stack.empty() )
			{
				char	prevOp( operator_stack.top() );
				u32		newPrecedence( GetOperatorPrecedence( c ) );
				u32		prevPrecedence( GetOperatorPrecedence( prevOp ) );

				// If the new precedence is less than that of the previous operator then
				// consume the previous operator before continuing
				if( newPrecedence <= prevPrecedence )
				{
					operator_stack.pop();
					HandleOperator( prevOp, operand_stack );
				}
			}

			operator_stack.push( c );
		}
		else if( IsIdentifierChar( c ) )
		{
			const char * p_identifier_start( p_cur );
			const char * p;

			// Grab all the identifier chars we can
			for( p = p_cur; IsIdentifierChar( *p ); ++p )
			{
			}

			const char * p_identifier_end( p );
			p_cur = p - 1;

			std::string		identifier( p_identifier_start, p_identifier_end );

			u32			mask( GetMaskFromIdentifier( identifier.c_str() ) );

			if( mask == 0 )
			{
				ReportError() << "Unknown control '" << identifier.c_str() << "'\n";
			}

			operand_stack.push( new CButtonMappingMask( mask ) );
		}
	}

	//
	//	At this point we have evaluated the expression string, but might still have
	//	unhandled operators lying on the expression stack. Spin through it
	//	consuming operators (and presumably operands)
	//
	while( !operator_stack.empty() )
	{
		char	op( operator_stack.top() );
		operator_stack.pop();

		HandleOperator( op, operand_stack );
	}

	CButtonMapping * p_mapping( NULL );
	if( operand_stack.empty() )
	{
		ReportError() << "Term did not evaluate to an expression\n";
	}
	else
	{
		p_mapping = operand_stack.top();
		operand_stack.pop();

		if( !operand_stack.empty() )
		{
			ReportError() << "More than one expression remaining\n";
		}
	}

	//if( mNumErrors > 0 )
	//{
	//	printf( "%s", mErrorStream.c_str() );
	//}

	return p_mapping;
}

}

//*****************************************************************************
//
//*****************************************************************************
void	IInputManager::LoadControllerConfigs( const char * p_dir )
{
	IO::FindHandleT		find_handle;
	IO::FindDataT		find_data;
	if(IO::FindFileOpen( p_dir, &find_handle, find_data ))
	{
		do
		{
			const char * filename( find_data.Name );
			const char * last_period( strrchr( filename, '.' ) );
			if(last_period != NULL)
			{
				if( _strcmpi(last_period, ".ini") == 0 )
				{
					std::string		full_path;

					full_path = p_dir;
					full_path += filename;

					CControllerConfig *	p_config( BuildControllerConfig( full_path.c_str() ) );

					if( p_config != NULL )
					{
						mControllerConfigs.push_back( p_config );
					}
				}
			}
		}
		while(IO::FindFileNext( find_handle, find_data ));

		IO::FindFileClose( find_handle );
	}
}

//*****************************************************************************
//	Build a default controller configuration
//	We do this in case the user deletes all the config files (or they're all
//	corrupt) so that we can ensure there's at least one valid config.
//*****************************************************************************
CControllerConfig *	IInputManager::BuildDefaultConfig()
{
	CControllerConfig *	p_config( new CControllerConfig );

	p_config->SetName( "Default" );
	p_config->SetDescription( "The default Daedalus controller configuration." );
	p_config->SetJoySwap( "A" );

	CButtonMappingExpressionEvaluator	eval;

	p_config->SetButtonMapping( N64Button_Start, eval.Parse( "VITA.Start" ) );

	p_config->SetButtonMapping( N64Button_A, eval.Parse( "VITA.Cross" ) );
	p_config->SetButtonMapping( N64Button_B, eval.Parse( "VITA.Square" ) );
	p_config->SetButtonMapping( N64Button_Z, eval.Parse( "VITA.LTrigger" ) );
	p_config->SetButtonMapping( N64Button_L, eval.Parse( "VITA.Triangle" ) );
	p_config->SetButtonMapping( N64Button_R, eval.Parse( "VITA.RTrigger" ) );

	p_config->SetButtonMapping( N64Button_Up, eval.Parse( "VITA.Up" ) );
	p_config->SetButtonMapping( N64Button_Down, eval.Parse( "VITA.Down" ) );
	p_config->SetButtonMapping( N64Button_Left, eval.Parse( "VITA.Left" ) );
	p_config->SetButtonMapping( N64Button_Right, eval.Parse( "VITA.Right" ) );
	
	p_config->SetButtonMapping( N64Button_CUp, eval.Parse( "VITA.RUp" ) );
	p_config->SetButtonMapping( N64Button_CDown, eval.Parse( "VITA.RDown" ) );
	p_config->SetButtonMapping( N64Button_CLeft, eval.Parse( "VITA.RLeft" ) );
	p_config->SetButtonMapping( N64Button_CRight, eval.Parse( "VITA.RRight" ) );

	return p_config;
}

//*****************************************************************************
//
//*****************************************************************************
CControllerConfig *	IInputManager::BuildControllerConfig( const char * filename )
{
	CIniFile * p_ini_file( CIniFile::Create( filename ) );
	if( p_ini_file == NULL )
	{
		return NULL;
	}

	const CIniFileProperty * p_property;
	CControllerConfig *		p_config( new CControllerConfig );

	//
	//	Firstly parse the default section
	//
	const CIniFileSection * p_default_section( p_ini_file->GetDefaultSection() );

	if( p_default_section->FindProperty( "Name", &p_property ) )
	{
		p_config->SetName( p_property->GetValue() );
	}
	if( p_default_section->FindProperty( "Description", &p_property ) )
	{
		char tmp[256];
		sprintf(tmp, p_property->GetValue());
		char *newline = nullptr, *p = tmp;
		while (newline = strstr(p, "\\n")) {
			newline[0] = '\n';
			sprintf(&newline[1], &newline[2]);
			p++;
		}
		p_config->SetDescription( tmp );
	}
	if( p_default_section->FindProperty( "JoystickSwap", &p_property ) )
	{
		p_config->SetJoySwap( p_property->GetValue() );
	}
	else
	{
		p_config->SetJoySwap( "A" );
	}

	//
	//	Now parse all the buttons
	//
	const CIniFileSection *		p_button_section( p_ini_file->GetSectionByName( "Buttons" ) );

	if( p_button_section != NULL )
	{
		CButtonMappingExpressionEvaluator	eval;

		for( u32 i = 0; i < NUM_N64_BUTTONS; ++i )
		{
			EN64Button		button = EN64Button( i );
			const char *	button_name( GetN64ButtonName( button ) );

			if( p_button_section->FindProperty( button_name, &p_property ) )
			{
				p_config->SetButtonMapping( button, eval.Parse( p_property->GetValue() ) );
			}
			else
			{
				//printf( "There was no property for %s\n", button_name );
			}
		}
	}
	else
	{
		//printf( "Couldn't find buttons section\n" );
	}

	delete p_ini_file;

	return p_config;
}

//*****************************************************************************
//
//*****************************************************************************
u32	 IInputManager::GetNumConfigurations() const
{
	return mControllerConfigs.size();
}

//*****************************************************************************
//
//*****************************************************************************
const char *	IInputManager::GetConfigurationName( u32 configuration_idx ) const
{
	if( configuration_idx < mControllerConfigs.size() )
	{
		return mControllerConfigs[ configuration_idx ]->GetName();
	}
	else
	{
		#ifdef DAEDALUS_DEBUG_CONSOLE
		DAEDALUS_ERROR( "Invalid controller config" );
		#endif
		return "?";
	}
}

//*****************************************************************************
//
//*****************************************************************************
const char *	IInputManager::GetConfigurationDescription( u32 configuration_idx ) const
{
	if( configuration_idx < mControllerConfigs.size() )
	{
		return mControllerConfigs[ configuration_idx ]->GetDescription();
	}
	else
	{
			#ifdef DAEDALUS_DEBUG_CONSOLE
		DAEDALUS_ERROR( "Invalid controller config" );
		#endif
		return "?";
	}
}

//*****************************************************************************
//
//*****************************************************************************
void			IInputManager::SetConfiguration( u32 configuration_idx )
{
	if( configuration_idx < mControllerConfigs.size() )
	{
		mpControllerConfig = mControllerConfigs[ configuration_idx ];
		gControllerIndex = configuration_idx;
		#ifdef DAEDALUS_DEBUG_CONSOLE
		DBGConsole_Msg( 0, "Setting the controller to [c%s]", mpControllerConfig->GetName() );
		#endif
	}
		#ifdef DAEDALUS_DEBUG_CONSOLE
	else
	{
		DAEDALUS_ERROR( "Invalid controller config" );
	}
	#endif
}

//*****************************************************************************
//
//*****************************************************************************
u32		IInputManager::GetConfigurationFromName( const char * name ) const
{
	for( u32 i = 0; i < mControllerConfigs.size(); ++i )
	{
		if( _strcmpi( mControllerConfigs[ i ]->GetName(), name ) == 0 )
		{
			return i;
		}
	}

	// Return the default controller config
	return 0;
}

//*************************************************************************************
//
//*************************************************************************************
v2	ProjectToUnitSquare( const v2 & in )
{
	f32		length( in.Length() );
	float	abs_x( fabsf( in.x ) );
	float	abs_y( fabsf( in.y ) );
	float	scale;

	//
	//	Select the longest axis, and
	//
	if( length < 0.01f )
	{
		scale = 1.0f;
	}
	else if( abs_x > abs_y )
	{
		scale = length / abs_x;
	}
	else
	{
		scale = length / abs_y;
	}

	return in * scale;
}

//*************************************************************************************
//
//*************************************************************************************
v2	ApplyDeadzone( const v2 & in, f32 min_deadzone, f32 max_deadzone )
{
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( min_deadzone >= 0.0f && min_deadzone <= 1.0f, "Invalid min deadzone" );
	DAEDALUS_ASSERT( max_deadzone >= 0.0f && max_deadzone <= 1.0f, "Invalid max deadzone" );
#endif
	float	length( in.Length() );

	if( length < min_deadzone )
		return v2( 0,0 );

	float	scale( ( length - min_deadzone ) / ( max_deadzone - min_deadzone )  );

	scale = Clamp( scale, 0.0f, 1.0f );

	return ProjectToUnitSquare( in * (scale / length) );
}
