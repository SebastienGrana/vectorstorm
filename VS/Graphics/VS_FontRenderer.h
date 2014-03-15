/*
 *  VS_FontRenderer.h
 *  VectorStorm
 *
 *  Created by Trevor Powell on 15/03/2014
 *  Copyright 2014 Trevor Powell.  All rights reserved.
 *
 */

#ifndef VS_FONTRENDERER_H
#define VS_FONTRENDERER_H

#include "VS/Graphics/VS_Font.h"

class vsFontRenderer
{
	struct FragmentConstructor
	{
		vsRenderBuffer::PT *		ptArray;
		uint16_t *			tlArray;

		int					ptIndex;
		int					tlIndex;
	};

	// reference only -- do not deallocate!
	vsFont *m_font;

	float m_size;
	vsVector2D m_bounds;
	JustificationType m_justification;
	vsTransform3D m_transform;
	vsColor m_color;
	bool m_hasColor;

#define MAX_WRAPPED_LINES (50)
	vsString	m_wrappedLine[MAX_WRAPPED_LINES];
	int m_wrappedLineCount;

	void		WrapStringSizeTop(const vsString &string, float *size_out, float *top_out);
	void		WrapLine(const vsString &string, float size);
	vsFragment* CreateString_Fragment( FontContext context, const vsString& string );
	void		CreateString_InDisplayList( FontContext context, vsDisplayList *list, const vsString &string );
	void		AppendStringToArrays( vsFontRenderer::FragmentConstructor *constructor, const char* string, const vsVector2D &size, JustificationType type, const vsVector2D &offset);
	void		BuildDisplayListGeometryFromString( FontContext context, vsDisplayList * list, const char* string, float size, JustificationType type, const vsVector2D &offset);

public:
	vsFontRenderer( vsFont *font, JustificationType type = Justification_Left );
	vsFontRenderer( vsFont *font, float size, JustificationType type = Justification_Left );

	// useful for supporting HighDPI displays, where you may want to inform
	// VectorStorm to automatically set font sizes at 50% the exported font size.
	static void SetGlobalFontScale( float scale );

	void SetJustificationType(JustificationType type);
	void SetSize(float size);
	void SetMaxWidthAndHeight(float maxWidth, float maxHeight);
	void SetMaxWidthAndHeight(const vsVector2D &bounds);
	void SetMaxWidth(float maxWidth);

	void SetTransform( const vsTransform3D& transform );

	// If set, this color will be applied to the drawn text.  If not, whatever
	// color is in the sprite font will show through unmodified.
	void SetColor( const vsColor& color );
	void SetNoColor();

	// the 'fragment' approach is ideal for long-lived strings, as it produces a
	// single renderable chunk of geometry which can be drawn in a single draw call,
	// but which requires its own blob of data on the GPU.
	vsFragment* Fragment2D( const vsString& string );
	vsFragment* Fragment3D( const vsString& string );

	// the non-fragment approach avoids creating its own data on the GPU, and instead
	// uses a single shared blob of data which is owned by the font itself.  The
	// downside of this is that it requires a separate draw call for each glyph
	// in the text, with transforms inserted between them to offset each glyph into
	// its own position.  (This approach is necessary because a vsDisplayList
	// cannot own GPU data).  In general, the Fragment approach is preferred in
	// virtually all cases!
	vsDisplayList* DisplayList2D( const vsString& string );
	vsDisplayList* DisplayList3D( const vsString& string );

	void DisplayList2D( vsDisplayList *list, const vsString& string );
	void DisplayList3D( vsDisplayList *list, const vsString& string );
};


#endif // VS_FONTRENDERER_H

