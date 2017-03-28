/*
 *  VS_Lines.cpp
 *  VectorStorm
 *
 *  Created by Trevor Powell on 16/05/2014
 *  Copyright 2014 Trevor Powell.  All rights reserved.
 *
 */

#include "VS_Lines.h"
#include "VS_RenderQueue.h"
#include "VS_Scene.h"
#include "VS_Camera.h"

float vsLines3D::s_widthFactor = 1.f;

vsFragment *vsLineList2D( const vsString &material, vsVector2D *point, vsColor *color, int count, float width )
{
	size_t vertexCount = (count/2) * 4;
	size_t indexCount = (count/2) * 6;

	vsRenderBuffer::PC *va = new vsRenderBuffer::PC[vertexCount];
	uint16_t *ia = new uint16_t[indexCount];
	int vertexCursor = 0;
	int indexCursor = 0;
	float halfWidth = width * 0.5f;

	for ( int i = 0; i < count; i+=2 )
	{
		int preI = i;
		int postI = i+1;

		vsVector3D dirOfTravel = point[postI] - point[preI];

		dirOfTravel.NormaliseSafe();

		vsVector2D offsetPre( dirOfTravel.y, -dirOfTravel.x );
		offsetPre.NormaliseSafe();

		va[vertexCursor].position = point[preI] - (offsetPre * halfWidth);
		va[vertexCursor+1].position = point[preI] + (offsetPre * halfWidth);
		va[vertexCursor+2].position = point[postI] - (offsetPre * halfWidth);
		va[vertexCursor+3].position = point[postI] + (offsetPre * halfWidth);

		if ( color )
		{
			va[vertexCursor].color = color[preI];
			va[vertexCursor+1].color = color[preI];
			va[vertexCursor+2].color = color[postI];
			va[vertexCursor+3].color = color[postI];
		}
		else
		{
			va[vertexCursor].color = c_white;
			va[vertexCursor+1].color = c_white;
			va[vertexCursor+2].color = c_white;
			va[vertexCursor+3].color = c_white;
		}

		ia[indexCursor] = vertexCursor;
		ia[indexCursor+1] = vertexCursor+1;
		ia[indexCursor+2] = vertexCursor+2;
		ia[indexCursor+3] = vertexCursor+2;
		ia[indexCursor+4] = vertexCursor+1;
		ia[indexCursor+5] = vertexCursor+3;
		indexCursor += 6;
		vertexCursor += 4;
	}

	vsRenderBuffer* vertexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vsRenderBuffer* indexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vertexBuffer->SetArray(va, vertexCursor);
	indexBuffer->SetArray(ia, indexCursor);
	vsFragment *fragment = new vsFragment;
	vsDisplayList *dlist = new vsDisplayList(128);
	dlist->BindBuffer(vertexBuffer);
	dlist->TriangleListBuffer(indexBuffer);
	dlist->ClearArrays();
	fragment->SetDisplayList(dlist);
	fragment->SetMaterial( material );
	fragment->AddBuffer(vertexBuffer);
	fragment->AddBuffer(indexBuffer);

	vsDeleteArray(va);
	vsDeleteArray(ia);

	return fragment;
}

