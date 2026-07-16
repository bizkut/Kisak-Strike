//===== Copyright � 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: 	ExprSimplifier builds a binary tree from an infix expression (in the
//				form of a character array). Evaluates C style infix parenthetic logical
//				expressions. Supports !, ||, &&, (). Symbols are resolved via callback.
//				Syntax is $<name>. $0 evaluates to false. $<number> evaluates to true.
//				e.g: ( $1 || ( $FOO || $WHATEVER ) && !$BAR )
//===========================================================================//

#include <ctype.h>
#include <vstdlib/ikeyvaluessystem.h>
#include "tier1/exprevaluator.h"
#include "tier1/convar.h"
#include "tier1/fmtstr.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( PLATFORM_PS4 )
extern "C" void KisakPs4StartupBreadcrumb( const char *line );

static bool s_bKisakPs4TraceCompoundConditional = false;
static int s_nKisakPs4CompoundConditionalTraceCount = 0;
static const int k_nKisakPs4CompoundConditionalTraceLimit = 64;

static void KisakPs4CompoundConditionalBreadcrumb( const char *pPhase,
	const char *pText, int nPosition, int nCurrentToken, int nKind )
{
	if ( !s_bKisakPs4TraceCompoundConditional ||
		s_nKisakPs4CompoundConditionalTraceCount >=
		k_nKisakPs4CompoundConditionalTraceLimit )
	{
		return;
	}

	char line[512];
	V_snprintf( line, sizeof( line ),
		"kisak-ps4: conditional eval event=%d phase=%s pos=%d current=%d kind=%d text=%.160s",
		s_nKisakPs4CompoundConditionalTraceCount,
		pPhase ? pPhase : "unknown", nPosition, nCurrentToken, nKind,
		pText ? pText : "<null>" );
	++s_nKisakPs4CompoundConditionalTraceCount;
	KisakPs4StartupBreadcrumb( line );
}

#define PS4_COMPOUND_CONDITIONAL_BREADCRUMB( phase, text, position, current, kind ) \
	do { KisakPs4CompoundConditionalBreadcrumb( phase, text, position, current, kind ); } while ( 0 )
#else
#define PS4_COMPOUND_CONDITIONAL_BREADCRUMB( phase, text, position, current, kind ) ((void)0)
#endif

//-----------------------------------------------------------------------------
// Default conditional symbol handler callback. Symbols are the form $<name>.
// Return true or false for the value of the symbol.
//-----------------------------------------------------------------------------
bool DefaultConditionalSymbolProc( const char *pKey )
{
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "symbol-proc-enter", pKey, -1, -1, -1 );
	if ( pKey[0] == '$' )
	{
		pKey++;
	}

	if ( !V_stricmp( pKey, "WIN32" ) )
	{
		return IsPC();
	}

	if ( !V_stricmp( pKey, "WINDOWS" ) )
	{
		return IsPlatformWindowsPC();
	}
	
	if ( !V_stricmp( pKey, "X360" ) )
	{
		return IsX360();
	}

	if ( !V_stricmp( pKey, "PS3" ) )
	{
		return IsPS3();
	}

	if ( !V_stricmp( pKey, "OSX" ) )
	{
		return IsPlatformOSX();
	}

	if ( !V_stricmp( pKey, "LINUX" ) )
	{
		return IsPlatformLinux();
	}

	if ( !V_stricmp( pKey, "POSIX" ) )
	{
		return IsPlatformPosix();
	}	
	
	if ( !V_stricmp( pKey, "GAMECONSOLE" ) )
	{
		return IsGameConsole();
	}

	if ( !V_stricmp( pKey, "DEMO" ) )
	{
#if defined( _DEMO )
		return true;
#else
		return false;
#endif
	}

	if ( !V_stricmp( pKey, "LOWVIOLENCE" ) )
	{
#if defined( _LOWVIOLENCE )
		return true;
#endif
		// If it is not a LOWVIOLENCE binary build, then fall through
		// and check if there was a run-time symbol installed for it
	}

	// don't know it at compile time, so fall through to installed symbol values
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-runtime-symbol", pKey, -1, -1, -1 );
	const bool bValue = KeyValuesSystem()->GetKeyValuesExpressionSymbol( pKey );
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "runtime-symbol-return", pKey, -1,
		bValue ? 1 : 0, -1 );
	return bValue;
}

void DefaultConditionalErrorProc( const char *pReason )
{
	Warning( "Conditional Error: %s\n", pReason );
}

