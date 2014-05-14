/*
 *  VS_Renderer.h
 *  VectorStorm
 *
 *  Created by Trevor Powell on 17/03/07.
 *  Copyright 2007 Trevor Powell. All rights reserved.
 *
 */

#ifndef VS_RENDERER_OPENGL3_H
#define VS_RENDERER_OPENGL3_H

#include "VS/Graphics/VS_Texture.h"

#include "VS_Renderer.h"
#include "VS_Color.h"
#include "VS_Fog.h"
#include "VS_Material.h"
#include "VS_RendererState.h"
#include "VS_ShaderSuite.h"
#include "VS_Texture.h"
#include "Math/VS_Transform.h"
#include "VS_OpenGL.h"

class vsCamera2D;
class vsDisplayList;
class vsMaterialInternal;
class vsOverlay;
class vsRenderBuffer;
class vsTransform2D;
class vsVector2D;
struct SDL_Surface;

#define MAX_STACK_LEVEL (30)
#define CHECK_GL_ERRORS

class vsRenderer_OpenGL3: public vsRenderer, public vsSingleton<vsRenderer_OpenGL3>
{
private:

	enum
	{ // if I extend this, be sure to update the string array in the .cpp!
		CAP_ARB_framebuffer_object,
		CAP_EXT_framebuffer_object,
		CAP_EXT_framebuffer_multisample,
		CAP_EXT_framebuffer_blit,
		CAP_MAX
	};
	struct Capabilities
	{
		bool supported[CAP_MAX];
	};
	Capabilities		m_capabilities;
	vsShaderSuite		m_defaultShaderSuite;

	vsVector3D           m_currentCameraPosition;
	Settings             m_currentSettings;

	vsMatrix4x4			m_currentLocalToWorld;
	vsMatrix4x4			m_currentWorldToView;
	vsMatrix4x4			m_currentViewToProjection;

	vsColor				m_currentColor;
	vsColor				m_currentFogColor;
	float				m_currentFogDensity;

	vsRenderTarget *     m_window;
	vsRenderTarget *     m_scene;
	vsRenderTarget *     m_currentRenderTarget;

    vsRendererState      m_state;

	vsMaterialInternal * m_currentMaterial;
	vsShader *           m_currentShader;
	bool                 m_invalidateMaterial;

	vsVector3D *         m_currentVertexArray;
	vsVector3D *         m_currentNormalArray;
	vsVector2D *         m_currentTexelArray;
	vsColor *            m_currentColorArray;

	vsRenderBuffer *     m_currentVertexBuffer;
	vsRenderBuffer *     m_currentNormalBuffer;
	vsRenderBuffer *     m_currentTexelBuffer;
	vsRenderBuffer *     m_currentColorBuffer;

	vsMatrix4x4          m_transformStack[MAX_STACK_LEVEL];
	int                  m_currentTransformStackLevel;
	int                  m_currentVertexArrayCount;
	int                  m_currentNormalArrayCount;
	int                  m_currentTexelArrayCount;
	int                  m_currentColorArrayCount;
	int                  m_lightCount;
	bool                 m_inOverlay;
	bool                 m_usingNormalArray;
	bool                 m_usingTexelArray;
	bool                 m_antialias;
	uint32_t			m_vao;	// temporary -- for our global VAO.
	// VAOs should really be integrated more nicely somewhere, but for now,
	// we'll treat our rendering like OpenGL2 and just continually reconfigure
	// a single global Vertex Array Object..

	void				FlushRenderState();
	virtual void		SetMaterial(vsMaterialInternal *material);
	//virtual void		SetDrawMode(vsDrawMode mode);

	void Resize();

	static void DrawElementsImmediate( int type, void* buffer, int count );
	void	SetRenderTarget( vsRenderTarget *target );

public:

	enum
	{
		Flag_Fullscreen = BIT(0),
		Flag_VSync = BIT(1),
		Flag_Resizable = BIT(2)
	};
	vsRenderer_OpenGL3(int width, int height, int depth, int flags);
	virtual ~vsRenderer_OpenGL3();

	bool	CheckVideoMode();
	void	UpdateVideoMode(int width, int height, int depth, bool fullscreen);

	void	PreRender( const Settings &s );
	void	RenderDisplayList( vsDisplayList *list );
	void	RawRenderDisplayList( vsDisplayList *list );
	void	PostRender();

	virtual vsRenderTarget *GetMainRenderTarget() { return m_scene; }
	virtual vsRenderTarget *GetPresentTarget() { return m_window; }

	int		GetWidthPixels() const { return m_widthPixels; }
	int		GetHeightPixels() const { return m_heightPixels; }
	void	SetViewportWidthPixels( int width ) { m_widthPixels = width; }
	void	SetViewportHeightPixels( int height ) { m_heightPixels = height; }

	vsRendererState * GetState() { return &m_state; }

	const Settings& GetCurrentSettings() const { return m_currentSettings; }

	vsImage*	Screenshot();
	vsImage*	ScreenshotDepth();
	vsImage*	ScreenshotAlpha();

	static GLuint		Compile(const char *vert, const char *frag, int vertLength = 0, int fragLength = 0 );
	static void			DestroyShader(GLuint shader);

};

#endif // VS_RENDERER_OPENGL3_H