vsFragment *vsLineStrip2D( const vsString& material, vsVector2D *point, vsColor *color, int count, float width, bool loop )
{
	width *= 0.707f;
	size_t vertexCount = count * 4;
	size_t indexCount = count * 18;

	float halfWidth = width * 0.5f;

	vsRenderBuffer::PC *va = new vsRenderBuffer::PC[vertexCount];
	uint16_t *ia = new uint16_t[indexCount];
	int vertexCursor = 0;
	int indexCursor = 0;

	for ( int i = 0; i < count; i++ )
	{
		vsVector3D dirOfTravel;

		int midI = i;
		int preI = midI-1;
		int postI = midI+1;

		// float widthHere = m_width;

		if ( postI >= count )
		{
			if ( loop )
				postI = 0;
			else
				postI = count-1;
		}
		if ( preI < 0 )
		{
			if ( loop )
				preI = count-1;
			else
				preI = 0;
		}

		vsVector2D dirOfTravelPre = point[midI] - point[preI];
		vsVector2D dirOfTravelPost = point[postI] - point[midI];
		float distanceOfTravelPre = dirOfTravelPre.Length();
		float distanceOfTravelPost = dirOfTravelPost.Length();
		dirOfTravelPre.NormaliseSafe();
		dirOfTravelPost.NormaliseSafe();

		vsVector2D offsetPre( dirOfTravelPre.y, -dirOfTravelPre.x );
		vsVector2D offsetPost( dirOfTravelPost.y, -dirOfTravelPost.x );
		offsetPre.NormaliseSafe();
		offsetPost.NormaliseSafe();

		if ( preI == midI )
		{
			offsetPre = offsetPost;
			dirOfTravelPre = dirOfTravelPost;
		}
		else if ( midI == postI )
		{
			offsetPost = offsetPre;
			dirOfTravelPost = dirOfTravelPre;
		}

		vsVector3D vertexPosition;
		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D insidePre = point[preI] - (offsetPre * halfWidth);
			vsVector3D insidePost = point[postI] - (offsetPost * halfWidth);

			vsSqDistanceBetweenLineSegments( insidePre,
					insidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * halfWidth),
					insidePost,
					insidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * halfWidth),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = point[midI] - offsetPre * halfWidth;
		}

		va[vertexCursor+0].position = (vertexPosition - point[midI]) + vertexPosition;
		va[vertexCursor+1].position = vertexPosition;

		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D outsidePre = point[preI] + (offsetPre * halfWidth);
			vsVector3D outsidePost = point[postI] + (offsetPost * halfWidth);

			vsSqDistanceBetweenLineSegments( outsidePre,
					outsidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * halfWidth),
					outsidePost,
					outsidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * halfWidth),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = point[midI] + offsetPre * halfWidth;
		}

		va[vertexCursor+2].position = vertexPosition;
		va[vertexCursor+3].position = (vertexPosition - point[midI]) + vertexPosition;
		if ( color )
		{
			va[vertexCursor+0].color = color[midI];
			va[vertexCursor+1].color = color[midI];
			va[vertexCursor+2].color = color[midI];
			va[vertexCursor+3].color = color[midI];
		}
		else
		{
			va[vertexCursor+0].color = c_white;
			va[vertexCursor+1].color = c_white;
			va[vertexCursor+2].color = c_white;
			va[vertexCursor+3].color = c_white;
		}
		va[vertexCursor+0].color.a = 0.f;
		va[vertexCursor+3].color.a = 0.f;

		if ( loop || i != (count - 1) ) // not at the end of the strip
		{
			for ( int q = 0; q < 3; q++ )
			{
				int otherSide = vertexCursor+4;
				if ( i == count-1 )
					otherSide = 0 + q;
				ia[indexCursor] = vertexCursor;
				ia[indexCursor+1] = vertexCursor+1;
				ia[indexCursor+2] = otherSide;
				ia[indexCursor+3] = otherSide;
				ia[indexCursor+4] = vertexCursor+1;
				ia[indexCursor+5] = otherSide+1;
				indexCursor += 6;
				vertexCursor ++;
			}
			vertexCursor ++;
		}
		else
			vertexCursor += 4;
	}

	vsRenderBuffer* vertexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vsRenderBuffer* indexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vertexBuffer->SetArray(va, vertexCursor);
	indexBuffer->SetArray(ia, indexCursor);
	vsFragment *fragment = new vsFragment;
	vsDisplayList *dlist = new vsDisplayList(128);
	dlist->BindBuffer(vertexBuffer);
	dlist->TriangleListBuffer(indexBuffer);
	dlist->ClearArrays();
	fragment->SetDisplayList(dlist);
	fragment->SetMaterial( material );
	fragment->AddBuffer(vertexBuffer);
	fragment->AddBuffer(indexBuffer);

	vsDeleteArray(va);
	vsDeleteArray(ia);

	return fragment;
}

vsFragment *vsLineStrip2D( const vsString &material, vsVector2D *array, int count, float width, bool loop ) { return vsLineStrip2D(material,array,NULL,count,width,loop); }
vsFragment *vsLineList2D( const vsString &material, vsVector2D *array, int count, float width ) { return vsLineList2D(material,array,NULL,count,width); }

