/*
 *  VS_Renderer_OpenGL3.cpp
 *  VectorStorm
 *
 *  Created by Trevor Powell on 17/03/07.
 *  Copyright 2007 Trevor Powell. All rights reserved.
 *
 */


#include "VS_Renderer_OpenGL3.h"

#include "VS_Camera.h"
#include "VS_Debug.h"
#include "VS_DisplayList.h"
#include "VS_Image.h"
#include "VS_MaterialInternal.h"
#include "VS_Matrix.h"
#include "VS_RenderBuffer.h"
#include "VS_RenderTarget.h"
#include "VS_Screen.h"
#include "VS_Shader.h"
#include "VS_ShaderSuite.h"
#include "VS_System.h"
#include "VS_Texture.h"
#include "VS_TextureInternal.h"

#include "VS_OpenGL.h"

#include "VS_TimerSystem.h"


#if TARGET_OS_IPHONE

#define glClearDepth( a ) glClearDepthf( a )
#define glFogi( a, b ) glFogx( a, b )
#define glTexParameteri( a, b, c ) glTexParameterx( a, b, c )

#endif

static SDL_Window *m_sdlWindow = NULL;
static SDL_GLContext m_sdlGlContext;

void vsRenderDebug( const vsString &message )
{
	vsLog("%s", message.c_str());
}

static void printAttributes ()
{
#if !TARGET_OS_IPHONE
    // Print out attributes of the context we created
    int nAttr;
    int i;

	vsLog("OpenGL Context:");
	vsLog("  Vendor: %s", glGetString(GL_VENDOR));
	vsLog("  Renderer: %s", glGetString(GL_RENDERER));
	vsLog("  Version: %s", glGetString(GL_VERSION));
	if ( glGetString(GL_SHADING_LANGUAGE_VERSION) )
	{
		vsLog("  Shading Language Version:  %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
	}
	else
	{
		vsLog("  Shader Langugage Version:  None");
	}

    SDL_GLattr  attr[] = { SDL_GL_RED_SIZE, SDL_GL_BLUE_SIZE, SDL_GL_GREEN_SIZE,
	SDL_GL_ALPHA_SIZE, SDL_GL_BUFFER_SIZE, SDL_GL_DEPTH_SIZE };

    const char *desc[] = { "Red size: %d bits", "Blue size: %d bits", "Green size: %d bits",
		"Alpha size: %d bits", "Color buffer size: %d bits", "Depth buffer size: %d bits" };

    nAttr = sizeof(attr) / sizeof(int);

    for (i = 0; i < nAttr; i++)
	{
        int value;
        SDL_GL_GetAttribute (attr[i], &value);
        vsLog(vsFormatString(desc[i], value));
    }
#endif // TARGET_OS_IPHONE
}



//static bool s_vertexBuffersSupported = false;

vsRenderer_OpenGL3::vsRenderer_OpenGL3(int width, int height, int depth, int flags, int bufferCount):
	vsRenderer(width, height, depth, flags),
	m_window(NULL),
	m_scene(NULL),
	m_bufferCount(bufferCount)
{
	const char* c_capabilityString[vsRenderer_OpenGL3::CAP_MAX] =
	{
		"GL_ARB_framebuffer_object",
		"GL_EXT_framebuffer_object",
		"GL_EXT_framebuffer_multisample",
		"GL_EXT_framebuffer_blit"
	};

	int displayCount = SDL_GetNumVideoDisplays();
	if (displayCount < 1)
	{
		fprintf(stderr, "%s\nExiting...\n", SDL_GetError());
		SDL_Quit();
		exit(1);
	}
	vsLog("Found %d displays:", displayCount);
	for(int i = 0; i < displayCount; i++)
	{
		SDL_Rect bounds;
		if(SDL_GetDisplayBounds(i, &bounds) == 0)
			vsLog("Display #%d %s (%dx%d)\n", i, SDL_GetDisplayName(i), bounds.w , bounds.h );
	}
#ifdef _DEBUG
	//SDL_DisplayMode mode;
	//if ( displayCount > 1 )
	//{
	//	flags |= Flag_Fullscreen;
	//	SDL_Rect bounds;
	//	SDL_GetDesktopDisplayMode(1, &mode);
	//	if ( SDL_GetDisplayBounds(1, &bounds) == 0 )
	//	{
	//		width = bounds.w;
	//		height = bounds.h;
	//	}
	//}
#endif

	m_viewportWidth = m_width = width;
	m_viewportHeight = m_height = height;
	m_invalidateMaterial = false;

#if !TARGET_OS_IPHONE
	//const SDL_VideoInfo *videoInfo = SDL_GetVideoInfo();
	int videoFlags;

	videoFlags = SDL_WINDOW_OPENGL;
#ifdef HIGHDPI_SUPPORTED
	videoFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif

	if ( flags & Flag_Fullscreen )
		videoFlags |= SDL_WINDOW_FULLSCREEN;
	else if ( flags & Flag_Resizable )
		videoFlags |= SDL_WINDOW_RESIZABLE;

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	// SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 1 );

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	// no depth buffer on our output target -- we don't render into it directly.
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef _DEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif // _DEBUG

	m_sdlWindow = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, videoFlags);
	// SDL_SetWindowMinimumSize(m_sdlWindow, 1024, 768);

	if ( !m_sdlWindow ){
		fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
				width, height, depth, SDL_GetError() );
		exit(1);
	}
#ifdef HIGHDPI_SUPPORTED
	SDL_GL_GetDrawableSize(m_sdlWindow, &m_widthPixels, &m_heightPixels);
#else
	m_widthPixels = width;
	m_heightPixels = height;
