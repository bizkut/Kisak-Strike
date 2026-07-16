//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include <string.h>

#include <vgui_controls/Panel.h>
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"

#include "vgui_internal.h"
#include "ScalableImageBorder.h"
#include "keyvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#if defined( PLATFORM_PS4 )
extern "C" void KisakPs4StartupBreadcrumb( const char *line );
static void Ps4ScalableBorderBreadcrumb( const char *stage, int value )
{
	char line[256];
	Q_snprintf( line, sizeof(line), "kisak-ps4: scalable border %s value=%d",
		stage ? stage : "<null>", value );
	KisakPs4StartupBreadcrumb( line );
}
#define PS4_SCALABLE_BORDER_BREADCRUMB( stage, value ) Ps4ScalableBorderBreadcrumb( stage, value )
#else
#define PS4_SCALABLE_BORDER_BREADCRUMB( stage, value ) ((void)0)
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ScalableImageBorder::ScalableImageBorder()
{
	PS4_SCALABLE_BORDER_BREADCRUMB( "constructor entered", 0 );
	_inset[0]=0;
	_inset[1]=0;
	_inset[2]=0;
	_inset[3]=0;
	_name = NULL;
	m_eBackgroundType = IBorder::BACKGROUND_TEXTURED;

	m_iSrcCornerHeight = 0;
	m_iSrcCornerWidth = 0;
	m_iCornerHeight = 0;
	m_iCornerWidth = 0;
	m_pszImageName = NULL;
	PS4_SCALABLE_BORDER_BREADCRUMB( "constructor surface state", g_pSurface ? 1 : 0 );
	PS4_SCALABLE_BORDER_BREADCRUMB( "constructor before texture id", 0 );
	m_iTextureID = g_pSurface->CreateNewTextureID();
	PS4_SCALABLE_BORDER_BREADCRUMB( "constructor complete", m_iTextureID );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ScalableImageBorder::~ScalableImageBorder()
{
	delete [] _name;
	if ( m_pszImageName )
	{
		delete [] m_pszImageName;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::SetImage(const char *imageName)
{
	PS4_SCALABLE_BORDER_BREADCRUMB( "set image entered", imageName ? 1 : 0 );
	if ( m_pszImageName )
	{
		delete [] m_pszImageName;
		m_pszImageName = NULL;
	}
	PS4_SCALABLE_BORDER_BREADCRUMB( "set image cleanup ready", 0 );

	PS4_SCALABLE_BORDER_BREADCRUMB( "set image before nonempty check", 0 );
	if (*imageName)
	{
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image nonempty", 1 );
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image before length", 0 );
		int len = Q_strlen(imageName) + 1 + 5;	// 5 for "vgui/"
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image length ready", len );
		delete [] m_pszImageName;
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image before allocation", len );
		m_pszImageName = new char[ len ];
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image allocation ready", m_pszImageName ? 1 : 0 );
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image before path format", 0 );
		Q_snprintf( m_pszImageName, len, "vgui/%s", imageName );
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image path ready", 0 );

		PS4_SCALABLE_BORDER_BREADCRUMB( "set image before texture file", m_iTextureID );
		g_pSurface->DrawSetTextureFile( m_iTextureID, m_pszImageName, true, false);
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image texture file ready", m_iTextureID );

		// get image dimensions, compare to m_iSrcCornerHeight, m_iSrcCornerWidth
		int wide,tall;
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image before texture size", m_iTextureID );
		g_pSurface->DrawGetTextureSize( m_iTextureID, wide, tall );
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image texture size ready", ( wide > 0 && tall > 0 ) ? 1 : 0 );

		m_flCornerWidthPercent = ( wide > 0 ) ? ( (float)m_iSrcCornerWidth / (float)wide ) : 0;
		m_flCornerHeightPercent = ( tall > 0 ) ? ( (float)m_iSrcCornerHeight / (float)tall ) : 0;
	}
	else
	{
		PS4_SCALABLE_BORDER_BREADCRUMB( "set image empty", 0 );
	}
	PS4_SCALABLE_BORDER_BREADCRUMB( "set image complete", m_iTextureID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::SetInset(int left,int top,int right,int bottom)
{
	_inset[SIDE_LEFT] = left;
	_inset[SIDE_TOP] = top;
	_inset[SIDE_RIGHT] = right;
	_inset[SIDE_BOTTOM] = bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::GetInset(int& left,int& top,int& right,int& bottom)
{
	left = _inset[SIDE_LEFT];
	top = _inset[SIDE_TOP];
	right = _inset[SIDE_RIGHT];
	bottom = _inset[SIDE_BOTTOM];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::Paint(int x, int y, int wide, int tall)
{
	Paint(x, y, wide, tall, -1, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: Draws the border with the specified size
//-----------------------------------------------------------------------------
void ScalableImageBorder::Paint(int x, int y, int wide, int tall, int breakSide, int breakStart, int breakEnd)
{
	if ( !m_pszImageName || !m_pszImageName[0] )
		return;

	g_pSurface->DrawSetColor( m_Color );
	g_pSurface->DrawSetTexture( m_iTextureID );

	float uvx = 0;
	float uvy = 0;
	float uvw, uvh;

	float drawW, drawH;

	int row, col;
	for ( row=0;row<3;row++ )
	{
		x = 0;
		uvx = 0;

		if ( row == 0 || row == 2 )
		{
			//uvh - row 0 or 2, is src_corner_height
			uvh = m_flCornerHeightPercent;
			drawH = m_iCornerHeight;
		}
		else
		{
			//uvh - row 1, is tall - ( 2 * src_corner_height ) ( min 0 )
			uvh = MAX( 1.0 - 2 * m_flCornerHeightPercent, 0.0f );
			drawH = MAX( 0, ( tall - 2 * m_iCornerHeight ) );
		}

		for ( col=0;col<3;col++ )
		{
			if ( col == 0 || col == 2 )
			{
				//uvw - col 0 or 2, is src_corner_width
				uvw = m_flCornerWidthPercent;
				drawW = m_iCornerWidth;
			}
			else
			{
				//uvw - col 1, is wide - ( 2 * src_corner_width ) ( min 0 )
				uvw = MAX( 1.0 - 2 * m_flCornerWidthPercent, 0.0f );
				drawW = MAX( 0, ( wide - 2 * m_iCornerWidth ) );
			}

			Vector2D uv11( uvx, uvy );
			Vector2D uv21( uvx+uvw, uvy );
			Vector2D uv22( uvx+uvw, uvy+uvh );
			Vector2D uv12( uvx, uvy+uvh );

			vgui::Vertex_t verts[4];
			verts[0].Init( Vector2D( x, y ), uv11 );
			verts[1].Init( Vector2D( x+drawW, y ), uv21 );
			verts[2].Init( Vector2D( x+drawW, y+drawH ), uv22 );
			verts[3].Init( Vector2D( x, y+drawH ), uv12  );

			g_pSurface->DrawTexturedPolygon( 4, verts );	

			x += drawW;
			uvx += uvw;
		}

		y += drawH;
		uvy += uvh;
	}

	g_pSurface->DrawSetTexture(0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::Paint(VPANEL panel)
{
	// get panel size
	int wide, tall;
	ipanel()->GetSize( panel, wide, tall );
	Paint(0, 0, wide, tall, -1, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::ApplySchemeSettings(IScheme *pScheme, KeyValues *inResourceData)
{
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings entered", pScheme ? 1 : 0 );
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings before background", 0 );
	m_eBackgroundType = (backgroundtype_e)inResourceData->GetInt("backgroundtype");
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings background ready", m_eBackgroundType );

	PS4_SCALABLE_BORDER_BREADCRUMB( "settings before corners", 0 );
	m_iSrcCornerHeight = inResourceData->GetInt( "src_corner_height" );
	m_iSrcCornerWidth = inResourceData->GetInt( "src_corner_width" );
	m_iCornerHeight = inResourceData->GetInt( "draw_corner_height" );
	m_iCornerWidth = inResourceData->GetInt( "draw_corner_width" );
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings corners ready", 0 );

	// scale the x and y up to our screen co-ords
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings before proportional scale", 0 );
	m_iCornerHeight = scheme()->GetProportionalScaledValue( m_iCornerHeight);
	m_iCornerWidth = scheme()->GetProportionalScaledValue(m_iCornerWidth);
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings proportional scale ready", 0 );

	PS4_SCALABLE_BORDER_BREADCRUMB( "settings before image lookup", 0 );
	const char *imageName = inResourceData->GetString("image", "");
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings image lookup ready", ( imageName && imageName[0] ) ? 1 : 0 );
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings before set image", 0 );
	SetImage( imageName );
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings set image ready", 0 );

	PS4_SCALABLE_BORDER_BREADCRUMB( "settings before paint first", 0 );
	m_bPaintFirst = inResourceData->GetBool("paintfirst", true );
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings paint first ready", m_bPaintFirst ? 1 : 0 );

	PS4_SCALABLE_BORDER_BREADCRUMB( "settings before color", 0 );
	const char *col = inResourceData->GetString("color", NULL);
	if ( col && col[0] )
	{
		m_Color = pScheme->GetColor(col, Color(255, 255, 255, 255));
	}
	else
	{
		m_Color = Color(255, 255, 255, 255);
	}
	PS4_SCALABLE_BORDER_BREADCRUMB( "settings complete", 0 );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *ScalableImageBorder::GetName()
{
	if (_name)
		return _name;
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void ScalableImageBorder::SetName(const char *name)
{
	if (_name)
	{
		delete [] _name;
	}

	int len = Q_strlen(name) + 1;
	_name = new char[ len ];
	Q_strncpy( _name, name, len );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IBorder::backgroundtype_e ScalableImageBorder::GetBackgroundType()
{
	return m_eBackgroundType;
}