vsFragment *vsLineList3D( const vsString &material, vsVector3D *point, int count, float width, const vsColor *color_in )
{
	const vsColor *color = (color_in) ? color_in : &c_white;
	size_t vertexCount = (count/2) * 4;
	size_t indexCount = (count/2) * 6;

	vsRenderBuffer::PC *va = new vsRenderBuffer::PC[vertexCount];
	uint16_t *ia = new uint16_t[indexCount];
	int vertexCursor = 0;
	int indexCursor = 0;
	float halfWidth = width * 0.5f;

	for ( int i = 0; i < count; i+=2 )
	{
		int preI = i;
		int postI = i+1;

		vsVector3D dirOfTravel = point[postI] - point[preI];

		dirOfTravel.NormaliseSafe();

		vsVector3D up(0.f,1.f,0.f);
		vsVector3D offsetPre = dirOfTravel.Cross(up);
		offsetPre.NormaliseSafe();

		va[vertexCursor].position = point[preI] - (offsetPre * halfWidth);
		va[vertexCursor+1].position = point[preI] + (offsetPre * halfWidth);
		va[vertexCursor+2].position = point[postI] - (offsetPre * halfWidth);
		va[vertexCursor+3].position = point[postI] + (offsetPre * halfWidth);

		va[vertexCursor].color = *color;
		va[vertexCursor+1].color = *color;
		va[vertexCursor+2].color = *color;
		va[vertexCursor+3].color = *color;

		ia[indexCursor] = vertexCursor;
		ia[indexCursor+1] = vertexCursor+1;
		ia[indexCursor+2] = vertexCursor+2;
		ia[indexCursor+3] = vertexCursor+2;
		ia[indexCursor+4] = vertexCursor+1;
		ia[indexCursor+5] = vertexCursor+3;
		indexCursor += 6;
		vertexCursor += 4;
	}

	vsRenderBuffer* vertexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vsRenderBuffer* indexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vertexBuffer->SetArray(va, vertexCursor);
	indexBuffer->SetArray(ia, indexCursor);
	vsFragment *fragment = new vsFragment;
	vsDisplayList *dlist = new vsDisplayList(128);
	dlist->BindBuffer(vertexBuffer);
	dlist->TriangleListBuffer(indexBuffer);
	dlist->ClearArrays();
	fragment->SetDisplayList(dlist);
	fragment->SetMaterial( material );
	fragment->AddBuffer(vertexBuffer);
	fragment->AddBuffer(indexBuffer);

	vsDeleteArray(va);
	vsDeleteArray(ia);

	return fragment;
}

vsFragment *vsLineStrip3D( const vsString& material, vsVector3D *point, int count, float width, bool loop, const vsColor *color_in, float texScale )
{
	const vsColor *color = (color_in) ? color_in : &c_white;
	size_t vertexCount = count * 2;
	size_t indexCount = count * 6;

	float halfWidth = width * 0.5f;

	vsRenderBuffer::PCT *va = new vsRenderBuffer::PCT[vertexCount];
	uint16_t *ia = new uint16_t[indexCount];
	int vertexCursor = 0;
	int indexCursor = 0;
	float distance = 0.0f;

	for ( int i = 0; i < count; i++ )
	{
		vsVector3D dirOfTravel;

		int midI = i;
		int preI = midI-1;
		int postI = midI+1;

		// float widthHere = m_width;

		if ( postI >= count )
		{
			if ( loop )
				postI = 0;
			else
				postI = count-1;
		}
		if ( preI < 0 )
		{
			if ( loop )
				preI = count-1;
			else
			{
				preI = 0;
			}
		}

		vsVector3D dirOfTravelPre = point[midI] - point[preI];
		vsVector3D dirOfTravelPost = point[postI] - point[midI];
		float distanceOfTravelPre = dirOfTravelPre.Length();
		float distanceOfTravelPost = dirOfTravelPost.Length();
		dirOfTravelPre.NormaliseSafe();
		dirOfTravelPost.NormaliseSafe();

		vsVector3D up(0.f,1.f,0.f);

		vsVector3D offsetPre = dirOfTravelPre.Cross(up);
		vsVector3D offsetPost = dirOfTravelPost.Cross(up);
		if ( preI == midI )
		{
			offsetPre = offsetPost;
			dirOfTravelPre = dirOfTravelPost;
		}
		else if ( midI == postI )
		{
			offsetPost = offsetPre;
			dirOfTravelPost = dirOfTravelPre;
		}
		offsetPre.NormaliseSafe();
		offsetPost.NormaliseSafe();

		vsVector3D vertexPosition;
		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D insidePre = point[preI] - (offsetPre * halfWidth);
			vsVector3D insidePost = point[postI] - (offsetPost * halfWidth);

			vsSqDistanceBetweenLineSegments( insidePre,
					insidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * halfWidth),
					insidePost,
					insidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * halfWidth),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = point[midI] - offsetPre * halfWidth;
		}

		va[vertexCursor].position = vertexPosition;
		va[vertexCursor].color = *color;
		va[vertexCursor].texel.Set( 0.0, distance / texScale );

		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D outsidePre = point[preI] + (offsetPre * halfWidth);
			vsVector3D outsidePost = point[postI] + (offsetPost * halfWidth);

			vsSqDistanceBetweenLineSegments( outsidePre,
					outsidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * halfWidth),
					outsidePost,
					outsidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * halfWidth),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = point[midI] + offsetPre * halfWidth;
		}

		va[vertexCursor+1].position = vertexPosition;
		va[vertexCursor+1].color = *color;
		va[vertexCursor+1].texel.Set( width / texScale, distance / texScale);
		distance += (point[postI] - point[midI]).Length();

		if ( loop || i != count - 1 ) // not at the end of the strip
		{
			int otherSide = vertexCursor+2;
			if ( i == count-1 )
				otherSide = 0;
			ia[indexCursor] = vertexCursor;
			ia[indexCursor+1] = vertexCursor+1;
			ia[indexCursor+2] = otherSide;
			ia[indexCursor+3] = otherSide;
			ia[indexCursor+4] = vertexCursor+1;
			ia[indexCursor+5] = otherSide+1;
			indexCursor += 6;
		}
		vertexCursor += 2;
	}

	vsRenderBuffer* vertexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vsRenderBuffer* indexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vertexBuffer->SetArray(va, vertexCursor);
	indexBuffer->SetArray(ia, indexCursor);
	vsFragment *fragment = new vsFragment;
	vsDisplayList *dlist = new vsDisplayList(128);
	dlist->BindBuffer(vertexBuffer);
	dlist->TriangleListBuffer(indexBuffer);
	dlist->ClearArrays();
	fragment->SetDisplayList(dlist);
	fragment->SetMaterial( material );
	fragment->AddBuffer(vertexBuffer);
	fragment->AddBuffer(indexBuffer);

	vsDeleteArray(va);
	vsDeleteArray(ia);

	return fragment;
}