#endif
	m_viewportWidthPixels = m_widthPixels;
	m_viewportHeightPixels = m_heightPixels;
	if ( m_viewportWidthPixels != m_widthPixels ||
			m_viewportHeightPixels != m_heightPixels )
	{
		vsLog("High DPI Rendering enabled");
		vsLog("High DPI backing store is: %dx%d", m_viewportWidthPixels, m_viewportHeightPixels);
	}

	m_sdlGlContext = SDL_GL_CreateContext(m_sdlWindow);
	if ( !m_sdlGlContext )
	{
		vsLog("Failed to create OpenGL context??");
		exit(1);
	}

	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	printAttributes();
	vsAssert(GLEW_OK == err, vsFormatString("GLEW error: %s", glewGetErrorString(err)).c_str());
	if ( !GL_VERSION_2_1 )
	{
		vsLog("No support for GL 2.1 -- cannot run.");
		exit(1);	// TODO:  What can I do in this situation?
	}
	else
	{
		for ( int i = 0; i < CAP_MAX; i++ )
		{
			m_capabilities.supported[i] = glewIsSupported( c_capabilityString[i] );
			vsLog("%s: %s", c_capabilityString[i], m_capabilities.supported[i] ?
					"supported" : "not supported");
		}

		// check caps to make sure that we can run.
		bool canRun = true;
		if ( !m_capabilities.supported[CAP_ARB_framebuffer_object] &&
				( !m_capabilities.supported[CAP_EXT_framebuffer_object] ||
				  !m_capabilities.supported[CAP_EXT_framebuffer_blit] ||
				  !m_capabilities.supported[CAP_EXT_framebuffer_multisample] )
		   )
		{
			canRun = false;
		}

		if ( !canRun )
		{
			vsLog("Insufficient extensions to run.");
			exit(1);
		}
	}

	m_antialias = (glRenderbufferStorageMultisampleEXT != NULL);
	// m_antialias = false;

	if ( SDL_GL_SetSwapInterval(flags & Flag_VSync ? 1 : 0) == -1 )
	{
		vsLog("Couldn't set vsync");
	}

	if ( glDebugMessageCallback )
	{
		vsLog("DebugMessageCallback:  SUPPORTED");
	}

	vsLog( "VSync: %s", SDL_GL_GetSwapInterval() > 0 ? "ENABLED" : "DISABLED" );

	int MaxVertexTextureImageUnits;
	glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &MaxVertexTextureImageUnits);
	int MaxCombinedTextureImageUnits;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &MaxCombinedTextureImageUnits);

	vsLog( "TextureUnits: %d from vertex shader, %d total", MaxVertexTextureImageUnits, MaxCombinedTextureImageUnits );

	int val;
	SDL_GL_GetAttribute( SDL_GL_MULTISAMPLEBUFFERS, &val );
	if ( val )
	{
		SDL_GL_GetAttribute( SDL_GL_MULTISAMPLESAMPLES, &val );
		vsLog("Using %d-sample multisampling.", val);
	}
	SDL_GL_GetAttribute( SDL_GL_DOUBLEBUFFER, &val );
	if ( !val )
		vsLog("WARNING:  Failed to initialise double-buffering");

	SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &val );
	if ( !val )
		vsLog("WARNING:  Failed to get stencil buffer bits");

#endif // !TARGET_OS_IPHONE

	CheckGLError("Initialising OpenGL rendering");
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
#if !TARGET_OS_IPHONE
	glClearDepth( 1.0f );  // arbitrary large value
#endif // !TARGET_OS_IPHONE
	CheckGLError("Initialising OpenGL rendering");

	glBlendFunc(GL_SRC_ALPHA,GL_ONE);							// Set The Blending Function For Additive
	glEnable(GL_BLEND);											// Enable Blending
	CheckGLError("Initialising OpenGL rendering");

	m_state.SetBool( vsRendererState::Bool_DepthTest, true );
	glDepthFunc( GL_LEQUAL );
	CheckGLError("Initialising OpenGL rendering");

	glViewport( 0, 0, (GLsizei)m_widthPixels, (GLsizei)m_heightPixels );
	CheckGLError("Initialising OpenGL rendering");

	m_defaultShaderSuite.InitShaders("default_v.glsl", "default_f.glsl");
	CheckGLError("Initialising OpenGL rendering");

	// TEMP VAO IMPLEMENTATION
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	CheckGLError("Initialising OpenGL rendering");

	Resize();

	CheckGLError("Initialising OpenGL rendering");

	// now set our OpenGL state to our expected defaults.
	m_state.Force();

}

vsRenderer_OpenGL3::~vsRenderer_OpenGL3()
{
	vsDelete(m_window);
	vsDelete(m_scene);
	CheckGLError("Initialising OpenGL rendering");
	SDL_GL_DeleteContext( m_sdlGlContext );
	SDL_DestroyWindow( m_sdlWindow );
	m_sdlWindow = NULL;
}

void
vsRenderer_OpenGL3::Resize()
{
	CheckGLError("Resizing");
	vsDelete( m_window );
	vsDelete( m_scene );

	// Create Window Surface
	vsSurface::Settings settings;
	settings.depth = 0;
	settings.width = GetWidthPixels();
	settings.height = GetHeightPixels();
	m_window = new vsRenderTarget( vsRenderTarget::Type_Window, settings );

	// Create 3D Scene Surface
	// we want to be big enough to hold our full m_window resolution, and set our viewport to match the window.

	settings.width = GetWidthPixels();
	settings.height = GetHeightPixels();
	settings.depth = true;
	settings.linear = true;
	settings.mipMaps = false;
	settings.stencil = true;
	settings.buffers = 2;	// one for opaque color, one for glow color.  TODO:  Set only one buffer if we're not doing a glow pass!

	if ( m_antialias )
	{
		m_scene = new vsRenderTarget( vsRenderTarget::Type_Multisample, settings );
	}
	else
	{
		m_scene = new vsRenderTarget( vsRenderTarget::Type_Texture, settings );
	}
	SetViewportWidthPixels( m_scene->GetViewportWidth() );
	SetViewportHeightPixels( m_scene->GetViewportHeight() );
	CheckGLError("Resizing");
}

bool
vsRenderer_OpenGL3::CheckVideoMode()
{
	CheckGLError("CheckVideoMode");
#ifdef HIGHDPI_SUPPORTED
	int nowWidthPixels, nowHeightPixels;
	SDL_GL_GetDrawableSize(m_sdlWindow, &nowWidthPixels, &nowHeightPixels);
	if ( nowWidthPixels != m_widthPixels || nowHeightPixels != m_heightPixels )
	{
		UpdateVideoMode( m_width, m_height, true, false, m_bufferCount );
		return true;
	}
#endif
	return false;
}

