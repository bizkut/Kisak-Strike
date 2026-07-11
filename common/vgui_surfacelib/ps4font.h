#ifndef PS4FONT_H
#define PS4FONT_H

#include "tier1/utlstring.h"

class CPs4Font
{
public:
	CPs4Font();
	virtual ~CPs4Font();

	virtual bool Create( const char *pFontName, int tall, int weight, int blur, int scanlines, int flags );
	virtual void GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *pRgba );
	virtual bool IsEqualTo( const char *pFontName, int tall, int weight, int blur, int scanlines, int flags );
	virtual bool IsValid();
	virtual void GetCharABCWidths( int ch, int &a, int &b, int &c );
	virtual int GetHeight();
	virtual int GetAscent();
	virtual int GetMaxCharWidth();
	virtual int GetFlags();
	virtual bool GetUnderlined() { return m_bUnderlined; }
	const char *GetName() { return m_szName.String(); }
	virtual int GetWeight() { return m_iWeight; }
	virtual void GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC );
	virtual bool CreateFromMemory( const char *pFontName, void *pData, int size, int tall, int weight, int blur, int scanlines, int flags );

protected:
	CUtlString m_szName;
	int m_iTall;
	int m_iWeight;
	int m_iFlags;
	bool m_bAntiAliased;
	bool m_bRotary;
	bool m_bAdditive;
	int m_iDropShadowOffset;
	bool m_bUnderlined;
	int m_iOutlineSize;
	int m_iHeight;
	int m_iMaxCharWidth;
	int m_iAscent;
	int m_iScanLines;
	int m_iBlur;
};

#endif