CExpressionEvaluator::CExpressionEvaluator()
{
	m_ExprTree = NULL;
}

CExpressionEvaluator::~CExpressionEvaluator()
{
	FreeTree( m_ExprTree );
}

//-----------------------------------------------------------------------------
//	Sets mCurToken to the next token in the input string. Skips all whitespace.
//-----------------------------------------------------------------------------
char CExpressionEvaluator::GetNextToken( void )
{
	// while whitespace, Increment CurrentPosition
	while ( m_pExpression[m_CurPosition] == ' ' )
		++m_CurPosition;
    
	// CurrentToken = Expression[CurrentPosition]
	m_CurToken = m_pExpression[m_CurPosition++];
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "token", m_pExpression,
		m_CurPosition, static_cast<unsigned char>( m_CurToken ), -1 );
  
	return m_CurToken;
}


//-----------------------------------------------------------------------------
//	Utility funcs
//-----------------------------------------------------------------------------
void CExpressionEvaluator::FreeNode( ExprNode *pNode )
{
	delete pNode;
}

ExprNode *CExpressionEvaluator::AllocateNode( void )
{
	return new ExprNode;
}

void CExpressionEvaluator::FreeTree( ExprTree& node )
{
	if ( !node )
		return;

	FreeTree( node->left );
	FreeTree( node->right );
	FreeNode( node );
	node = 0;
}

bool CExpressionEvaluator::IsConditional( bool &bConditional, const char token )
{
	char nextchar = ' ';
	if ( token == OR_OP || token == AND_OP )
	{
		// expect || or &&
		nextchar = m_pExpression[m_CurPosition++];
		if ( (token & nextchar) == token )
		{
			bConditional = true;
		}
		else if ( m_pSyntaxErrorProc )
		{
			m_pSyntaxErrorProc( CFmtStr( "Bad expression operator: '%c%c', expected C style operator", token, nextchar ) );
			return false;
		}
	}
	else
	{
		bConditional = false;
	}

	// valid
	return true;
}

bool CExpressionEvaluator::IsNotOp( const char token )
{
	if ( token == NOT_OP )
		return true;
	else
		return false;
}

bool CExpressionEvaluator::IsIdentifierOrConstant( const char token )
{
	bool success = false;
	if ( token == '$' )
	{
		// store the entire identifier
		int i = 0;
		m_Identifier[i++] = token;
		while( (V_isalnum( m_pExpression[m_CurPosition] ) || m_pExpression[m_CurPosition] == '_') && i < MAX_IDENTIFIER_LEN )
		{
			m_Identifier[i] = m_pExpression[m_CurPosition];
			++m_CurPosition;
			++i;
		}

		if ( i < MAX_IDENTIFIER_LEN - 1 )
		{
			m_Identifier[i] = '\0';
			success = true;
		}
	}
	else
	{
		if ( V_isdigit( token ) )
		{
			int i = 0;
			m_Identifier[i++] = token;
			while( V_isdigit( m_pExpression[m_CurPosition] ) && ( i < MAX_IDENTIFIER_LEN ) )
			{
				m_Identifier[i] = m_pExpression[m_CurPosition];
				++m_CurPosition;
				++i;
			}
			if ( i < MAX_IDENTIFIER_LEN - 1 )
			{
				m_Identifier[i] = '\0';
				success = true;
			}
		}
	}

	return success;
}

bool CExpressionEvaluator::MakeExprNode( ExprTree &tree, char token, Kind kind, ExprTree left, ExprTree right )
{
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-node-alloc", m_Identifier,
		m_CurPosition, static_cast<unsigned char>( token ), static_cast<int>( kind ) );
	tree = AllocateNode();
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "node-allocated", m_Identifier,
		m_CurPosition, static_cast<unsigned char>( token ), static_cast<int>( kind ) );
	tree->left = left;
	tree->right = right;
	tree->kind = kind;

	switch ( kind )
	{
	case CONDITIONAL:
		tree->data.cond = token;
		break;

	case LITERAL:
		if ( V_isdigit( m_Identifier[0] ) )
		{
			tree->data.value = ( atoi( m_Identifier ) != 0 );
		}
		else
		{
			PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-symbol-callback",
				m_Identifier, m_CurPosition, static_cast<unsigned char>( token ),
				static_cast<int>( kind ) );
			tree->data.value = m_pGetSymbolProc( m_Identifier );
			PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "symbol-callback-return",
				m_Identifier, m_CurPosition, tree->data.value ? 1 : 0,
				static_cast<int>( kind ) );
		}
		break;

	case NOT:
		break;

	default:
		if ( m_pSyntaxErrorProc )
		{
			Assert( 0 );
			m_pSyntaxErrorProc( CFmtStr( "Logic Error in CExpressionEvaluator" ) );
		}
		return false;
	}

	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "node-ready", m_Identifier,
		m_CurPosition, static_cast<unsigned char>( token ), static_cast<int>( kind ) );
	return true;
}