void
vsRenderer_OpenGL3::UpdateVideoMode(int width, int height, int depth, bool fullscreen, int bufferCount)
{
	UNUSED(depth);
	UNUSED(fullscreen);
	CheckGLError("UpdateVideoMode");
	//vsAssert(0, "Not yet implemented");
	m_width = m_viewportWidth = width;
	m_height = m_viewportHeight = height;
	m_bufferCount = bufferCount;
#ifdef HIGHDPI_SUPPORTED
	SDL_GL_GetDrawableSize(m_sdlWindow, &m_widthPixels, &m_heightPixels);
#else
	m_widthPixels = width;
	m_heightPixels = height;
#endif
	Resize();
	CheckGLError("UpdateVideoMode");
}

void
vsRenderer_OpenGL3::PreRender(const Settings &s)
{
	CheckGLError("PreRender");
	glViewport( 0, 0, (GLsizei)m_widthPixels, (GLsizei)m_heightPixels );

	m_state.SetBool( vsRendererState::Bool_DepthMask, true );
	m_currentMaterial = NULL;
	m_currentShader = NULL;

	m_scene->Bind();
	m_currentRenderTarget = m_scene;
	m_currentColor = c_white;

	if ( m_antialias )
	{
		m_state.SetBool( vsRendererState::Bool_Multisample, true );
	}
	glBlendFunc( GL_SRC_ALPHA, GL_ONE );

	m_state.SetBool( vsRendererState::Bool_DepthMask, true );
	m_state.Flush();

	glClearColor(0.0f,0.f,0.f,0.f);
	glClearDepth(1.f);
	glClearStencil(0);
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	m_state.SetBool( vsRendererState::Bool_StencilTest, true );
	m_state.Flush();
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

	// our baseline size is 1024x768.  We want single-pixel lines at that size.
	// float lineScaleFactor = vsMax(2.0f,m_heightPixels / 384.f);
	// glLineWidth( lineScaleFactor );

	CheckGLError("PreRender");
}

void
vsRenderer_OpenGL3::PostRender()
{
	CheckGLError("PostRender");

	vsTimerSystem::Instance()->EndRenderTime();
#if !TARGET_OS_IPHONE
	SDL_GL_SwapWindow(m_sdlWindow);
#endif
	vsTimerSystem::Instance()->EndGPUTime();

	CheckGLError("PostRender");
}

void
vsRenderer_OpenGL3::RenderDisplayList( vsDisplayList *list )
{
	CheckGLError("RenderDisplayList");

	m_currentMaterial = NULL;
	m_currentShader = NULL;
	RawRenderDisplayList(list);

	CheckGLError("RenderDisplayList");
}

void
vsRenderer_OpenGL3::FlushRenderState()
{
	CheckGLError("FlushRenderState");
	m_state.Flush();
	if ( m_currentShader )
	{
		glUseProgram( m_currentShader->GetShaderId() );
		m_currentShader->Prepare();
		m_currentShader->SetAlphaRef( m_currentMaterial->m_alphaRef );
		m_currentShader->SetGlow( m_currentMaterial->m_glow );
		m_currentShader->SetColor( m_currentColor );
		m_currentShader->SetFog( m_currentMaterial->m_fog, m_currentFogColor, m_currentFogDensity );
		m_currentShader->SetTextures( m_currentMaterial->m_texture );
		if ( m_currentLocalToWorldBuffer )
			m_currentShader->SetLocalToWorld( m_currentLocalToWorldBuffer );
		else if ( m_currentLocalToWorldCount > 0 )
			m_currentShader->SetLocalToWorld( m_currentLocalToWorld, m_currentLocalToWorldCount );
		else
			m_currentShader->SetLocalToWorld( &m_transformStack[0], 1 );
		m_currentShader->SetWorldToView( m_currentWorldToView );
		m_currentShader->SetViewToProjection( m_currentViewToProjection );
		for ( int i = 0; i < MAX_LIGHTS; i++ )
		{
			vsVector3D halfVector;
			m_currentShader->SetLight( i, m_lightStatus[i].ambient, m_lightStatus[i].diffuse,
					m_lightStatus[i].specular, m_lightStatus[i].position,
					halfVector);
		}
	}
	else
	{
		glUseProgram( 0 );
	}
	CheckGLError("FlushRenderState");
}