// color array version
vsFragment *vsLineList3D( const vsString &material, vsVector3D *point, vsColor *color, int count, float width)
{
	size_t vertexCount = (count/2) * 4;
	size_t indexCount = (count/2) * 6;

	vsRenderBuffer::PC *va = new vsRenderBuffer::PC[vertexCount];
	uint16_t *ia = new uint16_t[indexCount];
	int vertexCursor = 0;
	int indexCursor = 0;
	float halfWidth = width * 0.5f;

	for ( int i = 0; i < count; i+=2 )
	{
		int preI = i;
		int postI = i+1;

		vsVector3D dirOfTravel = point[postI] - point[preI];

		dirOfTravel.NormaliseSafe();

		vsVector3D up(0.f,1.f,0.f);
		vsVector3D offsetPre = dirOfTravel.Cross(up);
		offsetPre.NormaliseSafe();

		va[vertexCursor].position = point[preI] - (offsetPre * halfWidth);
		va[vertexCursor+1].position = point[preI] + (offsetPre * halfWidth);
		va[vertexCursor+2].position = point[postI] - (offsetPre * halfWidth);
		va[vertexCursor+3].position = point[postI] + (offsetPre * halfWidth);

		va[vertexCursor].color = color[preI];
		va[vertexCursor+1].color = color[preI];
		va[vertexCursor+2].color = color[postI];
		va[vertexCursor+3].color = color[postI];

		ia[indexCursor] = vertexCursor;
		ia[indexCursor+1] = vertexCursor+1;
		ia[indexCursor+2] = vertexCursor+2;
		ia[indexCursor+3] = vertexCursor+2;
		ia[indexCursor+4] = vertexCursor+1;
		ia[indexCursor+5] = vertexCursor+3;
		indexCursor += 6;
		vertexCursor += 4;
	}

	vsRenderBuffer* vertexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vsRenderBuffer* indexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vertexBuffer->SetArray(va, vertexCursor);
	indexBuffer->SetArray(ia, indexCursor);
	vsFragment *fragment = new vsFragment;
	vsDisplayList *dlist = new vsDisplayList(128);
	dlist->BindBuffer(vertexBuffer);
	dlist->TriangleListBuffer(indexBuffer);
	dlist->ClearArrays();
	fragment->SetDisplayList(dlist);
	fragment->SetMaterial( material );
	fragment->AddBuffer(vertexBuffer);
	fragment->AddBuffer(indexBuffer);

	vsDeleteArray(va);
	vsDeleteArray(ia);

	return fragment;
}