//-----------------------------------------------------------------------------
//	Makes a factor :: { <expression> } | <identifier>.
//-----------------------------------------------------------------------------
bool CExpressionEvaluator::MakeFactor( ExprTree &tree )
{
	if ( m_CurToken == '(' )
	{
		// Get the next token
		GetNextToken();

		// Make an expression, setting Tree to point to it
		if ( !MakeExpression( tree ) )
		{
			return false;
		}
	}
	else if ( IsIdentifierOrConstant( m_CurToken ) )
	{
		// Make a literal node, set Tree to point to it, set left/right children to NULL. 
		if ( !MakeExprNode( tree, m_CurToken, LITERAL, NULL, NULL ) )
		{
			return false;
		}
	}
	else if ( IsNotOp( m_CurToken ) )
	{
		// do nothing
		return true;
	}
	else
	{
		// This must be a bad token
		if ( m_pSyntaxErrorProc )
		{
			m_pSyntaxErrorProc( CFmtStr( "Bad expression token: %c", m_CurToken ) );
		}
		return false;
	}

	// Get the next token
	GetNextToken();
	return true;
}

//-----------------------------------------------------------------------------
//	Makes a term :: <factor> { <not> }.
//-----------------------------------------------------------------------------
bool CExpressionEvaluator::MakeTerm( ExprTree &tree )
{
	// Make a factor, setting Tree to point to it
	if ( !MakeFactor( tree ) )
	{
		return false;
	}

	// while the next token is !
	while ( IsNotOp( m_CurToken ) )
	{
		// Make an operator node, setting left child to Tree and right to NULL. (Tree points to new node)
		if ( !MakeExprNode( tree, m_CurToken, NOT, tree, NULL ) )
		{
			return false;
		}

		// Get the next token.
		GetNextToken();

		// Make a factor, setting the right child of Tree to point to it.
		if ( !MakeFactor( tree->right ) )
		{
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//	Makes a complete expression :: <term> { <cond> <term> }.
//-----------------------------------------------------------------------------
bool CExpressionEvaluator::MakeExpression( ExprTree &tree )
{
	// Make a term, setting Tree to point to it
	if ( !MakeTerm( tree ) )
	{
		return false;
	}

	// while the next token is a conditional
	while ( 1 )
	{
		bool bConditional = false;
		bool bValid = IsConditional( bConditional, m_CurToken );
		if ( !bValid )
		{
			return false;
		}

		if ( !bConditional )
		{
			break;
		}

		// Make a conditional node, setting left child to Tree and right to NULL. (Tree points to new node)
		if ( !MakeExprNode( tree, m_CurToken, CONDITIONAL, tree, NULL ) )
		{
			return false;
		}

		// Get the next token.
		GetNextToken();

		// Make a term, setting the right child of Tree to point to it.
		if ( !MakeTerm( tree->right ) )
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
//	returns true for success, false for failure
//-----------------------------------------------------------------------------
bool CExpressionEvaluator::BuildExpression( void )
{
	// Get the first token, and build the tree.
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "build-enter", m_pExpression,
		m_CurPosition, m_CurToken, -1 );
	GetNextToken();

	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-make-expression", m_pExpression,
		m_CurPosition, m_CurToken, -1 );
	const bool bValid = MakeExpression( m_ExprTree );
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "make-expression-return", m_pExpression,
		m_CurPosition, bValid ? 1 : 0, -1 );
	return bValid;
}

//-----------------------------------------------------------------------------
//	returns the value of the node after resolving all children
//-----------------------------------------------------------------------------
bool CExpressionEvaluator::SimplifyNode( ExprTree& node )
{
	if ( !node )
		return false;

	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "simplify-enter", m_pExpression,
		m_CurPosition, m_CurToken, static_cast<int>( node->kind ) );

	// Simplify the left and right children of this node
	bool leftVal = SimplifyNode(node->left);
	bool rightVal = SimplifyNode(node->right);

	// Simplify this node
	switch( node->kind )
	{
	case NOT:
		// the child of '!' is always to the right
		node->data.value = !rightVal;
		break;
	
	case CONDITIONAL:
		if ( node->data.cond == AND_OP )
		{
			node->data.value = leftVal && rightVal;
		}
		else // OR_OP
		{	
			node->data.value = leftVal || rightVal;
		}
		break;

	default: // LITERAL
		break;
	}

	// This node has beed resolved
	node->kind = LITERAL;
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "simplify-return", m_pExpression,
		m_CurPosition, node->data.value ? 1 : 0, static_cast<int>( node->kind ) );
	return node->data.value;
}