void
vsRenderer_OpenGL3::RawRenderDisplayList( vsDisplayList *list )
{
	m_currentCameraPosition = vsVector3D::Zero;

	vsDisplayList::op *op = list->PopOp();
	//vsVector3D	cursorPos;
	//vsColor		cursorColor;
	//vsColor		currentColor(-1,-1,-1,0);
	//vsColor		nextColor;
	//vsOverlay	currentOverlay;
	//bool		colorSet = false;
	//bool		cursorSet = false;
	//bool		inLineStrip = false;
	//bool		inPointList = false;
	//bool		recalculateColor = false;	// if true, recalc color even if we don't think it's changed

	//bool		usingVertexArray = false;
	m_usingNormalArray = false;
	m_usingTexelArray = false;
	m_lightCount = 0;

	m_currentLocalToWorld = NULL;
	m_currentLocalToWorldCount = 0;
	m_currentLocalToWorldBuffer = NULL;
	m_currentVertexArray = NULL;
	m_currentVertexBuffer = NULL;
	m_currentTexelArray = NULL;
	m_currentTexelBuffer = NULL;
	m_currentColorArray = NULL;
	m_currentColorBuffer = NULL;
	m_currentTransformStackLevel = 0;
	m_currentVertexArrayCount = 0;

	m_inOverlay = false;

	m_transformStack[m_currentTransformStackLevel] = vsMatrix4x4::Identity;

	while(op)
	{
// #define LOG_OPS
#ifdef LOG_OPS
		vsLog("%s", vsDisplayList::GetOpCodeString(op->type).c_str());
#endif // LOG_OPS
		switch( op->type )
		{
			case vsDisplayList::OpCode_SetColor:
				{
					m_currentColor = op->data.GetColor();
					break;
				}
			case vsDisplayList::OpCode_SetMaterial:
				{
					vsMaterialInternal *material = (vsMaterialInternal *)op->data.p;
					SetMaterial( material );
					break;
				}
			case vsDisplayList::OpCode_SetRenderTarget:
				{
					vsRenderTarget *target = (vsRenderTarget*)op->data.p;
					SetRenderTarget(target);
					break;
				}
			case vsDisplayList::OpCode_ResolveRenderTarget:
				{
					vsRenderTarget *target = (vsRenderTarget*)op->data.p;
					if ( target )
						target->Resolve();
					break;
				}
			case vsDisplayList::OpCode_BlitRenderTarget:
				{
					vsRenderTarget *from = (vsRenderTarget*)op->data.p;
					vsRenderTarget *to = (vsRenderTarget*)op->data.p2;
					from->BlitTo(to);
					break;
				}
			case vsDisplayList::OpCode_PushTransform:
				{
					vsTransform2D t = op->data.GetTransform();

					vsMatrix4x4 localToWorld = m_transformStack[m_currentTransformStackLevel] * t.GetMatrix();
					m_transformStack[++m_currentTransformStackLevel] = localToWorld;

					m_currentLocalToWorld = &m_transformStack[m_currentTransformStackLevel];
					m_currentLocalToWorldCount = 1;
					m_currentLocalToWorldBuffer = NULL;
					break;
				}
			case vsDisplayList::OpCode_PushTranslation:
				{
					vsVector3D &v = op->data.vector;

					vsMatrix4x4 m;
					m.SetTranslation(v);
					vsMatrix4x4 localToWorld = m_transformStack[m_currentTransformStackLevel] * m;
					m_transformStack[++m_currentTransformStackLevel] = localToWorld;
					m_currentLocalToWorld = &m_transformStack[m_currentTransformStackLevel];
					m_currentLocalToWorldCount = 1;
					m_currentLocalToWorldBuffer = NULL;
					break;
				}
			case vsDisplayList::OpCode_PushMatrix4x4:
				{
					vsMatrix4x4 m = op->data.GetMatrix4x4();
					vsMatrix4x4 localToWorld = m_transformStack[m_currentTransformStackLevel] * m;
					m_transformStack[++m_currentTransformStackLevel] = localToWorld;
					m_currentLocalToWorld = &m_transformStack[m_currentTransformStackLevel];
					m_currentLocalToWorldCount = 1;
					m_currentLocalToWorldBuffer = NULL;
					break;
				}
			case vsDisplayList::OpCode_SetMatrices4x4:
				{
					vsMatrix4x4 *m = (vsMatrix4x4*)op->data.p;
					int count = op->data.i;
					m_transformStack[++m_currentTransformStackLevel] = m[0];
					m_currentLocalToWorld = m;
					m_currentLocalToWorldCount = count;
					m_currentLocalToWorldBuffer = NULL;
					break;
				}
			case vsDisplayList::OpCode_SetMatrices4x4Buffer:
				{
					vsRenderBuffer *b = (vsRenderBuffer*)op->data.p;
					m_transformStack[++m_currentTransformStackLevel] = vsMatrix4x4::Identity;
					m_currentLocalToWorld = NULL;
					m_currentLocalToWorldCount = 0;
					m_currentLocalToWorldBuffer = b;
					break;
				}
			case vsDisplayList::OpCode_SnapMatrix:
				{
					vsMatrix4x4 m = m_transformStack[m_currentTransformStackLevel];
					vsVector4D &t = m.w;
					t.x = (int)t.x;
					t.y = (int)t.y;
					t.z = (int)t.z;
					m_transformStack[++m_currentTransformStackLevel] = m;
					m_currentLocalToWorld = &m_transformStack[m_currentTransformStackLevel];
					m_currentLocalToWorldCount = 1;
					m_currentLocalToWorldBuffer = NULL;
					break;
				}
			case vsDisplayList::OpCode_SetWorldToViewMatrix4x4:
				{
					m_currentWorldToView = op->data.GetMatrix4x4();
					break;
				}
			case vsDisplayList::OpCode_PopTransform:
				{
					vsAssert(m_currentTransformStackLevel > 0, "Renderer transform stack underflow??");
					m_currentTransformStackLevel--;
					m_currentLocalToWorld = &m_transformStack[m_currentTransformStackLevel];
					m_currentLocalToWorldCount = 1;
					break;
				}
			case vsDisplayList::OpCode_SetCameraTransform:
				{
					break;
				}
			case vsDisplayList::OpCode_SetProjectionMatrix4x4:
				{
					vsMatrix4x4 &m = op->data.GetMatrix4x4();
					m_currentViewToProjection = m;
					break;
				}
			case vsDisplayList::OpCode_VertexArray:
				{
					vsRenderBuffer::BindVertexArray( &m_state, op->data.p, op->data.i );
					m_state.SetBool( vsRendererState::ClientBool_VertexArray, true );
					break;
				}
			case vsDisplayList::OpCode_VertexBuffer:
				{
					m_currentVertexBuffer = (vsRenderBuffer *)op->data.p;
					m_currentVertexBuffer->BindVertexBuffer( &m_state );
					break;
				}
			case vsDisplayList::OpCode_NormalArray:
				{
					vsRenderBuffer::BindNormalArray( &m_state, op->data.p, op->data.i );
					m_state.SetBool( vsRendererState::ClientBool_NormalArray, true );
					break;
				}
			case vsDisplayList::OpCode_NormalBuffer:
				{
					m_currentNormalBuffer = (vsRenderBuffer *)op->data.p;
					m_currentNormalBuffer->BindNormalBuffer( &m_state );
					m_state.SetBool( vsRendererState::ClientBool_NormalArray, true );
					break;
				}
			case vsDisplayList::OpCode_ClearVertexArray:
				{
					m_currentVertexBuffer = NULL;
					m_state.SetBool( vsRendererState::ClientBool_VertexArray, false );
					m_currentVertexArray = NULL;
					m_currentVertexArrayCount = 0;
					break;
				}
			case vsDisplayList::OpCode_ClearNormalArray:
				{
					m_currentNormalBuffer = NULL;
					m_currentNormalArray = NULL;
					m_currentNormalArrayCount = 0;
					m_state.SetBool( vsRendererState::ClientBool_NormalArray, false );
					break;
				}
			case vsDisplayList::OpCode_TexelArray:
				{
					vsRenderBuffer::BindTexelArray( &m_state, op->data.p, op->data.i );
					m_state.SetBool( vsRendererState::ClientBool_TextureCoordinateArray, true );
					break;
				}
			case vsDisplayList::OpCode_TexelBuffer:
				{
					m_currentTexelBuffer = (vsRenderBuffer *)op->data.p;
					m_currentTexelBuffer->BindTexelBuffer( &m_state );
					break;
				}
			case vsDisplayList::OpCode_ClearTexelArray:
				{
					m_state.SetBool( vsRendererState::ClientBool_TextureCoordinateArray, false );
					break;
				}
			case vsDisplayList::OpCode_ColorArray:
				{
					vsRenderBuffer::BindColorArray( &m_state, op->data.p, op->data.i );
					m_state.SetBool( vsRendererState::ClientBool_ColorArray, true );
					break;
				}
			case vsDisplayList::OpCode_ColorBuffer:
				{
					m_currentColorBuffer = (vsRenderBuffer *)op->data.p;
					m_currentColorBuffer->BindColorBuffer( &m_state );
					break;
				}
			case vsDisplayList::OpCode_ClearColorArray:
				{
					if ( m_currentColorBuffer )
					{
						//m_currentColorBuffer->UnbindColorBuffer();
						m_currentColorBuffer = NULL;
					}
					m_currentColorArray = NULL;
					m_currentColorArrayCount = 0;
					m_state.SetBool( vsRendererState::ClientBool_ColorArray, false );
					break;
				}
			case vsDisplayList::OpCode_ClearArrays:
				{
					m_currentColorArray = NULL;
					m_currentColorBuffer = NULL;
					m_currentColorArrayCount = 0;
					m_state.SetBool( vsRendererState::ClientBool_ColorArray, false );

					m_currentTexelBuffer = NULL;
					m_currentTexelArray = NULL;
					m_currentTexelArrayCount = 0;
					m_state.SetBool( vsRendererState::ClientBool_TextureCoordinateArray, false );

					m_currentNormalBuffer = NULL;
					m_currentNormalArray = NULL;
					m_currentNormalArrayCount = 0;
					m_state.SetBool( vsRendererState::ClientBool_NormalArray, false );

					m_currentVertexBuffer = NULL;
					m_currentVertexArray = NULL;
					m_currentVertexArrayCount = 0;
					m_state.SetBool( vsRendererState::ClientBool_VertexArray, false );
					break;
				}
			case vsDisplayList::OpCode_BindBuffer:
				{
					vsRenderBuffer *buffer = (vsRenderBuffer *)op->data.p;
					buffer->Bind( &m_state );
					break;
				}
			case vsDisplayList::OpCode_UnbindBuffer:
				{
					vsRenderBuffer *buffer = (vsRenderBuffer *)op->data.p;
					buffer->Unbind( &m_state );
					break;
				}
			case vsDisplayList::OpCode_LineListArray:
				{
					FlushRenderState();
					vsRenderBuffer::DrawElementsImmediate( GL_LINES, op->data.p, op->data.GetUInt(), m_currentLocalToWorldCount );
					break;
				}
			case vsDisplayList::OpCode_LineStripArray:
				{
					FlushRenderState();
					vsRenderBuffer::DrawElementsImmediate( GL_LINE_STRIP, op->data.p, op->data.GetUInt(), m_currentLocalToWorldCount );
					break;
				}
			case vsDisplayList::OpCode_TriangleListArray:
				{
					FlushRenderState();

					vsRenderBuffer::DrawElementsImmediate( GL_TRIANGLES, op->data.p, op->data.GetUInt(), m_currentLocalToWorldCount );
					break;
				}
			case vsDisplayList::OpCode_TriangleStripArray:
				{
					FlushRenderState();
					vsRenderBuffer::DrawElementsImmediate( GL_TRIANGLE_STRIP, op->data.p, op->data.GetUInt(), m_currentLocalToWorldCount );
					break;
				}
			case vsDisplayList::OpCode_TriangleStripBuffer:
				{
					FlushRenderState();
					vsRenderBuffer *ib = (vsRenderBuffer *)op->data.p;
					ib->TriStripBuffer(m_currentLocalToWorldCount);
					break;
				}
			case vsDisplayList::OpCode_TriangleListBuffer:
				{
					FlushRenderState();
					vsRenderBuffer *ib = (vsRenderBuffer *)op->data.p;
					ib->TriListBuffer(m_currentLocalToWorldCount);
					break;
				}
			case vsDisplayList::OpCode_TriangleFanBuffer:
				{
					FlushRenderState();
					vsRenderBuffer *ib = (vsRenderBuffer *)op->data.p;
					ib->TriFanBuffer(m_currentLocalToWorldCount);
					break;
				}
			case vsDisplayList::OpCode_LineListBuffer:
				{
					FlushRenderState();
					vsRenderBuffer *ib = (vsRenderBuffer *)op->data.p;
					ib->LineListBuffer(m_currentLocalToWorldCount);
					break;
				}
			case vsDisplayList::OpCode_LineStripBuffer:
				{
					FlushRenderState();
					vsRenderBuffer *ib = (vsRenderBuffer *)op->data.p;
					ib->LineStripBuffer(m_currentLocalToWorldCount);
					break;
				}
			case vsDisplayList::OpCode_TriangleFanArray:
				{
					FlushRenderState();
					vsRenderBuffer::DrawElementsImmediate( GL_TRIANGLE_FAN, op->data.p, op->data.GetUInt(), m_currentLocalToWorldCount );
					// glDrawElements( GL_TRIANGLE_FAN, op->data.GetUInt(), GL_UNSIGNED_SHORT, op->data.p );
					break;
				}
			case vsDisplayList::OpCode_PointsArray:
				{
					FlushRenderState();
					vsRenderBuffer::DrawElementsImmediate( GL_POINTS, op->data.p, op->data.GetUInt(), m_currentLocalToWorldCount );
					// glDrawElements( GL_POINTS, op->data.GetUInt(), GL_UNSIGNED_SHORT, op->data.p );
					break;
				}
			case vsDisplayList::OpCode_Light:
				{
					if ( m_lightCount < MAX_LIGHTS - 1 )
					{
						vsLight &l = op->data.light;
						if ( l.m_type == vsLight::Type_Ambient )
						{
							m_lightStatus[m_lightCount].type = 1;
						}
						if ( l.m_type == vsLight::Type_Directional )
						{
							m_lightStatus[m_lightCount].type = 2;
							m_lightStatus[m_lightCount].position = l.m_direction;
						}
						if ( l.m_type == vsLight::Type_Point )
						{
							m_lightStatus[m_lightCount].type = 3;
							m_lightStatus[m_lightCount].position = l.m_position;
						}
						m_lightStatus[m_lightCount].ambient = l.m_ambient;
						m_lightStatus[m_lightCount].diffuse = l.m_color;
						m_lightStatus[m_lightCount].specular = c_white;

						m_lightCount++;
					}
					break;
				}
			case vsDisplayList::OpCode_ClearLights:
				{
					for ( int i = 0; i < m_lightCount; i++ )
					{
						m_lightStatus[i].type = 0;
					}
					m_lightCount = 0;
					break;
				}
			case vsDisplayList::OpCode_Fog:
				{
					vsColor fogColor = op->data.fog.GetColor();
					m_currentFogColor = fogColor;
					m_currentFogDensity = op->data.fog.GetDensity();
					break;
				}
			case vsDisplayList::OpCode_ClearFog:
				{
#if 0
					glFogf(GL_FOG_DENSITY, 0.f );
#endif // 0
					break;
				}
			case vsDisplayList::OpCode_FlatShading:
				{
					// glShadeModel( GL_FLAT );
					break;
				}
			case vsDisplayList::OpCode_SmoothShading:
				{
					// glShadeModel( GL_SMOOTH );
					break;
				}
			case vsDisplayList::OpCode_EnableStencil:
				{
					glStencilFunc(GL_EQUAL, 0x1, 0x1);
					break;
				}
			case vsDisplayList::OpCode_DisableStencil:
				{
					//m_state.SetBool( vsRendererState::Bool_StencilTest, false );
					glStencilFunc(GL_ALWAYS, 0x1, 0x1);
					break;
				}
			case vsDisplayList::OpCode_ClearStencil:
				{
					glClearStencil(0);
					glClear(GL_STENCIL_BUFFER_BIT);
					break;
				}
			case vsDisplayList::OpCode_EnableScissor:
				{
					m_state.SetBool( vsRendererState::Bool_ScissorTest, true );
					const vsBox2D& box = op->data.box2D;
					glScissor( box.GetMin().x * m_viewportWidthPixels,
							box.GetMin().y * m_viewportHeightPixels,
							box.Width() * m_viewportWidthPixels,
							box.Height() * m_viewportHeightPixels );
					break;
				}
			case vsDisplayList::OpCode_DisableScissor:
				{
					m_state.SetBool( vsRendererState::Bool_ScissorTest, false );
					break;
				}
			case vsDisplayList::OpCode_SetViewport:
				{
					const vsBox2D& box = op->data.box2D;
					glViewport( box.GetMin().x * m_viewportWidthPixels,
							box.GetMin().y * m_viewportHeightPixels,
							box.Width() * m_viewportWidthPixels,
							box.Height() * m_viewportHeightPixels );
					//glViewport( box.GetMin().x,
					//box.GetMin().y,
					//box.Width(),
					//box.Height());
					break;
				}
			case vsDisplayList::OpCode_ClearViewport:
				{
					glViewport( 0, 0, (GLsizei)m_viewportWidthPixels, (GLsizei)m_viewportHeightPixels );
					break;
				}
			case vsDisplayList::OpCode_Debug:
				{
					vsRenderDebug( op->data.string );
					break;
				}
			default:
				vsAssert(false, "Unknown opcode type in display list!");	// error;  unknown opcode type in the display list!
		}
		CheckGLError("RenderOp");
		op = list->PopOp();
	}
}