vsFragment *vsLineStrip3D( const vsString& material, vsVector3D *point, vsColor *color, int count, float width, bool loop )
{
	size_t vertexCount = count * 2;
	size_t indexCount = count * 6;

	float halfWidth = width * 0.5f;

	vsRenderBuffer::PCT *va = new vsRenderBuffer::PCT[vertexCount];
	uint16_t *ia = new uint16_t[indexCount];
	int vertexCursor = 0;
	int indexCursor = 0;
	float distance = 0.0;

	for ( int i = 0; i < count; i++ )
	{
		vsVector3D dirOfTravel;

		int midI = i;
		int preI = midI-1;
		int postI = midI+1;

		// float widthHere = m_width;

		if ( postI >= count )
		{
			if ( loop )
				postI = 0;
			else
				postI = count-1;
		}
		if ( preI < 0 )
		{
			if ( loop )
				preI = count-1;
			else
				preI = 0;
		}

		vsVector3D dirOfTravelPre = point[midI] - point[preI];
		vsVector3D dirOfTravelPost = point[postI] - point[midI];
		float distanceOfTravelPre = dirOfTravelPre.Length();
		float distanceOfTravelPost = dirOfTravelPost.Length();
		dirOfTravelPre.NormaliseSafe();
		dirOfTravelPost.NormaliseSafe();

		vsVector3D up(0.f,1.f,0.f);

		vsVector3D offsetPre = dirOfTravelPre.Cross(up);
		vsVector3D offsetPost = dirOfTravelPost.Cross(up);
		offsetPre.NormaliseSafe();
		offsetPost.NormaliseSafe();

		vsVector3D vertexPosition;
		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D insidePre = point[preI] - (offsetPre * halfWidth);
			vsVector3D insidePost = point[postI] - (offsetPost * halfWidth);

			vsSqDistanceBetweenLineSegments( insidePre,
					insidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * halfWidth),
					insidePost,
					insidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * halfWidth),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = point[midI] - offsetPre * halfWidth;
		}

		va[vertexCursor].position = vertexPosition;
		va[vertexCursor].color = color[midI];
		va[vertexCursor].texel.Set( 0.0, distance );

		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D outsidePre = point[preI] + (offsetPre * halfWidth);
			vsVector3D outsidePost = point[postI] + (offsetPost * halfWidth);

			vsSqDistanceBetweenLineSegments( outsidePre,
					outsidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * halfWidth),
					outsidePost,
					outsidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * halfWidth),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = point[midI] + offsetPre * halfWidth;
		}

		va[vertexCursor+1].position = vertexPosition;
		va[vertexCursor+1].color = color[midI];
		va[vertexCursor+1].texel.Set( width, distance );

		distance += (point[postI] - point[midI]).Length();

		if ( loop || i != count - 1 ) // not at the end of the strip
		{
			int otherSide = vertexCursor+2;
			if ( i == count-1 )
				otherSide = 0;
			ia[indexCursor] = vertexCursor;
			ia[indexCursor+1] = vertexCursor+1;
			ia[indexCursor+2] = otherSide;
			ia[indexCursor+3] = otherSide;
			ia[indexCursor+4] = vertexCursor+1;
			ia[indexCursor+5] = otherSide+1;
			indexCursor += 6;
		}
		vertexCursor += 2;
	}

	vsRenderBuffer* vertexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vsRenderBuffer* indexBuffer = new vsRenderBuffer( vsRenderBuffer::Type_Static );
	vertexBuffer->SetArray(va, vertexCursor);
	indexBuffer->SetArray(ia, indexCursor);
	vsFragment *fragment = new vsFragment;
	vsDisplayList *dlist = new vsDisplayList(128);
	dlist->BindBuffer(vertexBuffer);
	dlist->TriangleListBuffer(indexBuffer);
	dlist->ClearArrays();
	fragment->SetDisplayList(dlist);
	fragment->SetMaterial( material );
	fragment->AddBuffer(vertexBuffer);
	fragment->AddBuffer(indexBuffer);

	vsDeleteArray(va);
	vsDeleteArray(ia);

	return fragment;
}

class vsLines3D::Strip
{
public:
	vsVector3D *m_vertex;
	vsColor *m_color;
	int m_length;
	bool m_loop;

	Strip( vsVector3D *array, int length ):
		m_vertex( new vsVector3D[length] ),
		m_color( NULL ),
		m_length( length ),
		m_loop(false)
	{
		for ( int i = 0; i < length; i++ )
		{
			m_vertex[i] = array[i];
		}
	}
	Strip( vsVector3D *array, vsColor *carray, int length ):
		m_vertex( new vsVector3D[length] ),
		m_color( new vsColor[length] ),
		m_length( length ),
		m_loop(false)
	{
		for ( int i = 0; i < length; i++ )
		{
			m_vertex[i] = array[i];
			m_color[i] = carray[i];
		}
	}
	~Strip()
	{
		vsDeleteArray( m_vertex );
		vsDeleteArray( m_color );
	}
};

