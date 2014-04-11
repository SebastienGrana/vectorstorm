/*
 *  VS_FontRenderer.cpp
 *  VectorStorm
 *
 *  Created by Trevor Powell on 15/03/2014
 *  Copyright 2014 Trevor Powell.  All rights reserved.
 *
 */

#include "VS_FontRenderer.h"
#include "VS_DisplayList.h"
#include "VS_Fragment.h"

// #define FONTS_DRAW_FROM_BASE	// if set, fonts get drawn with their baseline at 0.  If not, the middle of the font goes there.

static float s_globalFontScale = 1.f;
static vsDisplayList s_tempFontList(1024*10);

void
vsFontRenderer::SetGlobalFontScale( float scale )
{
	s_globalFontScale = scale;
}

vsFontRenderer::vsFontRenderer( vsFont *font, float size, JustificationType type ):
	m_font(font),
	m_size(size),
	m_bounds(-1.f,-1.f),
	m_justification(type),
	m_color(),
	m_hasColor(false)
{
}

void
vsFontRenderer::SetJustificationType(JustificationType type)
{
	m_justification = type;
}

void
vsFontRenderer::SetSize(float size)
{
	m_size = s_globalFontScale * size;
}

void
vsFontRenderer::SetMaxWidthAndHeight(float maxWidth, float maxHeight)
{
	m_bounds.Set( maxWidth, maxHeight );
}

void
vsFontRenderer::SetMaxWidthAndHeight(const vsVector2D &bounds)
{
	m_bounds = bounds;
}

void
vsFontRenderer::SetMaxWidth(float maxWidth)
{
	m_bounds.Set(maxWidth, -1.f);
}

void
vsFontRenderer::SetTransform( const vsTransform3D& transform )
{
	m_transform = transform;
}

void
vsFontRenderer::SetColor( const vsColor& color )
{
	m_hasColor = true;
	m_color = color;
}

void
vsFontRenderer::SetNoColor()
{
	m_hasColor = false;
}

void
vsFontRenderer::WrapStringSizeTop(const vsString &string, float *size_out, float *top_out)
{
	float size = m_size;
	bool fits = false;

	float lineHeight = 1.f;
	float lineMargin = m_font->Size(m_size)->m_lineSpacing;

	while ( !fits )
	{
		WrapLine(string, size);
		fits = true;
		if ( m_bounds.y != -1.f )
		{
			float totalScaledHeight = size * ((lineHeight * m_wrappedLineCount) + (lineMargin * (m_wrappedLineCount-1)));
			if ( totalScaledHeight > m_bounds.y )
			{
				fits = false;
				// try a smaller font.
				size *= 0.95f;
			}
		}
	}

	float totalHeight = (lineHeight * m_wrappedLineCount) + (lineMargin * (m_wrappedLineCount-1));
	float baseOffsetDown = totalHeight * 0.5f;
	float topLinePosition = baseOffsetDown - totalHeight + lineHeight;

	*size_out = size;
	*top_out = topLinePosition;
}

void
vsFontRenderer::CreateString_InFragment( FontContext context, vsFontFragment *fragment, const vsString& string )
{
	fragment->Clear();

	size_t stringLength = string.length();
	float size = m_size;

	if ( stringLength == 0 )
		return;

	size_t requiredSize = stringLength * 4;      // we need a maximum of four verts for each character in the string.
	size_t requiredTriangles = stringLength * 6; // three indices per triangle, two triangles per character, max.

	vsRenderBuffer::PT *ptArray = new vsRenderBuffer::PT[ requiredSize ];
	uint16_t *tlArray = new uint16_t[ requiredTriangles ];

	vsRenderBuffer *ptBuffer = new vsRenderBuffer;
	vsRenderBuffer *tlBuffer = new vsRenderBuffer;

	FragmentConstructor constructor;
	constructor.ptArray = ptArray;
	constructor.tlArray = tlArray;
	constructor.ptIndex = constructor.tlIndex = 0;

	// figure out our wrapping and final render size
	float topLinePosition;
	WrapStringSizeTop(string, &size, &topLinePosition);

	vsVector2D size_vec(size, size);
	if ( context == FontContext_3D )
	{
		size_vec.y *= -1.f;	// upside down, in 3D context!
	}

	vsVector2D offset(0.f,topLinePosition);
#ifdef FONTS_DRAW_FROM_BASE
	// TODO:  This seems sensible, but it gets complicated when strings wrap --
	// do we move the top line in that case?  Maybe this needs to be (yet) more
	// configurable settings for the font drawing code?
	float totalHeight = lineHeight * m_size;
	if ( m_wrappedLineCount > 1 )
	{
		totalHeight += (lineHeight + lineMargin) * (m_wrappedLineCount-1);
	}
	float totalExtraHeight = totalHeight - (lineHeight * m_size);
	float baseOffsetDown = vsFloor(totalExtraHeight * 0.5f);
	float topLinePosition = baseOffsetDown - totalExtraHeight;
	vsVector2D offset(0.f,topLinePosition / m_size);
#endif

	float lineHeight = 1.f;
	float lineMargin = lineHeight * m_font->Size(m_size)->m_lineSpacing;
	for ( int i = 0; i < m_wrappedLineCount; i++ )
	{
		offset.y = topLinePosition + (i*(lineHeight+lineMargin));

		AppendStringToArrays( &constructor, m_wrappedLine[i].c_str(), size_vec, m_justification, offset );
	}

	ptBuffer->SetArray( constructor.ptArray, constructor.ptIndex );
	tlBuffer->SetArray( constructor.tlArray, constructor.tlIndex );

	fragment->SetMaterial( m_font->Size(m_size)->m_material );
	fragment->AddBuffer( ptBuffer );
	fragment->AddBuffer( tlBuffer );

	vsDisplayList *list = new vsDisplayList(60);

	if ( m_transform != vsTransform3D::Identity )
		list->PushTransform( m_transform );
	list->SnapMatrix();
	if ( m_hasColor )
		list->SetColor( m_color );
	list->BindBuffer( ptBuffer );
	list->TriangleListBuffer( tlBuffer );
	list->ClearArrays();
	list->PopTransform();
	if ( m_hasColor )
		list->SetColor( c_white );
	if ( m_transform != vsTransform3D::Identity )
		list->PopTransform();

	fragment->SetDisplayList( list );

	vsDeleteArray( ptArray );
	vsDeleteArray( tlArray );
}