void
vsRenderer_OpenGL3::SetMaterial(vsMaterialInternal *material)
{
	CheckGLError("Material");
	if ( !m_invalidateMaterial && (material == m_currentMaterial) )
	{
		return;
	}
	m_invalidateMaterial = true;
	m_currentMaterial = material;

	CheckGLError("glReadPixels");
	if ( m_currentSettings.writeColor )
	{
		glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	}
	else
	{
		glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
	}
	if ( m_currentSettings.writeDepth )
	{
		m_state.SetBool( vsRendererState::Bool_DepthMask, material->m_zWrite );
	}
	else
	{
		m_state.SetBool( vsRendererState::Bool_DepthMask, false );
	}
	CheckGLError("glReadPixels");

	switch ( material->m_stencil )
	{
		case StencilOp_None:
			glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
			break;
		case StencilOp_One:
			glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
			break;
		case StencilOp_Zero:
			glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
			break;
		case StencilOp_Inc:
			glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
			break;
		case StencilOp_Dec:
			glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
			break;
		case StencilOp_Invert:
			glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
			break;
		default:
			vsAssert(0, vsFormatString("Unhandled stencil type: %d", material->m_stencil));
	}
	CheckGLError("glReadPixels");

	vsAssert( m_currentMaterial != NULL, "In SetMaterial() with no material?" );
	m_currentShader = NULL;
	if ( material->m_shader )
	{
		m_currentShader = material->m_shader;
	}
	else
	{
		switch( material->m_drawMode )
		{
			case DrawMode_Add:
			case DrawMode_Subtract:
			case DrawMode_Normal:
			case DrawMode_Absolute:
				if ( material->m_texture[0] )
				{
					if ( m_currentSettings.shaderSuite && m_currentSettings.shaderSuite->GetShader(vsShaderSuite::NormalTex) )
						m_currentShader = m_currentSettings.shaderSuite->GetShader(vsShaderSuite::NormalTex);
					else
						m_currentShader = m_defaultShaderSuite.GetShader(vsShaderSuite::NormalTex);
				}
				else
				{
					if ( m_currentSettings.shaderSuite && m_currentSettings.shaderSuite->GetShader(vsShaderSuite::Normal) )
						m_currentShader = m_currentSettings.shaderSuite->GetShader(vsShaderSuite::Normal);
					else
						m_currentShader = m_defaultShaderSuite.GetShader(vsShaderSuite::Normal);
				}
				break;
			case DrawMode_Lit:
				if ( material->m_texture[0] )
				{
					if ( m_currentSettings.shaderSuite && m_currentSettings.shaderSuite->GetShader(vsShaderSuite::LitTex) )
						m_currentShader = m_currentSettings.shaderSuite->GetShader(vsShaderSuite::LitTex);
					else
						m_currentShader = m_defaultShaderSuite.GetShader(vsShaderSuite::LitTex);
				}
				else
				{
					if ( m_currentSettings.shaderSuite && m_currentSettings.shaderSuite->GetShader(vsShaderSuite::Lit) )
						m_currentShader = m_currentSettings.shaderSuite->GetShader(vsShaderSuite::Lit);
					else
						m_currentShader = m_defaultShaderSuite.GetShader(vsShaderSuite::Lit);
				}
				break;
			default:
				vsAssert(0,"Unknown drawmode??");
		}
	}

	/*static bool debugWireframe = false;
	  if ( debugWireframe )
	  {
	  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	  }
	  else
	  {
	  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	  }*/
	for ( int i = 0; i < MAX_TEXTURE_SLOTS; i++ )
	{
		vsTexture *t = material->GetTexture(i);
		glActiveTexture(GL_TEXTURE0 + i);
		if ( t )
		{
			// glEnable(GL_TEXTURE_2D);
			glBindTexture( GL_TEXTURE_2D, t->GetResource()->GetTexture() );

			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, material->m_clampU ? GL_CLAMP_TO_EDGE : GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, material->m_clampV ? GL_CLAMP_TO_EDGE : GL_REPEAT );
		}
		else
		{
			// glDisable(GL_TEXTURE_2D);
		}
	}
	CheckGLError("Material");
	glActiveTexture(GL_TEXTURE0);
	CheckGLError("Material");

	if ( material->m_alphaTest )
	{
		// m_state.SetFloat( vsRendererState::Float_AlphaThreshhold, material->m_alphaRef );
	}

	CheckGLError("Material");
	if ( material->m_zRead )
	{
		glDepthFunc( GL_LEQUAL );
	}
	CheckGLError("Material");

	if ( material->m_depthBiasConstant == 0.f && material->m_depthBiasFactor == 0.f )
	{
		m_state.SetBool( vsRendererState::Bool_PolygonOffsetFill, false );
	}
	else
	{
		m_state.SetBool( vsRendererState::Bool_PolygonOffsetFill, true );
		m_state.SetFloat2( vsRendererState::Float2_PolygonOffsetConstantAndFactor, material->m_depthBiasConstant, material->m_depthBiasFactor );
	}

	// m_state.SetBool( vsRendererState::Bool_AlphaTest, material->m_alphaTest );
	m_state.SetBool( vsRendererState::Bool_DepthTest, material->m_zRead );
	m_state.SetBool( vsRendererState::Bool_DepthMask, material->m_zWrite );
	// m_state.SetBool( vsRendererState::Bool_Fog, material->m_fog );

	CheckGLError("Material");
	if ( material->m_cullingType == Cull_None )
	{
		m_state.SetBool( vsRendererState::Bool_CullFace, false );
	}
	else
	{
		m_state.SetBool( vsRendererState::Bool_CullFace, true );

		bool cullingBack = (material->m_cullingType == Cull_Back);
		if ( m_currentSettings.invertCull )
		{
			cullingBack = !cullingBack;
		}

		if ( cullingBack )
		{
			m_state.SetInt( vsRendererState::Int_CullFace, GL_BACK );
		}
		else
		{
			m_state.SetInt( vsRendererState::Int_CullFace, GL_FRONT );
		}
	}
	CheckGLError("Material");

	m_state.SetBool( vsRendererState::Bool_Blend, material->m_blend );
	CheckGLError("Material");
	switch( material->m_drawMode )
	{
		case DrawMode_Add:
			{
#if !TARGET_OS_IPHONE
				glBlendEquation(GL_FUNC_ADD);
#endif
				glBlendFunc(GL_SRC_ALPHA,GL_ONE);					// additive
				// m_state.SetBool( vsRendererState::Bool_Lighting, false );
				// m_state.SetBool( vsRendererState::Bool_ColorMaterial, false );
	CheckGLError("Material");
				break;
			}
		case DrawMode_Subtract:
			{
#if !TARGET_OS_IPHONE
				glBlendEquation(GL_FUNC_SUBTRACT);
#endif
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				// m_state.SetBool( vsRendererState::Bool_Lighting, false );
				// m_state.SetBool( vsRendererState::Bool_ColorMaterial, false );
	CheckGLError("Material");
				break;
			}
		case DrawMode_Absolute:
			{
#if !TARGET_OS_IPHONE
				glBlendEquation(GL_FUNC_ADD);
#endif
				glBlendFunc(GL_ONE,GL_ZERO);	// absolute
				break;
			}
		case DrawMode_Normal:
			{
#if !TARGET_OS_IPHONE
				glBlendEquation(GL_FUNC_ADD);
				glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);	// opaque
#else
				glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);	// opaque
