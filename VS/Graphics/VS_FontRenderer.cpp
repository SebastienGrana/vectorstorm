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

// #define USE_EXPLICIT_ARRAY	// if set, we'll use vertex/texel arrays, instead of VBOs.
// #define FONTS_DRAW_FROM_BASE	// if set, fonts get drawn with their baseline at 0.  If not, the middle of the font goes there.

static float s_globalFontScale = 1.f;
static vsDisplayList s_tempFontList(1024*10);

static float GetDisplayListWidth( vsDisplayList *list )
{
	vsVector2D topLeft, bottomRight;

	list->GetBoundingBox(topLeft, bottomRight);
	topLeft.x = 0.f;
	float width = bottomRight.x - topLeft.x;
	return width;
}

void
vsFontRenderer::SetGlobalFontScale( float scale )
{
	s_globalFontScale = scale;
}

vsFontRenderer::vsFontRenderer( vsFont *font, JustificationType type ):
	m_font(font),
	m_size(s_globalFontScale * font->GetNativeSize()),
	m_bounds(-1.f,-1.f),
	m_justification(type),
	m_color(),
	m_hasColor(false)
{
}

vsFontRenderer::vsFontRenderer( vsFont *font, float size, JustificationType type ):
	m_font(font),
	m_size(s_globalFontScale * size),
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
	float lineMargin = m_font->m_lineSpacing;

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

vsFragment*
vsFontRenderer::CreateString_Fragment( FontContext context, const vsString& string )
{
	size_t stringLength = string.length();
	float size = m_size;

	if ( stringLength == 0 )
		return NULL;

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
	float lineMargin = lineHeight * m_font->m_lineSpacing;
	for ( int i = 0; i < m_wrappedLineCount; i++ )
	{
		offset.y = topLinePosition + (i*(lineHeight+lineMargin));

		AppendStringToArrays( &constructor, m_wrappedLine[i].c_str(), size_vec, m_justification, offset );
	}

	ptBuffer->SetArray( constructor.ptArray, constructor.ptIndex );
	tlBuffer->SetArray( constructor.tlArray, constructor.tlIndex );

	vsFragment *fragment = new vsFragment;

	fragment->SetMaterial( m_font->m_material );
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
	if ( m_transform != vsTransform3D::Identity )
		list->PopTransform();

	fragment->SetDisplayList( list );

	vsDeleteArray( ptArray );
	vsDeleteArray( tlArray );

	return fragment;
}

void
vsFontRenderer::CreateString_InDisplayList( FontContext context, vsDisplayList *list, const vsString &string )
{
	list->SetMaterial( m_font->m_material );
	if ( m_hasColor )
		list->SetColor( m_color );

	if ( m_transform != vsTransform3D::Identity )
		list->PushTransform( m_transform );

	float size, topLinePosition;
	WrapStringSizeTop(string, &size, &topLinePosition);

	float lineHeight = 1.f;
	float lineMargin = m_font->m_lineSpacing;

	vsVector2D offset(0.f,topLinePosition);

	for ( int i = 0; i < m_wrappedLineCount; i++ )
	{
		offset.y = topLinePosition + (i*(lineHeight+lineMargin));
		s_tempFontList.Clear();
		BuildDisplayListGeometryFromString( context, &s_tempFontList, m_wrappedLine[i].c_str(), m_size, m_justification, offset );
		list->Append(s_tempFontList);
	}

	if ( m_transform != vsTransform3D::Identity )
		list->PopTransform();
}

vsFragment*
vsFontRenderer::Fragment2D( const vsString& string )
{
	return CreateString_Fragment(FontContext_2D, string);
}

vsFragment*
vsFontRenderer::Fragment3D( const vsString& string )
{
	return CreateString_Fragment(FontContext_3D, string);
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
		bool outOfSpace = (maxWidth > 0.f && m_font->GetStringWidth(line, size) > maxWidth && lineEnd > 0) ;
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

		BuildDisplayListGeometryFromString( FontContext_2D, &s_tempFontList, string, size.x, Justification_Left, vsVector2D::Zero);
		float width = GetDisplayListWidth(&s_tempFontList);

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
		vsGlyph *g = m_font->FindGlyphForCharacter( string[i] );

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
		BuildDisplayListGeometryFromString( context, list, string, size, Justification_Left, vsVector2D::Zero );
		float width = GetDisplayListWidth(list);

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

#if defined(USE_EXPLICIT_ARRAY)
	vsVector3D *vertexArray = new vsVector3D[len*4];
	vsVector2D *texelArray = new vsVector2D[len*4];
	int *triangleListArray = new int[len*6];
	int vertexCount = 0;
	int triangleIndexCount = 0;
#endif

	for ( size_t i = 0; i < len; i++ )
	{
		vsGlyph *g = m_font->FindGlyphForCharacter( string[i] );

		if ( !g )
		{
			vsLog("Missing character in font: %c", string[i]);
		}
		else
		{
#if defined(USE_EXPLICIT_ARRAY)
			triangleListArray[triangleIndexCount++] = vertexCount;
			triangleListArray[triangleIndexCount++] = vertexCount + 2;
			triangleListArray[triangleIndexCount++] = vertexCount + 1;
			triangleListArray[triangleIndexCount++] = vertexCount + 1;
			triangleListArray[triangleIndexCount++] = vertexCount + 2;
			triangleListArray[triangleIndexCount++] = vertexCount + 3;

			for ( int vi = 0; vi < 4; vi++ )
			{
				vertexArray[vertexCount] = g->vertex[vi] + offset - g->baseline;
				texelArray[vertexCount] = g->texel[vi];
				vertexCount++;
			}
#else
			list->PushTranslation(offset - g->baseline);
			list->BindBuffer( &g->ptBuffer );
			list->TriangleListBuffer( &m_font->m_glyphTriangleList );

			list->PopTransform();
#endif
			offset.x += g->xAdvance;
		}
	}
#if defined(USE_EXPLICIT_ARRAY)
	list->VertexArray(vertexArray, vertexCount);
	list->TexelArray(texelArray, vertexCount);
	list->TriangleList(triangleListArray, triangleIndexCount);

	vsDeleteArray(vertexArray);
	vsDeleteArray(texelArray);
	vsDeleteArray(triangleListArray);
#endif

	list->PopTransform();
	list->SetMaterial( vsMaterial::White );
	list->ClearArrays();
}