vsLines3D::vsLines3D( int maxStrips, float width, bool screenspace ):
	m_strip( new Strip*[maxStrips] ),
	m_stripCount( 0 ),
	m_maxStripCount( maxStrips ),
	m_leftWidth( width * 0.5f ),
	m_rightWidth( width * 0.5f ),
	m_widthInScreenspace( screenspace ),
	m_vertices( vsRenderBuffer::Type_Stream ),
	m_indices( vsRenderBuffer::Type_Stream ),
	m_constantViewDirection(),
	m_useConstantViewDirection(false)
{
	if ( m_widthInScreenspace )
	{
		// With our new wider drawing method (with alphaed edges),
		// we're drawing lines a bit wider than before.  So let's tamp
		// them back down to be approximately the same width that they
		// were before, so clients don't need to change all their code
		// that calls into us.
		m_leftWidth *= 0.707f;
		m_rightWidth *= 0.707f;
	}
}

vsLines3D::~vsLines3D()
{
	Clear();
	vsDeleteArray( m_strip );
}

void
vsLines3D::SetConstantViewDirection( const vsVector3D& direction )
{
	m_constantViewDirection = direction;
	m_useConstantViewDirection = true;
}

void
vsLines3D::Clear()
{
	for ( int i = 0; i < m_stripCount; i++ )
	{
		vsDelete( m_strip[i] );
	}
	m_stripCount = 0;
}

void
vsLines3D::AddLine( vsVector3D &a, vsVector3D &b )
{
	vsVector3D vert[2] = { a, b };
	AddStrip(vert, 2);
}

void
vsLines3D::AddStrip( vsVector3D *array, vsColor *carray, int arraySize )
{
	vsAssert( m_stripCount < m_maxStripCount, "Too many strips in vsLines3D" );
	int i = m_stripCount++;
	if ( carray )
		m_strip[i] = new Strip(array, carray, arraySize);
	else
		m_strip[i] = new Strip(array, arraySize);
}

void
vsLines3D::AddLoop( vsVector3D *array, vsColor *carray, int arraySize )
{
	vsAssert( m_stripCount < m_maxStripCount, "Too many strips in vsLines3D" );
	int i = m_stripCount++;

	if ( carray )
		m_strip[i] = new Strip(array, carray, arraySize);
	else
		m_strip[i] = new Strip(array, arraySize);
	m_strip[i]->m_loop = true;
}

size_t
vsLines3D::GetFinalVertexCount()
{
	size_t result = 0;
	for ( int i = 0; i < m_stripCount; i++ )
	{
		// we're going to emit FOUR vertices per strip vertex.
		//
		// Assuming (for the sake of this diagram) that our line
		// is travelling downward or upward, these vertices will be:
		//
		//  -> crossline direction
		//
		//  0  1  2  3
		//  t  o  o  t
		//
		//  (t == transparent)
		//  (o == opaque)
		//
		// The idea here is that if we're drawing one-pixel lines, this
		// should give us some simple anti-aliasing in the pixels between 0 and 1,
		// and between 2 and 3, even if MSAA is disabled
		//
		result += m_strip[i]->m_length * 4;
	}
	return result;
}

size_t
vsLines3D::GetFinalIndexCount()
{
	size_t result = 0;
	for ( int i = 0; i < m_stripCount; i++ )
	{
		// we're going to emit 18 indices for each quad.  We're going
		// to emit one quad for each strip vertex, except for the last one
		// of each strip.  ('n' vertices means 'n-1' quads)
		result += (m_strip[i]->m_length-1) * 18;
		if ( m_strip[i]->m_loop )
			result += 18;	// six more indices if we're looping, as we connect end->start
	}
	return result;
}

void
vsLines3D::DynamicDraw( vsRenderQueue *queue )
{
	m_vertexCursor = 0;
	m_indexCursor = 0;

	size_t vertexCount = GetFinalVertexCount();
	size_t indexCount = GetFinalIndexCount();
	if ( vertexCount == 0 || indexCount == 0 )
		return;

	m_vertices.ResizeArray( sizeof(vsVector3D) * vertexCount );
	m_colors.ResizeArray( sizeof(vsColor) * vertexCount );
	m_indices.ResizeArray( sizeof(uint16_t) * indexCount );

	for ( int i = 0; i < m_stripCount; i++ )
	{
		DrawStrip( queue, m_strip[i] );
	}

	m_vertices.BakeArray();
	m_colors.BakeArray();
	m_indices.BakeArray();

	vsDisplayList *	list = queue->MakeTemporaryBatchList( GetMaterial(), queue->GetMatrix(), 1024 );
	list->VertexBuffer(&m_vertices);
	list->ColorBuffer(&m_colors);
	list->TriangleListBuffer(&m_indices);
	list->ClearBuffers();
}