//-----------------------------------------------------------------------------
//	Interface to solve a conditional expression. Returns false on failure, Result is undefined.
//-----------------------------------------------------------------------------
bool CExpressionEvaluator::Evaluate( bool &bResult, const char *pInfixExpression, GetSymbolProc_t pGetSymbolProc, SyntaxErrorProc_t pSyntaxErrorProc )
{
#if defined( PLATFORM_PS4 )
	const bool bTraceCompoundConditional = pInfixExpression &&
		V_strcmp( pInfixExpression, "[$PS3 && !$INPUTSWAPAB]" ) == 0;
	if ( bTraceCompoundConditional )
	{
		s_nKisakPs4CompoundConditionalTraceCount = 0;
		s_bKisakPs4TraceCompoundConditional = true;
	}
#endif
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "evaluate-enter", pInfixExpression,
		-1, -1, -1 );
	if ( !pInfixExpression )
	{
		return false;
	}

	// for caller simplicity, we strip of any enclosing braces
	// strip the bracketing [] if present
	char szCleanToken[512];
	if ( pInfixExpression[0] == '[' )
	{
		PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-input-length",
			pInfixExpression, -1, -1, -1 );
		int len = V_strlen( pInfixExpression );
		PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "input-length-ready",
			pInfixExpression, len, -1, -1 );

		// SECURITY: Bail on input buffers that are too large, they're used for RCEs and we don't 
		// need to support them.
		if ( len + 1 > ARRAYSIZE( szCleanToken ) )
		{
			return false;
		}

		PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-input-copy",
			pInfixExpression, len, -1, -1 );
		V_strncpy( szCleanToken, pInfixExpression + 1, len );
		PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "input-copy-ready",
			szCleanToken, len, -1, -1 );
		len--;
		if ( szCleanToken[len-1] == ']' )
		{
			szCleanToken[len-1] = '\0';
		}
		pInfixExpression = szCleanToken;
		PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "input-clean-ready",
			pInfixExpression, len, -1, -1 );
	}

	// reset state
	m_pExpression = pInfixExpression;
	m_pGetSymbolProc = pGetSymbolProc ? pGetSymbolProc : DefaultConditionalSymbolProc;
	m_pSyntaxErrorProc = pSyntaxErrorProc ? pSyntaxErrorProc : DefaultConditionalErrorProc;
	m_ExprTree = 0;
	m_CurPosition = 0;
	m_CurToken = 0;
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "state-ready", m_pExpression,
		m_CurPosition, m_CurToken, -1 );

	// Building the expression tree will fail on bad syntax
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-build", m_pExpression,
		m_CurPosition, m_CurToken, -1 );
	bool bValid = BuildExpression();
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "build-return", m_pExpression,
		m_CurPosition, bValid ? 1 : 0, -1 );
	if ( bValid )
	{
		PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-simplify", m_pExpression,
			m_CurPosition, m_CurToken, -1 );
		bResult = SimplifyNode( m_ExprTree );
		PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "simplify-complete", m_pExpression,
			m_CurPosition, bResult ? 1 : 0, -1 );
	}

	// don't leak
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "before-free", m_pExpression,
		m_CurPosition, m_CurToken, -1 );
	FreeTree( m_ExprTree );
	m_ExprTree = NULL;
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "free-complete", m_pExpression,
		m_CurPosition, m_CurToken, -1 );
	PS4_COMPOUND_CONDITIONAL_BREADCRUMB( "evaluate-return", m_pExpression,
		m_CurPosition, bValid ? 1 : 0, -1 );
#if defined( PLATFORM_PS4 )
	if ( bTraceCompoundConditional )
	{
		s_bKisakPs4TraceCompoundConditional = false;
	}
#endif
	return bValid;
}








