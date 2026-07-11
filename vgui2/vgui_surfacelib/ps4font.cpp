#include "ps4font.h"
#include "tier1/strtools.h"

#include <string.h>

CPs4Font::CPs4Font()
	: m_iTall( 0 ),
	  m_iWeight( 0 ),
	  m_iFlags( 0 ),
	  m_bAntiAliased( false ),
	  m_bRotary( false ),
	  m_bAdditive( false ),
	  m_iDropShadowOffset( 0 ),
	  m_bUnderlined( false ),
	  m_iOutlineSize( 0 ),
	  m_iHeight( 0 ),
	  m_iMaxCharWidth( 0 ),
	  m_iAscent( 0 ),
	  m_iScanLines( 0 ),
	  m_iBlur( 0 )
{
}

CPs4Font::~CPs4Font()
{
}

bool CPs4Font::Create( const char *pFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	m_szName = pFontName ? pFontName : "";
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_iHeight = tall;
	m_iAscent = ( tall * 3 ) / 4;
	m_iMaxCharWidth = tall > 1 ? tall / 2 : tall;
	return false;
}

void CPs4Font::GetCharRGBA( wchar_t, int rgbaWide, int rgbaTall, unsigned char *pRgba )
{
	if ( pRgba && rgbaWide > 0 && rgbaTall > 0 )
	{
		memset( pRgba, 0, rgbaWide * rgbaTall * 4 );
	}
}

bool CPs4Font::IsEqualTo( const char *pFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	return !Q_stricmp( m_szName.String(), pFontName ? pFontName : "" ) &&
		m_iTall == tall && m_iWeight == weight && m_iBlur == blur &&
		m_iScanLines == scanlines && m_iFlags == flags;
}

bool CPs4Font::IsValid()
{
	return false;
}

void CPs4Font::GetCharABCWidths( int, int &a, int &b, int &c )
{
	a = 0;
	b = m_iMaxCharWidth;
	c = 0;
}

int CPs4Font::GetHeight() { return m_iHeight; }
int CPs4Font::GetAscent() { return m_iAscent; }
int CPs4Font::GetMaxCharWidth() { return m_iMaxCharWidth; }
int CPs4Font::GetFlags() { return m_iFlags; }

void CPs4Font::GetKernedCharWidth( wchar_t ch, wchar_t, wchar_t, float &wide, float &abcA, float &abcC )
{
	int a, b, c;
	GetCharABCWidths( ch, a, b, c );
	abcA = a;
	abcC = c;
	wide = a + b + c;
}

bool CPs4Font::CreateFromMemory( const char *pFontName, void *, int, int tall, int weight, int blur, int scanlines, int flags )
{
	Create( pFontName, tall, weight, blur, scanlines, flags );
	return false;
}