void
vsLines3D::DrawStrip( vsRenderQueue *queue, Strip *strip )
{
	int startOfStripVertexCursor = m_vertexCursor;
	float fullFov = queue->GetFOV();
	float fovPerPixel = fullFov / vsScreen::Instance()->GetHeight();
	float tanHalfFovPerPixel = 2.f * vsTan( 0.5f * fovPerPixel );

	// vsMatrix4x4 localToView = queue->GetMatrix() * queue->GetWorldToViewMatrix();
	vsMatrix4x4 localToView = queue->GetWorldToViewMatrix() * queue->GetMatrix() ;
	vsMatrix4x4 viewToLocal = localToView.Inverse();
	vsVector3D camPos = viewToLocal.ApplyTo(vsVector3D::Zero);

	vsVector3D *va = m_vertices.GetVector3DArray();
	vsColor *ca = m_colors.GetColorArray();
	uint16_t *ia = m_indices.GetIntArray();

	for ( int i = 0; i < strip->m_length; i++ )
	{
		vsVector3D dirOfTravel;

		int midI = i;
		int preI = midI-1;
		int postI = midI+1;

		if ( postI >= strip->m_length )
		{
			if ( strip->m_loop )
				postI = 0;
			else
				postI = strip->m_length-1;
		}
		if ( preI < 0 )
		{
			if ( strip->m_loop )
				preI = strip->m_length-1;
			else
				preI = 0;
		}

		vsVector3D cameraForward = strip->m_vertex[midI] - camPos;
		if ( m_useConstantViewDirection )
			cameraForward = m_constantViewDirection;
		// vsVector3D cameraForward = viewToLocal.z;
		cameraForward.NormaliseSafe();

		vsVector3D dirOfTravelPre = strip->m_vertex[midI] - strip->m_vertex[preI];
		vsVector3D dirOfTravelPost = strip->m_vertex[postI] - strip->m_vertex[midI];
		if ( midI == preI )
			dirOfTravelPre = dirOfTravelPost;
		if ( midI == postI )
			dirOfTravelPost = dirOfTravelPre;
		float distanceOfTravelPre = dirOfTravelPre.Length();
		float distanceOfTravelPost = dirOfTravelPost.Length();
		dirOfTravelPre.NormaliseSafe();
		dirOfTravelPost.NormaliseSafe();

		vsVector3D offsetPre = dirOfTravelPre.Cross( cameraForward );
		vsVector3D offsetPost = dirOfTravelPost.Cross( cameraForward );
		offsetPre.NormaliseSafe();
		offsetPost.NormaliseSafe();

		float leftWidthHere = m_leftWidth;
		float rightWidthHere = m_rightWidth;
		if ( m_widthInScreenspace )
		{
			if ( queue->IsOrthographic() )
			{
				leftWidthHere *= fovPerPixel;
				rightWidthHere *= fovPerPixel;
			}
			else
			{
				vsVector3D viewPos = localToView.ApplyTo( vsVector4D(strip->m_vertex[midI],1.f) );
				leftWidthHere *= tanHalfFovPerPixel * viewPos.z;
				rightWidthHere *= tanHalfFovPerPixel * viewPos.z;
			}
		}

		vsVector3D vertexPosition;
		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D insidePre = strip->m_vertex[preI] - (offsetPre * rightWidthHere);
			vsVector3D insidePost = strip->m_vertex[postI] - (offsetPost * rightWidthHere);

			vsSqDistanceBetweenLineSegments( insidePre,
					insidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * rightWidthHere),
					insidePost,
					insidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * rightWidthHere),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = strip->m_vertex[midI] - offsetPre * rightWidthHere;
		}

		va[m_vertexCursor+0] = (vertexPosition - strip->m_vertex[midI]) + vertexPosition;
		va[m_vertexCursor+1] = vertexPosition;

		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D outsidePre = strip->m_vertex[preI] + (offsetPre * leftWidthHere);
			vsVector3D outsidePost = strip->m_vertex[postI] + (offsetPost * leftWidthHere);

			vsSqDistanceBetweenLineSegments( outsidePre,
					outsidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * leftWidthHere),
					outsidePost,
					outsidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * leftWidthHere),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = strip->m_vertex[midI] + offsetPre * leftWidthHere;
		}

		va[m_vertexCursor+2] = vertexPosition;
		va[m_vertexCursor+3] = (vertexPosition - strip->m_vertex[midI]) + vertexPosition;

		if ( strip->m_color )
		{
			ca[m_vertexCursor+0] = strip->m_color[i];
			ca[m_vertexCursor+1] = strip->m_color[i];
			ca[m_vertexCursor+2] = strip->m_color[i];
			ca[m_vertexCursor+3] = strip->m_color[i];
		}
		else
		{
			ca[m_vertexCursor+0] = c_white;
			ca[m_vertexCursor+1] = c_white;
			ca[m_vertexCursor+2] = c_white;
			ca[m_vertexCursor+3] = c_white;
		}
		ca[m_vertexCursor+0].a = 0;
		ca[m_vertexCursor+3].a = 0;

		if ( i != strip->m_length - 1 ) // not at the end of the strip
		{
			for ( int i = 0; i < 3; i++ )
			{
				ia[m_indexCursor+0] = m_vertexCursor+0;
				ia[m_indexCursor+1] = m_vertexCursor+1;
				ia[m_indexCursor+2] = m_vertexCursor+4;
				ia[m_indexCursor+3] = m_vertexCursor+4;
				ia[m_indexCursor+4] = m_vertexCursor+1;
				ia[m_indexCursor+5] = m_vertexCursor+5;

				m_indexCursor += 6;
				m_vertexCursor ++;
			}
			m_vertexCursor ++;
		}
	}

	if ( strip->m_loop )
	{
		for ( int i = 0; i < 3; i++ )
		{
			// and join up the end to the start.
			ia[m_indexCursor] = m_vertexCursor-2;
			ia[m_indexCursor+1] = m_vertexCursor-1;
			ia[m_indexCursor+2] = startOfStripVertexCursor;
			ia[m_indexCursor+3] = startOfStripVertexCursor;
			ia[m_indexCursor+4] = m_vertexCursor-1;
			ia[m_indexCursor+5] = startOfStripVertexCursor + 1;
			m_indexCursor += 6;
			m_vertexCursor ++;
			startOfStripVertexCursor += 2;
		}
		m_vertexCursor ++;
	}
}