#endif
				// m_state.SetBool( vsRendererState::Bool_Lighting, false );
				// m_state.SetBool( vsRendererState::Bool_ColorMaterial, false );
	CheckGLError("Material");
				break;
			}
		case DrawMode_Lit:
			{
#if !TARGET_OS_IPHONE
				glBlendEquation(GL_FUNC_ADD);
				glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);	// opaque
#else
				glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);	// opaque
#endif
// 				m_state.SetBool( vsRendererState::Bool_Lighting, true );
// 				m_state.SetBool( vsRendererState::Bool_ColorMaterial, true );
// #if !TARGET_OS_IPHONE
// 				glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE ) ;
// #endif
// 				float materialAmbient[4] = {0.f, 0.f, 0.f, 1.f};
//
// 				glMaterialf( GL_FRONT_AND_BACK, GL_SHININESS, 50.f );
// 				glLightModelfv( GL_LIGHT_MODEL_AMBIENT, materialAmbient);
	CheckGLError("Material");
				break;
			}
		default:
			vsAssert(0, "Unknown draw mode requested!");
	}
	CheckGLError("Material");

	if ( material->m_hasColor )
	{
		m_currentColor = material->m_color;
		// const vsColor &c = material->m_color;
		// glColor4f( c.r, c.g, c.b, c.a );

		if ( material->m_drawMode == DrawMode_Lit )
		{
			// const vsColor &specColor = material->m_specularColor;
			// glMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, (float*)&specColor );
		}
	}
	else
	{
		m_currentColor = c_white;
	}
	CheckGLError("Material");
}