void
vsFontRenderer::CreateString_InDisplayList( FontContext context, vsDisplayList *list, const vsString &string )
{
	list->SetMaterial( m_font->Size(m_size)->m_material );
	if ( m_hasColor )
		list->SetColor( m_color );

	if ( m_transform != vsTransform3D::Identity )
		list->PushTransform( m_transform );
	list->SnapMatrix();

	float size, topLinePosition;
	WrapStringSizeTop(string, &size, &topLinePosition);

	float lineHeight = 1.f;
	float lineMargin = m_font->Size(m_size)->m_lineSpacing;

	vsVector2D offset(0.f,topLinePosition);

	for ( int i = 0; i < m_wrappedLineCount; i++ )
	{
		offset.y = topLinePosition + (i*(lineHeight+lineMargin));
		s_tempFontList.Clear();
		BuildDisplayListGeometryFromString( context, &s_tempFontList, m_wrappedLine[i].c_str(), m_size, m_justification, offset );
		list->Append(s_tempFontList);
	}

	if ( m_hasColor )
		list->SetColor( c_white );

	list->PopTransform();
	if ( m_transform != vsTransform3D::Identity )
		list->PopTransform();
}

vsFragment*
vsFontRenderer::Fragment2D( const vsString& string )
{
	vsFontFragment *fragment = new vsFontFragment(*this, FontContext_2D, string);
	CreateString_InFragment(FontContext_2D, fragment, string);
	m_font->RegisterFragment(fragment);
	if ( fragment->GetDisplayList() == NULL )
	{
		vsDelete(fragment);
		return NULL;
	}
	return fragment;
}

vsFragment*
vsFontRenderer::Fragment3D( const vsString& string )
{
	vsFontFragment *fragment = new vsFontFragment(*this, FontContext_3D, string);
	CreateString_InFragment(FontContext_3D, fragment, string);
	m_font->RegisterFragment(fragment);
	if ( fragment->GetDisplayList() == NULL )
	{
		vsDelete(fragment);
		return NULL;
	}
	return fragment;
}

vsDisplayList*
vsFontRenderer::DisplayList2D( const vsString& string )
{
	vsDisplayList *loader = new vsDisplayList(1024 * 10);
	CreateString_InDisplayList(FontContext_2D, loader, string);

	vsDisplayList *result = new vsDisplayList( loader->GetSize() );
	result->Append(*loader);
	delete loader;

	return result;
}

vsDisplayList*
vsFontRenderer::DisplayList3D( const vsString& string )
{
	vsDisplayList *loader = new vsDisplayList(1024 * 10);
	CreateString_InDisplayList(FontContext_3D, loader, string);

	vsDisplayList *result = new vsDisplayList( loader->GetSize() );
	result->Append(*loader);
	delete loader;

	return result;
}

void
vsFontRenderer::DisplayList2D( vsDisplayList *list, const vsString& string )
{
	vsDisplayList *loader = new vsDisplayList(1024 * 10);
	CreateString_InDisplayList(FontContext_3D, loader, string);

	list->Append(*loader);
	delete loader;
}

void
vsFontRenderer::DisplayList3D( vsDisplayList *list, const vsString& string )
{
	vsDisplayList *loader = new vsDisplayList(1024 * 10);
	CreateString_InDisplayList(FontContext_3D, loader, string);

	list->Append(*loader);
	delete loader;
}