void vsMakeOutlineFromLineStrip2D( vsArray<vsVector2D> *result, vsVector2D *point, int count, float width, bool loop )
{
	size_t vertexCount = count * 2;
	float halfWidth = width * 0.5f;

	result->SetArraySize(vertexCount);
	int vertexCursor = 0;

	vsArray<vsVector2D>& va = *result;

	for ( int i = 0; i < count; i++ )
	{
		vsVector3D dirOfTravel;

		int midI = i;
		int preI = midI-1;
		int postI = midI+1;

		// float widthHere = m_width;

		if ( postI >= count )
		{
			if ( loop )
				postI = 0;
			else
				postI = count-1;
		}
		if ( preI < 0 )
		{
			if ( loop )
				preI = count-1;
			else
				preI = 0;
		}

		vsVector2D dirOfTravelPre = point[midI] - point[preI];
		vsVector2D dirOfTravelPost = point[postI] - point[midI];
		float distanceOfTravelPre = dirOfTravelPre.Length();
		float distanceOfTravelPost = dirOfTravelPost.Length();
		dirOfTravelPre.NormaliseSafe();
		dirOfTravelPost.NormaliseSafe();

		vsVector2D offsetPre( dirOfTravelPre.y, -dirOfTravelPre.x );
		vsVector2D offsetPost( dirOfTravelPost.y, -dirOfTravelPost.x );
		offsetPre.NormaliseSafe();
		offsetPost.NormaliseSafe();

		if ( preI == midI )
		{
			offsetPre = offsetPost;
			dirOfTravelPre = dirOfTravelPost;
		}
		else if ( midI == postI )
		{
			offsetPost = offsetPre;
			dirOfTravelPost = dirOfTravelPre;
		}

		vsVector3D vertexPosition;
		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D insidePre = point[preI] - (offsetPre * halfWidth);
			vsVector3D insidePost = point[postI] - (offsetPost * halfWidth);

			vsSqDistanceBetweenLineSegments( insidePre,
					insidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * halfWidth),
					insidePost,
					insidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * halfWidth),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = point[midI] - offsetPre * halfWidth;
		}

		va[vertexCursor] = vertexPosition;

		if ( offsetPre != offsetPost )
		{
			vsVector3D closestPre, closestPost;
			vsVector3D outsidePre = point[preI] + (offsetPre * halfWidth);
			vsVector3D outsidePost = point[postI] + (offsetPost * halfWidth);

			vsSqDistanceBetweenLineSegments( outsidePre,
					outsidePre + dirOfTravelPre * (distanceOfTravelPre + 3.f * halfWidth),
					outsidePost,
					outsidePost - dirOfTravelPost * (distanceOfTravelPost + 3.f * halfWidth),
					&closestPre, &closestPost );

			vertexPosition = vsInterpolate(0.5f, closestPre, closestPost);
		}
		else
		{
			vertexPosition = point[midI] + offsetPre * halfWidth;
		}

		va[vertexCursor+1] = vertexPosition;
		vertexCursor += 2;
	}
}