GLuint
vsRenderer_OpenGL3::Compile(const char *vert, const char *frag, int vLength, int fLength )
{
	GLchar buf[256];
	GLuint vertShader, fragShader, program;
	GLint success;

	CheckGLError("Compiling");
	GLint *vLengthPtr = NULL;
	GLint *fLengthPtr = NULL;

	if ( vLength > 0 )
		vLengthPtr = (GLint*)&vLength;
	if ( fLength > 0 )
		fLengthPtr = (GLint*)&fLength;

	vertShader = glCreateShader(GL_VERTEX_SHADER);
	CheckGLError("Compiling");
	glShaderSource(vertShader, 1, (const GLchar**) &vert, vLengthPtr);
	CheckGLError("Compiling");
	glCompileShader(vertShader);
	CheckGLError("Compiling");
	glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);
	CheckGLError("Compiling");
	if (!success)
	{
		glGetShaderInfoLog(vertShader, sizeof(buf), 0, buf);
		CheckGLError("Compiling");
		// vsLog("%s",vert);
		vsLog("%s",buf);
		vsAssert(success,"Unable to compile vertex shader.\n");
	}

	fragShader = glCreateShader(GL_FRAGMENT_SHADER);
	CheckGLError("Compiling");
	glShaderSource(fragShader, 1, (const GLchar**) &frag, fLengthPtr);
	CheckGLError("Compiling");
	glCompileShader(fragShader);
	CheckGLError("Compiling");
	glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
	CheckGLError("Compiling");
	if (!success)
	{
		glGetShaderInfoLog(fragShader, sizeof(buf), 0, buf);
	CheckGLError("Compiling");
		vsLog("%s",frag);
		vsLog(buf);
		vsAssert(success,"Unable to compile fragment shader.\n");
	}

	program = glCreateProgram();
	glAttachShader(program, vertShader);
	CheckGLError("Compiling");
	glAttachShader(program, fragShader);
	CheckGLError("Compiling");

	glBindAttribLocation(program, 0, "vertex");
	glBindAttribLocation(program, 1, "texcoord");
	glBindAttribLocation(program, 2, "normal");
	glBindAttribLocation(program, 3, "color");
	CheckGLError("Compiling");

	glLinkProgram(program);
	CheckGLError("Compiling");
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	CheckGLError("Compiling");
	if (!success)
	{
		glGetProgramInfoLog(program, sizeof(buf), 0, buf);
	CheckGLError("Compiling");
		vsLog(buf);
		vsAssert(success,"Unable to link shaders.\n");
	}
	glDetachShader(program,vertShader);
	CheckGLError("Compiling");
	glDetachShader(program,fragShader);
	CheckGLError("Compiling");
	glDeleteShader(vertShader);
	CheckGLError("Compiling");
	glDeleteShader(fragShader);
	CheckGLError("Compiling");

	return program;
}