void
vsFontRenderer::WrapLine(const vsString &string, float size)
{
	vsString remainingString = string;

	// okay, check where we need to wordwrap
	size_t lineEnd = 0;
	size_t seekPosition = 0;

	float maxWidth = m_bounds.x;

	m_wrappedLineCount = 0;

	while ( remainingString != vsEmptyString )
	{
		vsString line = remainingString;
		seekPosition = remainingString.find_first_of(" \n",seekPosition+1);
//		newlinePosition = remainingString.find('\n',newlinePosition+1);

		if ( seekPosition != vsString::npos )
		{
			line.erase(seekPosition);
		}

		// check if we want to do a line break here!
		bool outOfSpace = (maxWidth > 0.f && m_font->Size(m_size)->GetStringWidth(line, size) > maxWidth && lineEnd > 0) ;
		bool wrapping = outOfSpace;
		if ( seekPosition != vsString::npos )
		{
			if ( remainingString[seekPosition] == '\n' )	// newline?
			{
				lineEnd = seekPosition;
				wrapping = true;
			}
		}

		if ( wrapping )
		{
			// time to wrap!
			line.erase(lineEnd);
			remainingString.erase(0,lineEnd+1);
			// we've gone too far, and need to wrap!  Wrap to the last safe wrap point we found.

			m_wrappedLine[m_wrappedLineCount++] = line;

			lineEnd = 0;
			seekPosition = 0;
		}
		else if ( seekPosition == vsString::npos )
		{
			m_wrappedLine[m_wrappedLineCount] = remainingString;
			m_wrappedLineCount++;
			remainingString = vsEmptyString;
		}
		else
		{
			lineEnd = seekPosition;
		}
	}
}

void
vsFontRenderer::AppendStringToArrays( vsFontRenderer::FragmentConstructor *constructor, const char* string, const vsVector2D &size, JustificationType j, const vsVector2D &offset_in)
{
	vsVector2D offset = offset_in;
	size_t len = strlen(string);

	if ( j != Justification_Left )
	{
		s_tempFontList.Clear();

		float width = m_font->Size(m_size)->GetStringWidth(string, size.x);

		if ( j == Justification_Right )
			offset.x = -width;
		if ( j == Justification_Center )
			offset.x = -(width*0.5f);

		offset.x *= (1.f / size.x);

		s_tempFontList.Clear();
	}

	uint16_t glyphIndices[6] = { 0, 2, 1, 1, 2, 3 };

	for ( size_t i = 0; i < len; i++ )
	{
		vsGlyph *g = m_font->Size(m_size)->FindGlyphForCharacter( string[i] );

		if ( !g )
		{
			vsLog("Missing character in font: %c", string[i]);
		}
		else
		{
			vsVector2D characterOffset = offset - g->baseline;
			vsVector2D scaledPosition;

			// now, add our four verts and two triangles onto the arrays.

			for ( int i = 0; i < 4; i++ )
			{
				scaledPosition = g->vertex[i] + characterOffset;
				scaledPosition.x *= size.x;
				scaledPosition.y *= size.y;

				constructor->ptArray[ constructor->ptIndex+i ].position = scaledPosition;
				constructor->ptArray[ constructor->ptIndex+i ].texel = g->texel[i];
			}
			for ( int i = 0; i < 6; i++ )
			{
				constructor->tlArray[ constructor->tlIndex+i ] = constructor->ptIndex + glyphIndices[i];
			}

			constructor->ptIndex += 4;
			constructor->tlIndex += 6;

			offset.x += g->xAdvance;
		}
	}
}

void
vsFontRenderer::BuildDisplayListGeometryFromString( FontContext context, vsDisplayList * list, const char* string, float size, JustificationType j, const vsVector2D &offset_in)
{
	vsVector2D offset = offset_in;
	size_t len = strlen(string);

	if ( j != Justification_Left )
	{
		float width = m_font->Size(m_size)->GetStringWidth(string, size);

		if ( j == Justification_Right )
			offset.x = -width;
		if ( j == Justification_Center )
			offset.x = -(width*0.5f);

		offset.x *= (1.f / size);

		list->Clear();
	}

	vsTransform2D transform;
	float ysize = size;
	if ( context == FontContext_3D )
	{
		ysize *= -1.f;	// upside down, in 3D context!
	}
	transform.SetScale( vsVector2D(size,ysize) );
	list->PushTransform(transform);
	list->BindBuffer( m_font->Size(m_size)->m_ptBuffer );

	for ( size_t i = 0; i < len; i++ )
	{
		vsGlyph *g = m_font->Size(m_size)->FindGlyphForCharacter( string[i] );

		if ( !g )
		{
			vsLog("Missing character in font: %c", string[i]);
		}
		else
		{
			list->PushTranslation(offset - g->baseline);
			list->TriangleStripBuffer( &g->tsBuffer );
			list->PopTransform();
			offset.x += g->xAdvance;
		}
	}

	list->PopTransform();
	list->ClearArrays();
}

vsFontFragment::vsFontFragment( vsFontRenderer& renderer, FontContext fc, const vsString& string ):
	m_renderer(renderer),
	m_context(fc),
	m_string(string),
	m_attached(true)
{
}

vsFontFragment::~vsFontFragment()
{
	if ( m_attached )
		m_renderer.m_font->RemoveFragment(this);
}

void
vsFontFragment::Rebuild()
{
	if ( m_attached )
	{
		Clear();
		m_renderer.CreateString_InFragment( m_context, this, m_string );
	}
}

void
vsFontFragment::Detach()
{
	m_attached = false;
}