void
vsRenderer_OpenGL3::DestroyShader(GLuint shader)
{
	glDeleteProgram(shader);
	CheckGLError("DeleteProgram");
}


vsImage *
vsRenderer_OpenGL3::Screenshot()
{
	const size_t bytesPerPixel = 3;	// RGB
	const size_t imageSizeInBytes = bytesPerPixel * size_t(m_widthPixels) * size_t(m_heightPixels);

	uint8_t* pixels = new uint8_t[imageSizeInBytes];

	// glReadPixels can align the first pixel in each row at 1-, 2-, 4- and 8-byte boundaries. We
	// have allocated the exact size needed for the image so we have to use 1-byte alignment
	// (otherwise glReadPixels would write out of bounds)
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, m_widthPixels, m_heightPixels, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	CheckGLError("glReadPixels");


	vsImage *image = new vsImage( m_widthPixels, m_heightPixels );

	for ( int y = 0; y < m_heightPixels; y++ )
	{
		int rowStart = y * m_widthPixels * bytesPerPixel;

		for ( int x = 0; x < m_widthPixels; x++ )
		{
			int rInd = rowStart + (x*bytesPerPixel);
			int gInd = rInd+1;
			int bInd = rInd+2;

			int rVal = pixels[rInd];
			int gVal = pixels[gInd];
			int bVal = pixels[bInd];

			image->Pixel(x,y).Set( rVal/255.f, gVal/255.f, bVal/255.f, 1.0f );
		}
	}

	vsDeleteArray( pixels );

	return image;
}

vsImage *
vsRenderer_OpenGL3::ScreenshotDepth()
{
#if !TARGET_OS_IPHONE
	int imageSize = sizeof(float) * m_widthPixels * m_heightPixels;

	float* pixels = new float[imageSize];

	// glReadPixels can align the first pixel in each row at 1-, 2-, 4- and 8-byte boundaries. We
	// have allocated the exact size needed for the image so we have to use 1-byte alignment
	// (otherwise glReadPixels would write out of bounds)
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, m_widthPixels, m_heightPixels, GL_DEPTH_COMPONENT, GL_FLOAT, pixels);
	CheckGLError("glReadPixels");


	vsImage *image = new vsImage( m_widthPixels, m_heightPixels );

	for ( int y = 0; y < m_heightPixels; y++ )
	{
		int rowStart = y * m_widthPixels;

		for ( int x = 0; x < m_widthPixels; x++ )
		{
			int ind = rowStart + x;

			float val = pixels[ind];

			image->Pixel(x,y).Set( val, val, val, 1.0f );
		}
	}

	vsDeleteArray( pixels );

	return image;
#else
	return NULL;
#endif
}

vsImage *
vsRenderer_OpenGL3::ScreenshotAlpha()
{
	int imageSize = sizeof(float) * m_widthPixels * m_heightPixels;

	float* pixels = new float[imageSize];

	// glReadPixels can align the first pixel in each row at 1-, 2-, 4- and 8-byte boundaries. We
	// have allocated the exact size needed for the image so we have to use 1-byte alignment
	// (otherwise glReadPixels would write out of bounds)
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, m_widthPixels, m_heightPixels, GL_ALPHA, GL_FLOAT, pixels);
	CheckGLError("glReadPixels");


	vsImage *image = new vsImage( m_widthPixels, m_heightPixels );

	for ( int y = 0; y < m_heightPixels; y++ )
	{
		int rowStart = y * m_widthPixels;

		for ( int x = 0; x < m_widthPixels; x++ )
		{
			int ind = rowStart + x;

			float val = pixels[ind];

			image->Pixel(x,y).Set( val, val, val, 1.0f );
		}
	}

	vsDeleteArray( pixels );

	return image;
}

void
vsRenderer_OpenGL3::SetRenderTarget( vsRenderTarget *target )
{
	// TODO:  The OpenGL code should be in HERE, not in the vsRenderTarget!
	if ( target )
	{
		target->Bind();
		m_currentRenderTarget = target;
		target->Clear();// this doesn't belong here.  Separate display list call?
	}
	else
	{
		m_scene->Bind();
		m_currentRenderTarget = m_scene;
		m_scene->Clear();
	}
}
