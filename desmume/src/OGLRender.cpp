/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2015 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "OGLRender.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gfx3d.h"
#include "NDSSystem.h"
#include "texcache.h"


typedef struct
{
	unsigned int major;
	unsigned int minor;
	unsigned int revision;
} OGLVersion;

static OGLVersion _OGLDriverVersion = {0, 0, 0};
static OpenGLRenderer *_OGLRenderer = NULL;

// Lookup Tables
CACHE_ALIGN GLfloat material_8bit_to_float[256] = {0};
CACHE_ALIGN GLuint dsDepthToD24S8_LUT[32768] = {0};
const GLfloat divide5bitBy31_LUT[32]	= {0.0, 0.03225806451613, 0.06451612903226, 0.09677419354839,
									   0.1290322580645, 0.1612903225806, 0.1935483870968, 0.2258064516129,
									   0.258064516129, 0.2903225806452, 0.3225806451613, 0.3548387096774,
									   0.3870967741935, 0.4193548387097, 0.4516129032258, 0.4838709677419,
									   0.5161290322581, 0.5483870967742, 0.5806451612903, 0.6129032258065,
									   0.6451612903226, 0.6774193548387, 0.7096774193548, 0.741935483871,
									   0.7741935483871, 0.8064516129032, 0.8387096774194, 0.8709677419355,
									   0.9032258064516, 0.9354838709677, 0.9677419354839, 1.0};

static bool BEGINGL()
{
	if(oglrender_beginOpenGL) 
		return oglrender_beginOpenGL();
	else return true;
}

static void ENDGL()
{
	if(oglrender_endOpenGL) 
		oglrender_endOpenGL();
}

// Function Pointers
bool (*oglrender_init)() = NULL;
bool (*oglrender_beginOpenGL)() = NULL;
void (*oglrender_endOpenGL)() = NULL;
void (*OGLLoadEntryPoints_3_2_Func)() = NULL;
void (*OGLCreateRenderer_3_2_Func)(OpenGLRenderer **rendererPtr) = NULL;

//------------------------------------------------------------

// Textures
#if !defined(GLX_H)
OGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
OGLEXT(PFNGLACTIVETEXTUREARBPROC, glActiveTextureARB)
#endif

// Blending
OGLEXT(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) // Core in v1.4
OGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) // Core in v2.0

OGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC, glBlendFuncSeparateEXT)
OGLEXT(PFNGLBLENDEQUATIONSEPARATEEXTPROC, glBlendEquationSeparateEXT)

// Shaders
OGLEXT(PFNGLCREATESHADERPROC, glCreateShader) // Core in v2.0
OGLEXT(PFNGLSHADERSOURCEPROC, glShaderSource) // Core in v2.0
OGLEXT(PFNGLCOMPILESHADERPROC, glCompileShader) // Core in v2.0
OGLEXT(PFNGLCREATEPROGRAMPROC, glCreateProgram) // Core in v2.0
OGLEXT(PFNGLATTACHSHADERPROC, glAttachShader) // Core in v2.0
OGLEXT(PFNGLDETACHSHADERPROC, glDetachShader) // Core in v2.0
OGLEXT(PFNGLLINKPROGRAMPROC, glLinkProgram) // Core in v2.0
OGLEXT(PFNGLUSEPROGRAMPROC, glUseProgram) // Core in v2.0
OGLEXT(PFNGLGETSHADERIVPROC, glGetShaderiv) // Core in v2.0
OGLEXT(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) // Core in v2.0
OGLEXT(PFNGLDELETESHADERPROC, glDeleteShader) // Core in v2.0
OGLEXT(PFNGLDELETEPROGRAMPROC, glDeleteProgram) // Core in v2.0
OGLEXT(PFNGLGETPROGRAMIVPROC, glGetProgramiv) // Core in v2.0
OGLEXT(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) // Core in v2.0
OGLEXT(PFNGLVALIDATEPROGRAMPROC, glValidateProgram) // Core in v2.0
OGLEXT(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) // Core in v2.0
OGLEXT(PFNGLUNIFORM1IPROC, glUniform1i) // Core in v2.0
OGLEXT(PFNGLUNIFORM1IVPROC, glUniform1iv) // Core in v2.0
OGLEXT(PFNGLUNIFORM1FPROC, glUniform1f) // Core in v2.0
OGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
OGLEXT(PFNGLDRAWBUFFERSPROC, glDrawBuffers) // Core in v2.0
OGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) // Core in v2.0
OGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
OGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
OGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

#ifdef X432R_CUSTOMRENDERER_ENABLED
//OGLEXT(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData)	// Core in v1.5 (for PBO: v2.1)
#ifdef X432R_OPENGL_FOG_ENABLED
OGLEXT(PFNGLUNIFORM1FVPROC, glUniform1fv)				// Core in v2.0
#endif
#endif

// VAO
OGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)
OGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)
OGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)

// Buffer Objects
OGLEXT(PFNGLGENBUFFERSARBPROC, glGenBuffersARB)
OGLEXT(PFNGLDELETEBUFFERSARBPROC, glDeleteBuffersARB)
OGLEXT(PFNGLBINDBUFFERARBPROC, glBindBufferARB)
OGLEXT(PFNGLBUFFERDATAARBPROC, glBufferDataARB)
OGLEXT(PFNGLBUFFERSUBDATAARBPROC, glBufferSubDataARB)
OGLEXT(PFNGLMAPBUFFERARBPROC, glMapBufferARB)
OGLEXT(PFNGLUNMAPBUFFERARBPROC, glUnmapBufferARB)

OGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
OGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
OGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
OGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
OGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
OGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
OGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5

// FBO
OGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT)
OGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT)
OGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT)
OGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT)
OGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT)
OGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT)
OGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC, glBlitFramebufferEXT)
OGLEXT(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT)
OGLEXT(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT)
OGLEXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT)
OGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, glRenderbufferStorageMultisampleEXT)
OGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC, glDeleteRenderbuffersEXT)

static void OGLLoadEntryPoints_Legacy()
{
	// Textures
	#if !defined(GLX_H)
	INITOGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
	INITOGLEXT(PFNGLACTIVETEXTUREARBPROC, glActiveTextureARB)
	#endif

	// Blending
	INITOGLEXT(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) // Core in v1.4
	INITOGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) // Core in v2.0

	INITOGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC, glBlendFuncSeparateEXT)
	INITOGLEXT(PFNGLBLENDEQUATIONSEPARATEEXTPROC, glBlendEquationSeparateEXT)

	// Shaders
	INITOGLEXT(PFNGLCREATESHADERPROC, glCreateShader) // Core in v2.0
	INITOGLEXT(PFNGLSHADERSOURCEPROC, glShaderSource) // Core in v2.0
	INITOGLEXT(PFNGLCOMPILESHADERPROC, glCompileShader) // Core in v2.0
	INITOGLEXT(PFNGLCREATEPROGRAMPROC, glCreateProgram) // Core in v2.0
	INITOGLEXT(PFNGLATTACHSHADERPROC, glAttachShader) // Core in v2.0
	INITOGLEXT(PFNGLDETACHSHADERPROC, glDetachShader) // Core in v2.0
	INITOGLEXT(PFNGLLINKPROGRAMPROC, glLinkProgram) // Core in v2.0
	INITOGLEXT(PFNGLUSEPROGRAMPROC, glUseProgram) // Core in v2.0
	INITOGLEXT(PFNGLGETSHADERIVPROC, glGetShaderiv) // Core in v2.0
	INITOGLEXT(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) // Core in v2.0
	INITOGLEXT(PFNGLDELETESHADERPROC, glDeleteShader) // Core in v2.0
	INITOGLEXT(PFNGLDELETEPROGRAMPROC, glDeleteProgram) // Core in v2.0
	INITOGLEXT(PFNGLGETPROGRAMIVPROC, glGetProgramiv) // Core in v2.0
	INITOGLEXT(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) // Core in v2.0
	INITOGLEXT(PFNGLVALIDATEPROGRAMPROC, glValidateProgram) // Core in v2.0
	INITOGLEXT(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM1IPROC, glUniform1i) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM1IVPROC, glUniform1iv) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM1FPROC, glUniform1f) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
	INITOGLEXT(PFNGLDRAWBUFFERSPROC, glDrawBuffers) // Core in v2.0
	INITOGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) // Core in v2.0
	INITOGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
	INITOGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
	INITOGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

	#ifdef X432R_CUSTOMRENDERER_ENABLED
//	INITOGLEXT(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData)	// Core in v1.5 (for PBO: v2.1)
	#ifdef X432R_OPENGL_FOG_ENABLED
	INITOGLEXT(PFNGLUNIFORM1FVPROC, glUniform1fv)				// Core in v2.0
	#endif
	#endif

	// VAO
	INITOGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)
	INITOGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)
	INITOGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)

	// Buffer Objects
	INITOGLEXT(PFNGLGENBUFFERSARBPROC, glGenBuffersARB)
	INITOGLEXT(PFNGLDELETEBUFFERSARBPROC, glDeleteBuffersARB)
	INITOGLEXT(PFNGLBINDBUFFERARBPROC, glBindBufferARB)
	INITOGLEXT(PFNGLBUFFERDATAARBPROC, glBufferDataARB)
	INITOGLEXT(PFNGLBUFFERSUBDATAARBPROC, glBufferSubDataARB)
	INITOGLEXT(PFNGLMAPBUFFERARBPROC, glMapBufferARB)
	INITOGLEXT(PFNGLUNMAPBUFFERARBPROC, glUnmapBufferARB)

	INITOGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
	INITOGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
	INITOGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
	INITOGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
	INITOGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
	INITOGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
	INITOGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5

	// FBO
	INITOGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT)
	INITOGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT)
	INITOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT)
	INITOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT)
	INITOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT)
	INITOGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT)
	INITOGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC, glBlitFramebufferEXT)
	INITOGLEXT(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT)
	INITOGLEXT(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT)
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT)
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, glRenderbufferStorageMultisampleEXT)
	INITOGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC, glDeleteRenderbuffersEXT)
}

// Vertex Shader GLSL 1.00
static const char *vertexShader_100 = {"\
	attribute vec4 inPosition; \n\
	attribute vec2 inTexCoord0; \n\
	attribute vec3 inColor; \n\
	\n\
	uniform float polyAlpha; \n\
	uniform vec2 texScale; \n\
	\n\
	varying vec4 vtxPosition; \n\
	varying vec2 vtxTexCoord; \n\
	varying vec4 vtxColor; \n\
	\n\
	void main() \n\
	{ \n\
		mat2 texScaleMtx	= mat2(	vec2(texScale.x,        0.0), \n\
									vec2(       0.0, texScale.y)); \n\
		\n\
		vtxPosition = inPosition; \n\
		vtxTexCoord = texScaleMtx * inTexCoord0; \n\
		vtxColor = vec4(inColor * 4.0, polyAlpha); \n\
		\n\
		gl_Position = vtxPosition; \n\
	} \n\
"};

// Fragment Shader GLSL 1.00
static const char *fragmentShader_100 = {"\
	varying vec4 vtxPosition; \n\
	varying vec2 vtxTexCoord; \n\
	varying vec4 vtxColor; \n\
	\n\
	uniform sampler2D texMainRender; \n\
	uniform sampler1D texToonTable; \n\
	uniform int polyID; \n\
	uniform bool hasTexture; \n\
	uniform int polygonMode; \n\
	uniform int toonShadingMode; \n\
	uniform int oglWBuffer; \n\
	uniform bool enableAlphaTest; \n\
	uniform float alphaTestRef; \n\
	\n\
	void main() \n\
	{ \n\
		vec4 texColor = vec4(1.0, 1.0, 1.0, 1.0); \n\
		vec4 fragColor; \n\
		\n\
		if(hasTexture) \n\
		{ \n\
			texColor = texture2D(texMainRender, vtxTexCoord); \n\
		} \n\
		\n\
		fragColor = texColor; \n\
		\n\
		if(polygonMode == 0) \n\
		{ \n\
			fragColor = vtxColor * texColor; \n\
		} \n\
		else if(polygonMode == 1) \n\
		{ \n\
			if (texColor.a == 0.0 || !hasTexture) \n\
			{ \n\
				fragColor.rgb = vtxColor.rgb; \n\
			} \n\
			else if (texColor.a == 1.0) \n\
			{ \n\
				fragColor.rgb = texColor.rgb; \n\
			} \n\
			else \n\
			{ \n\
				fragColor.rgb = texColor.rgb * (1.0-texColor.a) + vtxColor.rgb * texColor.a;\n\
			} \n\
			\n\
			fragColor.a = vtxColor.a; \n\
		} \n\
		else if(polygonMode == 2) \n\
		{ \n\
			if (toonShadingMode == 0) \n\
			{ \n\
				vec3 toonColor = vec3(texture1D(texToonTable, vtxColor.r).rgb); \n\
				fragColor.rgb = texColor.rgb * toonColor.rgb;\n\
				fragColor.a = texColor.a * vtxColor.a;\n\
			} \n\
			else \n\
			{ \n\
				vec3 toonColor = vec3(texture1D(texToonTable, vtxColor.r).rgb); \n\
				fragColor.rgb = texColor.rgb * vtxColor.rgb + toonColor.rgb; \n\
				fragColor.a = texColor.a * vtxColor.a; \n\
			} \n\
		} \n\
		else if(polygonMode == 3) \n\
		{ \n\
			if (polyID != 0) \n\
			{ \n\
				fragColor = vtxColor; \n\
			} \n\
		} \n\
		\n\
		if (fragColor.a == 0.0 || (enableAlphaTest && fragColor.a < alphaTestRef)) \n\
		{ \n\
			discard; \n\
		} \n\
		\n\
		if (oglWBuffer == 1) \n\
		{ \n\
			/* The w component is in 1.12 format, normalize it to [-1;+1] */ \
			gl_FragDepth = (vtxPosition.w / 4096.0) * 0.5 + 0.5; \n\
		} \n\
		else \n\
		{ \n\
			gl_FragDepth = (vtxPosition.z / vtxPosition.w) * 0.5 + 0.5; \n\
		} \n\
		\n\
		gl_FragColor = fragColor; \n\
	} \n\
"};

FORCEINLINE u32 BGRA8888_32_To_RGBA6665_32(const u32 srcPix)
{
	const u32 dstPix = (srcPix >> 2) & 0x3F3F3F3F;
	
	return	 (dstPix & 0x0000FF00) << 16 |		// R
			 (dstPix & 0x00FF0000)       |		// G
			 (dstPix & 0xFF000000) >> 16 |		// B
			((dstPix >> 1) & 0x000000FF);		// A
}

FORCEINLINE u32 BGRA8888_32Rev_To_RGBA6665_32Rev(const u32 srcPix)
{
	const u32 dstPix = (srcPix >> 2) & 0x3F3F3F3F;
	
	return	 (dstPix & 0x00FF0000) >> 16 |		// R
			 (dstPix & 0x0000FF00)       |		// G
			 (dstPix & 0x000000FF) << 16 |		// B
			((dstPix >> 1) & 0xFF000000);		// A
}

//opengl state caching:
//This is of dubious performance assistance, but it is easy to take out so I am leaving it for now.
//every function that is xgl* can be replaced with gl* if we decide to rip this out or if anyone else
//doesnt feel like sticking with it (or if it causes trouble)

struct GLCaps {
	u8 caps[0x100];
	GLCaps() {
		memset(caps,0xFF,sizeof(caps));
	}
};
static GLCaps glcaps;

static void _xglEnable(GLenum cap) {
	cap -= 0x0B00;
	if(glcaps.caps[cap] == 0xFF || glcaps.caps[cap] == 0) {
		glEnable(cap+0x0B00);
		glcaps.caps[cap] = 1;
	}
}

static void _xglDisable(GLenum cap) {
	cap -= 0x0B00;
	if(glcaps.caps[cap]) {
		glDisable(cap+0x0B00);
		glcaps.caps[cap] = 0;
	}
}

#define xglEnable(cap) { \
	CTASSERT((cap-0x0B00)<0x100); \
	_xglEnable(cap); }

#define xglDisable(cap) {\
	CTASSERT((cap-0x0B00)<0x100); \
	_xglDisable(cap); }

bool IsVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision)
{
	bool result = false;
	
	if ( (_OGLDriverVersion.major > checkVersionMajor) ||
		 (_OGLDriverVersion.major >= checkVersionMajor && _OGLDriverVersion.minor > checkVersionMinor) ||
		 (_OGLDriverVersion.major >= checkVersionMajor && _OGLDriverVersion.minor >= checkVersionMinor && _OGLDriverVersion.revision >= checkVersionRevision) )
	{
		result = true;
	}
	
	return result;
}

static void OGLGetDriverVersion(const char *oglVersionString,
								unsigned int *versionMajor,
								unsigned int *versionMinor,
								unsigned int *versionRevision)
{
	size_t versionStringLength = 0;
	
	if (oglVersionString == NULL)
	{
		return;
	}
	
	// First, check for the dot in the revision string. There should be at
	// least one present.
	const char *versionStrEnd = strstr(oglVersionString, ".");
	if (versionStrEnd == NULL)
	{
		return;
	}
	
	// Next, check for the space before the vendor-specific info (if present).
	versionStrEnd = strstr(oglVersionString, " ");
	if (versionStrEnd == NULL)
	{
		// If a space was not found, then the vendor-specific info is not present,
		// and therefore the entire string must be the version number.
		versionStringLength = strlen(oglVersionString);
	}
	else
	{
		// If a space was found, then the vendor-specific info is present,
		// and therefore the version number is everything before the space.
		versionStringLength = versionStrEnd - oglVersionString;
	}
	
	// Copy the version substring and parse it.
	char *versionSubstring = (char *)malloc(versionStringLength * sizeof(char));
	strncpy(versionSubstring, oglVersionString, versionStringLength);
	
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int revision = 0;
	
	sscanf(versionSubstring, "%u.%u.%u", &major, &minor, &revision);
	
	free(versionSubstring);
	versionSubstring = NULL;
	
	if (versionMajor != NULL)
	{
		*versionMajor = major;
	}
	
	if (versionMinor != NULL)
	{
		*versionMinor = minor;
	}
	
	if (versionRevision != NULL)
	{
		*versionRevision = revision;
	}
}

static void texDeleteCallback(TexCacheItem *item)
{
	_OGLRenderer->DeleteTexture(item);
}

template<bool require_profile, bool enable_3_2>
static char OGLInit(void)
{
	char result = 0;
	Render3DError error = OGLERROR_NOERR;
	
	if(!oglrender_init)
		return result;
	if(!oglrender_init())
		return result;
	
	result = Default3D_Init();
	if (result == 0)
	{
		return result;
	}

	if(!BEGINGL())
	{
		INFO("OpenGL<%s,%s>: Could not initialize -- BEGINGL() failed.\n",require_profile?"force":"auto",enable_3_2?"3_2":"old");
		result = 0;
		return result;
	}
	
	// Get OpenGL info
	const char *oglVersionString = (const char *)glGetString(GL_VERSION);
	const char *oglVendorString = (const char *)glGetString(GL_VENDOR);
	const char *oglRendererString = (const char *)glGetString(GL_RENDERER);

	// Writing to gl_FragDepth causes the driver to fail miserably on systems equipped 
	// with a Intel G965 graphic card. Warn the user and fail gracefully.
	// http://forums.desmume.org/viewtopic.php?id=9286
	if(!strcmp(oglVendorString,"Intel") && strstr(oglRendererString,"965")) 
	{
		INFO("Incompatible graphic card detected. Disabling OpenGL support.\n");
		result = 0;
		return result;
	}
	
	// Check the driver's OpenGL version
	OGLGetDriverVersion(oglVersionString, &_OGLDriverVersion.major, &_OGLDriverVersion.minor, &_OGLDriverVersion.revision);
	
	if (!IsVersionSupported(OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION))
	{
		INFO("OpenGL: Driver does not support OpenGL v%u.%u.%u or later. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			 OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION,
			 oglVersionString, oglVendorString, oglRendererString);
		
		result = 0;
		return result;
	}
	
	// Create new OpenGL rendering object
	if(enable_3_2)
	{
		if (OGLLoadEntryPoints_3_2_Func != NULL && OGLCreateRenderer_3_2_Func != NULL)
		{
			OGLLoadEntryPoints_3_2_Func();
			OGLLoadEntryPoints_Legacy(); //zero 04-feb-2013 - this seems to be necessary as well
			OGLCreateRenderer_3_2_Func(&_OGLRenderer);
		}
		else 
		{
			if(require_profile)
				return 0;
		}
	}
	
	// If the renderer doesn't initialize with OpenGL v3.2 or higher, fall back
	// to one of the lower versions.
	if (_OGLRenderer == NULL)
	{
		OGLLoadEntryPoints_Legacy();
		
		if (IsVersionSupported(2, 1, 0))
		{
			_OGLRenderer = new OpenGLRenderer_2_1;
			_OGLRenderer->SetVersion(2, 1, 0);
		}
		else if (IsVersionSupported(2, 0, 0))
		{
			_OGLRenderer = new OpenGLRenderer_2_0;
			_OGLRenderer->SetVersion(2, 0, 0);
		}
		else if (IsVersionSupported(1, 5, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_5;
			_OGLRenderer->SetVersion(1, 5, 0);
		}
		else if (IsVersionSupported(1, 4, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_4;
			_OGLRenderer->SetVersion(1, 4, 0);
		}
		else if (IsVersionSupported(1, 3, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_3;
			_OGLRenderer->SetVersion(1, 3, 0);
		}
		else if (IsVersionSupported(1, 2, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_2;
			_OGLRenderer->SetVersion(1, 2, 0);
		}
	}
	
	if (_OGLRenderer == NULL)
	{
		INFO("OpenGL: Renderer did not initialize. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			 oglVersionString, oglVendorString, oglRendererString);
		result = 0;
		return result;
	}
	
	// Initialize OpenGL extensions
	error = _OGLRenderer->InitExtensions();
	if (error != OGLERROR_NOERR)
	{
		if ( IsVersionSupported(2, 0, 0) &&
			(error == OGLERROR_SHADER_CREATE_ERROR ||
			 error == OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR ||
			 error == OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR) )
		{
			INFO("OpenGL: Shaders are not working, even though they should be. Disabling 3D renderer.\n");
			result = 0;
			return result;
		}
		else if (IsVersionSupported(3, 0, 0) && error == OGLERROR_FBO_CREATE_ERROR && OGLLoadEntryPoints_3_2_Func != NULL)
		{
			INFO("OpenGL: FBOs are not working, even though they should be. Disabling 3D renderer.\n");
			result = 0;
			return result;
		}
	}
	
	// Initialization finished -- reset the renderer
	_OGLRenderer->Reset();
	
	ENDGL();
	
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int revision = 0;
	_OGLRenderer->GetVersion(&major, &minor, &revision);
	
	INFO("OpenGL: Renderer initialized successfully (v%u.%u.%u).\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
		 major, minor, revision, oglVersionString, oglVendorString, oglRendererString);
	
	return result;
}

static void OGLReset()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->Reset();
	
	ENDGL();
}

static void OGLClose()
{
	if(!BEGINGL())
		return;
	
	delete _OGLRenderer;
	_OGLRenderer = NULL;
	
	ENDGL();
	
	Default3D_Close();
}

static void OGLRender()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->Render(&gfx3d.renderState, gfx3d.vertlist, gfx3d.polylist, &gfx3d.indexlist, gfx3d.frameCtr);
	
	ENDGL();
}

static void OGLVramReconfigureSignal()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->VramReconfigureSignal();
	
	ENDGL();
}

static void OGLRenderFinish()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->RenderFinish();
	
	ENDGL();
}

//automatically select 3.2 or old profile depending on whether 3.2 is available
GPU3DInterface gpu3Dgl = {
	"OpenGL",
	OGLInit<false,true>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

//forcibly use old profile
GPU3DInterface gpu3DglOld = {
	"OpenGL Old",
	OGLInit<true,false>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

//forcibly use new profile
GPU3DInterface gpu3Dgl_3_2 = {
	"OpenGL 3.2",
	OGLInit<true,true>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

OpenGLRenderer::OpenGLRenderer()
{
	versionMajor = 0;
	versionMinor = 0;
	versionRevision = 0;
}

bool OpenGLRenderer::IsExtensionPresent(const std::set<std::string> *oglExtensionSet, const std::string extensionName) const
{
	if (oglExtensionSet == NULL || oglExtensionSet->size() == 0)
	{
		return false;
	}
	
	return (oglExtensionSet->find(extensionName) != oglExtensionSet->end());
}

bool OpenGLRenderer::ValidateShaderCompile(GLuint theShader) const
{
	bool isCompileValid = false;
	GLint status = GL_FALSE;
	
	glGetShaderiv(theShader, GL_COMPILE_STATUS, &status);
	if(status == GL_TRUE)
	{
		isCompileValid = true;
	}
	else
	{
		GLint logSize;
		GLchar *log = NULL;
		
		glGetShaderiv(theShader, GL_INFO_LOG_LENGTH, &logSize);
		log = new GLchar[logSize];
		glGetShaderInfoLog(theShader, logSize, &logSize, log);
		
		INFO("OpenGL: SEVERE - FAILED TO COMPILE SHADER : %s\n", log);
		delete[] log;
	}
	
	return isCompileValid;
}

bool OpenGLRenderer::ValidateShaderProgramLink(GLuint theProgram) const
{
	bool isLinkValid = false;
	GLint status = GL_FALSE;
	
	glGetProgramiv(theProgram, GL_LINK_STATUS, &status);
	if(status == GL_TRUE)
	{
		isLinkValid = true;
	}
	else
	{
		GLint logSize;
		GLchar *log = NULL;
		
		glGetProgramiv(theProgram, GL_INFO_LOG_LENGTH, &logSize);
		log = new GLchar[logSize];
		glGetProgramInfoLog(theProgram, logSize, &logSize, log);
		
		INFO("OpenGL: SEVERE - FAILED TO LINK SHADER PROGRAM : %s\n", log);
		delete[] log;
	}
	
	return isLinkValid;
}

void OpenGLRenderer::GetVersion(unsigned int *major, unsigned int *minor, unsigned int *revision) const
{
	*major = this->versionMajor;
	*minor = this->versionMinor;
	*revision = this->versionRevision;
}

void OpenGLRenderer::SetVersion(unsigned int major, unsigned int minor, unsigned int revision)
{
	this->versionMajor = major;
	this->versionMinor = minor;
	this->versionRevision = revision;
}

void OpenGLRenderer::ConvertFramebuffer(const u32 *__restrict srcBuffer, u32 *dstBuffer)
{
	if (srcBuffer == NULL || dstBuffer == NULL)
	{
		return;
	}
	
	// Convert from 32-bit BGRA8888 format to 32-bit RGBA6665 reversed format. OpenGL
	// stores pixels using a flipped Y-coordinate, so this needs to be flipped back
	// to the DS Y-coordinate.
	for(int i = 0, y = 191; y >= 0; y--)
	{
		u32 *__restrict dst = dstBuffer + (y * GFX3D_FRAMEBUFFER_WIDTH);
		
		for(unsigned int x = 0; x < GFX3D_FRAMEBUFFER_WIDTH; x++, i++)
		{
			// Use the correct endian format since OpenGL uses the native endian of
			// the architecture it is running on.
#ifdef WORDS_BIGENDIAN
			*dst++ = BGRA8888_32_To_RGBA6665_32(srcBuffer[i]);
#else
			*dst++ = BGRA8888_32Rev_To_RGBA6665_32Rev(srcBuffer[i]);
#endif
		}
	}
}

OpenGLRenderer_1_2::OpenGLRenderer_1_2()
{
	isVBOSupported = false;
	isPBOSupported = false;
	isFBOSupported = false;
	isMultisampledFBOSupported = false;
	isShaderSupported = false;
	isVAOSupported = false;
	
	// Init OpenGL rendering states
	ref = new OGLRenderRef;
}

OpenGLRenderer_1_2::~OpenGLRenderer_1_2()
{
	if (ref == NULL)
	{
		return;
	}
	
	glFinish();
	
	gpuScreen3DHasNewData[0] = false;
	gpuScreen3DHasNewData[1] = false;
	
	delete [] ref->color4fBuffer;
	ref->color4fBuffer = NULL;
	
	DestroyShaders();
	DestroyVAOs();
	DestroyVBOs();
	DestroyPBOs();
	DestroyFBOs();
	DestroyMultisampledFBO();
	
	//kill the tex cache to free all the texture ids
	TexCache_Reset();
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	while(!ref->freeTextureIDs.empty())
	{
		GLuint temp = ref->freeTextureIDs.front();
		ref->freeTextureIDs.pop();
		glDeleteTextures(1, &temp);
	}
	
	glFinish();
	
	// Destroy OpenGL rendering states
	delete ref;
	ref = NULL;
}

Render3DError OpenGLRenderer_1_2::InitExtensions()
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// Initialize OpenGL
	this->InitTables();
	
	this->isShaderSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_shader_objects") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_fragment_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_program");
	
	if (this->isShaderSupported)
	{
		std::string vertexShaderProgram;
		std::string fragmentShaderProgram;
		
		error = this->LoadShaderPrograms(&vertexShaderProgram, &fragmentShaderProgram);
		if (error == OGLERROR_NOERR)
		{
			error = this->CreateShaders(&vertexShaderProgram, &fragmentShaderProgram);
			if (error != OGLERROR_NOERR)
			{
				this->isShaderSupported = false;
				
				if (error == OGLERROR_SHADER_CREATE_ERROR)
				{
					return error;
				}
			}
			else
			{
				this->CreateToonTable();
			}
		}
		else
		{
			this->isShaderSupported = false;
		}
	}
	else
	{
		INFO("OpenGL: Shaders are unsupported. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
	}
	
	this->isVBOSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object");
	if (this->isVBOSupported)
	{
		this->CreateVBOs();
	}
	
#if	!defined(GL_ARB_pixel_buffer_object) && !defined(GL_EXT_pixel_buffer_object)
	this->isPBOSupported = false;
#else
	this->isPBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_pixel_buffer_object"));
#endif
	if (this->isPBOSupported)
	{
		this->CreatePBOs();
	}
	
#if !defined(GL_ARB_vertex_array_object) && !defined(GL_APPLE_vertex_array_object)
	this->isVAOSupported = false;
#else
	this->isVAOSupported	= this->isShaderSupported &&
							  this->isVBOSupported &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_array_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_APPLE_vertex_array_object"));
#endif
	if (this->isVAOSupported)
	{
		this->CreateVAOs();
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)		|| \
	!defined(GL_EXT_framebuffer_blit)		|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	this->isFBOSupported = false;
#else
	this->isFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil");
#endif
	if (this->isFBOSupported)
	{
		error = this->CreateFBOs();
		if (error != OGLERROR_NOERR)
		{
			OGLRef.fboFinalOutputID = 0;
			this->isFBOSupported = false;
		}
	}
	else
	{
		OGLRef.fboFinalOutputID = 0;
		INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)			|| \
	!defined(GL_EXT_framebuffer_multisample)	|| \
	!defined(GL_EXT_framebuffer_blit)			|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	this->isMultisampledFBOSupported = false;
#else
	this->isMultisampledFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
#endif
	if (this->isMultisampledFBOSupported)
	{
		error = this->CreateMultisampledFBO();
		if (error != OGLERROR_NOERR)
		{
			OGLRef.selectedRenderingFBO = 0;
			this->isMultisampledFBOSupported = false;
		}
	}
	else
	{
		OGLRef.selectedRenderingFBO = 0;
		INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
	}
	
	this->InitTextures();
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateVBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffersARB(1, &OGLRef.vboVertexID);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, VERTLIST_SIZE * sizeof(VERT), NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	glGenBuffersARB(1, &OGLRef.iboIndexID);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboIndexID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort), NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyVBOs()
{
	if (!this->isVBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glDeleteBuffersARB(1, &OGLRef.vboVertexID);
	
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	glDeleteBuffersARB(1, &OGLRef.iboIndexID);
	
	this->isVBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreatePBOs()
{
	glGenBuffersARB(2, this->ref->pboRenderDataID);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT * sizeof(u32), NULL, GL_STREAM_READ_ARB);
	}
	
	glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyPBOs()
{
	if (!this->isPBOSupported)
	{
		return;
	}
	
	glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	glDeleteBuffersARB(2, this->ref->pboRenderDataID);
	
	this->isPBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::LoadShaderPrograms(std::string *outVertexShaderProgram, std::string *outFragmentShaderProgram)
{
	outVertexShaderProgram->clear();
	outFragmentShaderProgram->clear();
	
	*outVertexShaderProgram += std::string(vertexShader_100);
	*outFragmentShaderProgram += std::string(fragmentShader_100);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupShaderIO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_Color, "inColor");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateShaders(const std::string *vertexShaderProgram, const std::string *fragmentShaderProgram)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	OGLRef.vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexShaderID)
	{
		INFO("OpenGL: Failed to create the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");		
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *vertexShaderProgramChar = vertexShaderProgram->c_str();
	glShaderSource(OGLRef.vertexShaderID, 1, (const GLchar **)&vertexShaderProgramChar, NULL);
	glCompileShader(OGLRef.vertexShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexShaderID))
	{
		glDeleteShader(OGLRef.vertexShaderID);
		INFO("OpenGL: Failed to compile the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentShaderID)
	{
		glDeleteShader(OGLRef.vertexShaderID);
		INFO("OpenGL: Failed to create the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fragmentShaderProgramChar = fragmentShaderProgram->c_str();
	glShaderSource(OGLRef.fragmentShaderID, 1, (const GLchar **)&fragmentShaderProgramChar, NULL);
	glCompileShader(OGLRef.fragmentShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentShaderID))
	{
		glDeleteShader(OGLRef.vertexShaderID);
		glDeleteShader(OGLRef.fragmentShaderID);
		INFO("OpenGL: Failed to compile the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.shaderProgram = glCreateProgram();
	if(!OGLRef.shaderProgram)
	{
		glDeleteShader(OGLRef.vertexShaderID);
		glDeleteShader(OGLRef.fragmentShaderID);
		INFO("OpenGL: Failed to create the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.shaderProgram, OGLRef.vertexShaderID);
	glAttachShader(OGLRef.shaderProgram, OGLRef.fragmentShaderID);
	
	this->SetupShaderIO();
	
	glLinkProgram(OGLRef.shaderProgram);
	if (!this->ValidateShaderProgramLink(OGLRef.shaderProgram))
	{
		glDetachShader(OGLRef.shaderProgram, OGLRef.vertexShaderID);
		glDetachShader(OGLRef.shaderProgram, OGLRef.fragmentShaderID);
		glDeleteProgram(OGLRef.shaderProgram);
		glDeleteShader(OGLRef.vertexShaderID);
		glDeleteShader(OGLRef.fragmentShaderID);
		INFO("OpenGL: Failed to link the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.shaderProgram);
	glUseProgram(OGLRef.shaderProgram);
	
	// Set up shader uniforms
	GLint uniformTexSampler = glGetUniformLocation(OGLRef.shaderProgram, "texMainRender");
	glUniform1i(uniformTexSampler, 0);
	
	uniformTexSampler = glGetUniformLocation(OGLRef.shaderProgram, "texToonTable");
	glUniform1i(uniformTexSampler, OGLTextureUnitID_ToonTable);
	
	OGLRef.uniformPolyAlpha			= glGetUniformLocation(OGLRef.shaderProgram, "polyAlpha");
	OGLRef.uniformTexScale			= glGetUniformLocation(OGLRef.shaderProgram, "texScale");
	OGLRef.uniformPolyID			= glGetUniformLocation(OGLRef.shaderProgram, "polyID");
	OGLRef.uniformHasTexture		= glGetUniformLocation(OGLRef.shaderProgram, "hasTexture");
	OGLRef.uniformPolygonMode		= glGetUniformLocation(OGLRef.shaderProgram, "polygonMode");
	OGLRef.uniformToonShadingMode	= glGetUniformLocation(OGLRef.shaderProgram, "toonShadingMode");
	OGLRef.uniformWBuffer			= glGetUniformLocation(OGLRef.shaderProgram, "oglWBuffer");
	OGLRef.uniformEnableAlphaTest	= glGetUniformLocation(OGLRef.shaderProgram, "enableAlphaTest");
	OGLRef.uniformAlphaTestRef		= glGetUniformLocation(OGLRef.shaderProgram, "alphaTestRef");
	
	INFO("OpenGL: Successfully created shaders.\n");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyShaders()
{
	if(!this->isShaderSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(0);
	
	glDetachShader(OGLRef.shaderProgram, OGLRef.vertexShaderID);
	glDetachShader(OGLRef.shaderProgram, OGLRef.fragmentShaderID);
	
	glDeleteProgram(OGLRef.shaderProgram);
	glDeleteShader(OGLRef.vertexShaderID);
	glDeleteShader(OGLRef.fragmentShaderID);
	
	this->DestroyToonTable();
	
	this->isShaderSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoMainStatesID);
	glBindVertexArray(OGLRef.vaoMainStatesID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboVertexID);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glEnableVertexAttribArray(OGLVertexAttributeID_Color);
	
	glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
	glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyVAOs()
{
	if (!this->isVAOSupported)
	{
		return;
	}
	
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &this->ref->vaoMainStatesID);
	
	this->isVAOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateFBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	this->CreateClearImage();
	
	// Set up FBOs
	glGenFramebuffersEXT(1, &OGLRef.fboClearImageID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texClearImageColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &OGLRef.fboClearImageID);
		this->DestroyClearImage();
		
		this->isFBOSupported = false;
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Set up final output FBO
	OGLRef.fboFinalOutputID = 0;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
	
	INFO("OpenGL: Successfully created FBOs.\n");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyFBOs()
{
	if (!this->isFBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glDeleteFramebuffersEXT(1, &OGLRef.fboClearImageID);
	this->DestroyClearImage();
	this->isFBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateMultisampledFBO()
{
	// Check the maximum number of samples that the driver supports and use that.
	// Since our target resolution is only 256x192 pixels, using the most samples
	// possible is the best thing to do.
	GLint maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamples);
	
	if (maxSamples < 2)
	{
		INFO("OpenGL: Driver does not support at least 2x multisampled FBOs. Multisample antialiasing will be disabled.\n");
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	else if (maxSamples > OGLRENDER_MAX_MULTISAMPLES)
	{
		maxSamples = OGLRENDER_MAX_MULTISAMPLES;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
	
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleColorID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_DEPTH24_STENCIL8_EXT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	
	// Set up multisampled rendering FBO
	glGenFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMultisampleRenderID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleColorID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
	INFO("OpenGL: Successfully created multisampled FBO.\n");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyMultisampledFBO()
{
	if (!this->isMultisampledFBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glDeleteFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
	
	this->isMultisampledFBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::InitFinalRenderStates(const std::set<std::string> *oglExtensionSet)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	bool isTexMirroredRepeatSupported = this->IsExtensionPresent(oglExtensionSet, "GL_ARB_texture_mirrored_repeat");
	bool isBlendFuncSeparateSupported = this->IsExtensionPresent(oglExtensionSet, "GL_EXT_blend_func_separate");
	bool isBlendEquationSeparateSupported = this->IsExtensionPresent(oglExtensionSet, "GL_EXT_blend_equation_separate");
	
	// Blending Support
	if (isBlendFuncSeparateSupported)
	{
		if (isBlendEquationSeparateSupported)
		{
			// we want to use alpha destination blending so we can track the last-rendered alpha value
			// test: new super mario brothers renders the stormclouds at the beginning
			glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
			glBlendEquationSeparateEXT(GL_FUNC_ADD, GL_MAX);
		}
		else
		{
			glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
		}
	}
	else
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	
	// Mirrored Repeat Mode Support
	OGLRef.stateTexMirroredRepeat = isTexMirroredRepeatSupported ? GL_MIRRORED_REPEAT : GL_REPEAT;
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Map the vertex list's colors with 4 floats per color. This is being done
	// because OpenGL needs 4-colors per vertex to support translucency. (The DS
	// uses 3-colors per vertex, and adds alpha through the poly, so we can't
	// simply reference the colors+alpha from just the vertices by themselves.)
	OGLRef.color4fBuffer = this->isShaderSupported ? NULL : new GLfloat[VERTLIST_SIZE * 4];
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitTextures()
{
	this->ExpandFreeTextures();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitTables()
{
	static bool needTableInit = true;
	
	if (needTableInit)
	{
		for (unsigned int i = 0; i < 256; i++)
			material_8bit_to_float[i] = (GLfloat)(i * 4) / 255.0f;
		
		for (unsigned int i = 0; i < 32768; i++)
			dsDepthToD24S8_LUT[i] = (GLuint)DS_DEPTH15TO24(i) << 8;
		
		needTableInit = false;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateToonTable()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// The toon table is a special 1D texture where each pixel corresponds
	// to a specific color in the toon table.
	glGenTextures(1, &OGLRef.texToonTableID);
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
	
	glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_1D, 0);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DestroyToonTable()
{
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glDeleteTextures(1, &this->ref->texToonTableID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UploadToonTable(const GLushort *toonTableBuffer)
{
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, this->ref->texToonTableID);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, toonTableBuffer);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateClearImage()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenTextures(1, &OGLRef.texClearImageColorID);
	glGenTextures(1, &OGLRef.texClearImageDepthStencilID);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DestroyClearImage()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ClearImage);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glDeleteTextures(1, &OGLRef.texClearImageColorID);
	glDeleteTextures(1, &OGLRef.texClearImageDepthStencilID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UploadClearImage(const GLushort *clearImageColorBuffer, const GLint *clearImageDepthBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, clearImageColorBuffer);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, clearImageDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::GetExtensionSet(std::set<std::string> *oglExtensionSet)
{
	std::string oglExtensionString = std::string((const char *)glGetString(GL_EXTENSIONS));
	
	size_t extStringStartLoc = 0;
	size_t delimiterLoc = oglExtensionString.find_first_of(' ', extStringStartLoc);
	while (delimiterLoc != std::string::npos)
	{
		std::string extensionName = oglExtensionString.substr(extStringStartLoc, delimiterLoc - extStringStartLoc);
		oglExtensionSet->insert(extensionName);
		
		extStringStartLoc = delimiterLoc + 1;
		delimiterLoc = oglExtensionString.find_first_of(' ', extStringStartLoc);
	}
	
	if (extStringStartLoc - oglExtensionString.length() > 0)
	{
		std::string extensionName = oglExtensionString.substr(extStringStartLoc, oglExtensionString.length() - extStringStartLoc);
		oglExtensionSet->insert(extensionName);
	}
}

Render3DError OpenGLRenderer_1_2::ExpandFreeTextures()
{
	static const int kInitTextures = 128;
	GLuint oglTempTextureID[kInitTextures];
	glGenTextures(kInitTextures, oglTempTextureID);
	
	for(int i=0;i<kInitTextures;i++)
	{
		this->ref->freeTextureIDs.push(oglTempTextureID[i]);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, unsigned int *outIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	const unsigned int polyCount = polyList->count;
	unsigned int vertIndexCount = 0;
	
	for(unsigned int i = 0; i < polyCount; i++)
	{
		const POLY *poly = &polyList->list[indexList->list[i]];
		const unsigned int polyType = poly->type;
		
		if (this->isShaderSupported)
		{
			for(unsigned int j = 0; j < polyType; j++)
			{
				const GLushort vertIndex = poly->vertIndexes[j];
				
				// While we're looping through our vertices, add each vertex index to
				// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
				// vertices here to convert them to GL_TRIANGLES, which are much easier
				// to work with and won't be deprecated in future OpenGL versions.
				outIndexBuffer[vertIndexCount++] = vertIndex;
				if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
				{
					if (j == 2)
					{
						outIndexBuffer[vertIndexCount++] = vertIndex;
					}
					else if (j == 3)
					{
						outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
					}
				}
			}
		}
		else
		{
			const GLfloat thePolyAlpha = (!poly->isWireframe() && poly->isTranslucent()) ? divide5bitBy31_LUT[poly->getAttributeAlpha()] : 1.0f;
			
			for(unsigned int j = 0; j < polyType; j++)
			{
				const GLushort vertIndex = poly->vertIndexes[j];
				const GLushort colorIndex = vertIndex * 4;
				
				// Consolidate the vertex color and the poly alpha to our internal color buffer
				// so that OpenGL can use it.
				const VERT *vert = &vertList->list[vertIndex];
				OGLRef.color4fBuffer[colorIndex+0] = material_8bit_to_float[vert->color[0]];
				OGLRef.color4fBuffer[colorIndex+1] = material_8bit_to_float[vert->color[1]];
				OGLRef.color4fBuffer[colorIndex+2] = material_8bit_to_float[vert->color[2]];
				OGLRef.color4fBuffer[colorIndex+3] = thePolyAlpha;
				
				// While we're looping through our vertices, add each vertex index to a
				// buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
				// vertices here to convert them to GL_TRIANGLES, which are much easier
				// to work with and won't be deprecated in future OpenGL versions.
				outIndexBuffer[vertIndexCount++] = vertIndex;
				if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
				{
					if (j == 2)
					{
						outIndexBuffer[vertIndexCount++] = vertIndex;
					}
					else if (j == 3)
					{
						outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
					}
				}
			}
		}
	}
	
	*outIndexCount = vertIndexCount;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoMainStatesID);
		glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glEnableVertexAttribArray(OGLVertexAttributeID_Position);
			glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glEnableVertexAttribArray(OGLVertexAttributeID_Color);
			
			if (this->isVBOSupported)
			{
				glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboIndexID);
				glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), OGLRef.vertIndexBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboVertexID);
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
				glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
				glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
				glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
			}
			else
			{
				glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), &vertList->list[0].coord);
				glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), &vertList->list[0].texcoord);
				glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), &vertList->list[0].color);
			}
		}
		else
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			
			if (this->isVBOSupported)
			{
				glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboIndexID);
				glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), OGLRef.vertIndexBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboVertexID);
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
				glVertexPointer(4, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
				glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
			}
			else
			{
				glVertexPointer(4, GL_FLOAT, sizeof(VERT), &vertList->list[0].coord);
				glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), &vertList->list[0].texcoord);
				glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
			}
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DisableVertexAttributes()
{
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glDisableVertexAttribArray(OGLVertexAttributeID_Position);
			glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glDisableVertexAttribArray(OGLVertexAttributeID_Color);
		}
		else
		{
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		
		if (this->isVBOSupported)
		{
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SelectRenderingFramebuffer()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isMultisampledFBOSupported)
	{
		OGLRef.selectedRenderingFBO = CommonSettings.GFX3D_Renderer_Multisample ? OGLRef.fboMultisampleRenderID : OGLRef.fboFinalOutputID;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DownsampleFBO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isMultisampledFBOSupported || OGLRef.selectedRenderingFBO != OGLRef.fboMultisampleRenderID)
	{
		return OGLERROR_NOERR;
	}
	
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
	glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ReadBackPixels()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (this->isPBOSupported)
	{
		this->DownsampleFBO();
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DeleteTexture(const TexCacheItem *item)
{
	this->ref->freeTextureIDs.push((GLuint)item->texid);
	if(this->currTexture == item)
	{
		this->currTexture = NULL;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::BeginRender(const GFX3D_State *renderState)
{
	OGLRenderRef &OGLRef = *this->ref;
	this->doubleBufferIndex = (this->doubleBufferIndex + 1) & 0x01;
	
	this->SelectRenderingFramebuffer();
	
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformEnableAlphaTest, renderState->enableAlphaTest ? GL_TRUE : GL_FALSE);
		glUniform1f(OGLRef.uniformAlphaTestRef, divide5bitBy31_LUT[renderState->alphaTestRef]);
		glUniform1i(OGLRef.uniformToonShadingMode, renderState->shading);
		glUniform1i(OGLRef.uniformWBuffer, renderState->wbuffer);
	}
	else
	{
		if(renderState->enableAlphaTest && (renderState->alphaTestRef > 0))
		{
			glAlphaFunc(GL_GEQUAL, divide5bitBy31_LUT[renderState->alphaTestRef]);
		}
		else
		{
			glAlphaFunc(GL_GREATER, 0);
		}
	}
	
	if(renderState->enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	glDepthMask(GL_TRUE);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	OGLRenderRef &OGLRef = *this->ref;
	unsigned int vertIndexCount = 0;
	
	if (!this->isShaderSupported)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
	}
	
	this->SetupVertices(vertList, polyList, indexList, OGLRef.vertIndexBuffer, &vertIndexCount);
	this->EnableVertexAttributes(vertList, OGLRef.vertIndexBuffer, vertIndexCount);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DoRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	OGLRenderRef &OGLRef = *this->ref;
	u32 lastTexParams = 0;
	u32 lastTexPalette = 0;
	u32 lastPolyAttr = 0;
	u32 lastViewport = 0xFFFFFFFF;
	const unsigned int polyCount = polyList->count;
	GLushort *indexBufferPtr = this->isVBOSupported ? 0 : OGLRef.vertIndexBuffer;
	
	// Map GFX3D_QUADS and GFX3D_QUAD_STRIP to GL_TRIANGLES since we will convert them.
	//
	// Also map GFX3D_TRIANGLE_STRIP to GL_TRIANGLES. This is okay since this is actually
	// how the POLY struct stores triangle strip vertices, which is in sets of 3 vertices
	// each. This redefinition is necessary since uploading more than 3 indices at a time
	// will cause glDrawElements() to draw the triangle strip incorrectly.
	static const GLenum oglPrimitiveType[]	= {GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES,
											   GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP};
	
	static const unsigned int indexIncrementLUT[] = {3, 6, 3, 6, 3, 4, 3, 4};
	
	for(unsigned int i = 0; i < polyCount; i++)
	{
		const POLY *poly = &polyList->list[indexList->list[i]];
		
		// Set up the polygon if it changed
		if(lastPolyAttr != poly->polyAttr || i == 0)
		{
			lastPolyAttr = poly->polyAttr;
			this->SetupPolygon(poly);
		}
		
		// Set up the texture if it changed
		if(lastTexParams != poly->texParam || lastTexPalette != poly->texPalette || i == 0)
		{
			lastTexParams = poly->texParam;
			lastTexPalette = poly->texPalette;
			this->SetupTexture(poly, renderState->enableTexturing);
		}
		
		// Set up the viewport if it changed
		if(lastViewport != poly->viewport || i == 0)
		{
			lastViewport = poly->viewport;
			this->SetupViewport(poly);
		}
		
		// In wireframe mode, redefine all primitives as GL_LINE_LOOP rather than
		// setting the polygon mode to GL_LINE though glPolygonMode(). Not only is
		// drawing more accurate this way, but it also allows GFX3D_QUADS and
		// GFX3D_QUAD_STRIP primitives to properly draw as wireframe without the
		// extra diagonal line.
		const GLenum polyPrimitive = !poly->isWireframe() ? oglPrimitiveType[poly->vtxFormat] : GL_LINE_LOOP;
		
		// Render the polygon
		const unsigned int vertIndexCount = indexIncrementLUT[poly->vtxFormat];
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		indexBufferPtr += vertIndexCount;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::PostRender()
{
	this->DisableVertexAttributes();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::EndRender(const u64 frameCount)
{
	//needs to happen before endgl because it could free some textureids for expired cache items
	TexCache_EvictFrame();
	
	this->ReadBackPixels();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UpdateClearImage(const u16 *__restrict colorBuffer, const u16 *__restrict depthBuffer, const u8 clearStencil, const u8 xScroll, const u8 yScroll)
{
	static const size_t pixelsPerLine = GFX3D_FRAMEBUFFER_WIDTH;
	static const size_t lineCount = GFX3D_FRAMEBUFFER_HEIGHT;
	static const size_t totalPixelCount = pixelsPerLine * lineCount;
	static const size_t bufferSize = totalPixelCount * sizeof(u16);
	
	static CACHE_ALIGN GLushort clearImageColorBuffer[totalPixelCount] = {0};
	static CACHE_ALIGN GLint clearImageDepthBuffer[totalPixelCount] = {0};
	static CACHE_ALIGN u16 lastColorBuffer[totalPixelCount] = {0};
	static CACHE_ALIGN u16 lastDepthBuffer[totalPixelCount] = {0};
	static u8 lastXScroll = 0;
	static u8 lastYScroll = 0;
	
	if (!this->isFBOSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
	if (lastXScroll != xScroll ||
		lastYScroll != yScroll ||
		memcmp(colorBuffer, lastColorBuffer, bufferSize) ||
		memcmp(depthBuffer, lastDepthBuffer, bufferSize) )
	{
		lastXScroll = xScroll;
		lastYScroll = yScroll;
		memcpy(lastColorBuffer, colorBuffer, bufferSize);
		memcpy(lastDepthBuffer, depthBuffer, bufferSize);
		
		unsigned int dd = totalPixelCount - pixelsPerLine;
		
		for(unsigned int iy = 0; iy < lineCount; iy++)
		{
			const unsigned int y = ((iy + yScroll) & 0xFF) << 8;
			
			for(unsigned int ix = 0; ix < pixelsPerLine; ix++)
			{
				const unsigned int x = (ix + xScroll) & 0xFF;
				const unsigned int adr = y + x;
				
				clearImageColorBuffer[dd] = colorBuffer[adr];
				clearImageDepthBuffer[dd] = dsDepthToD24S8_LUT[depthBuffer[adr] & 0x7FFF] | clearStencil;
				
				dd++;
			}
			
			dd -= pixelsPerLine * 2;
		}
		
		this->UploadClearImage(clearImageColorBuffer, clearImageDepthBuffer);
	}
	
	this->clearImageStencilValue = clearStencil;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UpdateToonTable(const u16 *toonTableBuffer)
{
	// Update the toon table if it changed.
	if (memcmp(this->currentToonTable16, toonTableBuffer, sizeof(this->currentToonTable16)))
	{
		memcpy(this->currentToonTable16, toonTableBuffer, sizeof(this->currentToonTable16));
		this->toonTableNeedsUpdate = true;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingImage() const
{
	static u8 lastClearStencil = 0;
	
	if (!this->isFBOSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	
	// It might seem wasteful to be doing a separate glClear(GL_STENCIL_BUFFER_BIT) instead
	// of simply blitting the stencil buffer with everything else.
	//
	// We do this because glBlitFramebufferEXT() for GL_STENCIL_BUFFER_BIT has been tested
	// to be unsupported on ATI/AMD GPUs running in compatibility mode. So we do the separate
	// glClear() for GL_STENCIL_BUFFER_BIT to keep these GPUs working.
	if (lastClearStencil == this->clearImageStencilValue)
	{
		lastClearStencil = this->clearImageStencilValue;
		glClearStencil(lastClearStencil);
	}
	
	glClear(GL_STENCIL_BUFFER_BIT);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingValues(const u8 r, const u8 g, const u8 b, const u8 a, const u32 clearDepth, const u8 clearStencil) const
{
	static u8 last_r = 0;
	static u8 last_g = 0;
	static u8 last_b = 0;
	static u8 last_a = 0;
	static u32 lastClearDepth = 0;
	static u8 lastClearStencil = 0;
	
	if (r != last_r || g != last_g || b != last_b || a != last_a)
	{
		last_r = r;
		last_g = g;
		last_b = b;
		last_a = a;
		glClearColor(divide5bitBy31_LUT[r], divide5bitBy31_LUT[g], divide5bitBy31_LUT[b], divide5bitBy31_LUT[a]);
	}
	
	if (clearDepth != lastClearDepth)
	{
		lastClearDepth = clearDepth;
		glClearDepth((GLfloat)clearDepth / (GLfloat)0x00FFFFFF);
	}
	
	if (clearStencil != lastClearStencil)
	{
		lastClearStencil = clearStencil;
		glClearStencil(clearStencil);
	}
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupPolygon(const POLY *thePoly)
{
	static unsigned int lastTexBlendMode = 0;
	static int lastStencilState = -1;
	
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonAttributes attr = thePoly->getAttributes();
	
	// Set up polygon ID
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformPolyID, attr.polygonID);
	}
	
	// Set up alpha value
	if (this->isShaderSupported)
	{
		const GLfloat thePolyAlpha = (!attr.isWireframe && attr.isTranslucent) ? divide5bitBy31_LUT[attr.alpha] : 1.0f;
		glUniform1f(OGLRef.uniformPolyAlpha, thePolyAlpha);
	}
	
	// Set up depth test mode
	static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
	glDepthFunc(oglDepthFunc[attr.enableDepthTest]);
	
	// Set up culling mode
	static const GLenum oglCullingMode[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
	GLenum cullingMode = oglCullingMode[attr.surfaceCullingMode];
	
	if (cullingMode == 0)
	{
		xglDisable(GL_CULL_FACE);
	}
	else
	{
		xglEnable(GL_CULL_FACE);
		glCullFace(cullingMode);
	}
	
	// Set up depth write
	GLboolean enableDepthWrite = GL_TRUE;
	
	// Handle shadow polys. Do this after checking for depth write, since shadow polys
	// can change this too.
	if(attr.polygonMode == 3)
	{
		xglEnable(GL_STENCIL_TEST);
		if(attr.polygonID == 0)
		{
			enableDepthWrite = GL_FALSE;
			if(lastStencilState != 0)
			{
				lastStencilState = 0;
				//when the polyID is zero, we are writing the shadow mask.
				//set stencilbuf = 1 where the shadow volume is obstructed by geometry.
				//do not write color or depth information.
				glStencilFunc(GL_ALWAYS, 65, 255);
				glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			}
		}
		else
		{
			enableDepthWrite = GL_TRUE;
			if(lastStencilState != 1)
			{
				lastStencilState = 1;
				//when the polyid is nonzero, we are drawing the shadow poly.
				//only draw the shadow poly where the stencilbuf==1.
				//I am not sure whether to update the depth buffer here--so I chose not to.
				glStencilFunc(GL_EQUAL, 65, 255);
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
		}
	}
	else
	{
		xglEnable(GL_STENCIL_TEST);
		if(attr.isTranslucent)
		{
			lastStencilState = 3;
			glStencilFunc(GL_NOTEQUAL, attr.polygonID, 255);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		else if(lastStencilState != 2)
		{
			lastStencilState = 2;
			glStencilFunc(GL_ALWAYS, 64, 255);
			glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
	}
	
	if(attr.isTranslucent && !attr.enableAlphaDepthWrite)
	{
		enableDepthWrite = GL_FALSE;
	}
	
	glDepthMask(enableDepthWrite);
	
	// Set up texture blending mode
	if(attr.polygonMode != lastTexBlendMode)
	{
		lastTexBlendMode = attr.polygonMode;
		
		if(this->isShaderSupported)
		{
			glUniform1i(OGLRef.uniformPolygonMode, attr.polygonMode);
			
			// Update the toon table if necessary
			if (this->toonTableNeedsUpdate && attr.polygonMode == 2)
			{
				this->UploadToonTable(this->currentToonTable16);
				this->toonTableNeedsUpdate = false;
			}
		}
		else
		{
			static const GLint oglTexBlendMode[4] = {GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE};
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oglTexBlendMode[attr.polygonMode]);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupTexture(const POLY *thePoly, bool enableTexturing)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonTexParams params = thePoly->getTexParams();
	
	// Check if we need to use textures
	if (thePoly->texParam == 0 || params.texFormat == TEXMODE_NONE || !enableTexturing)
	{
		if (this->isShaderSupported)
		{
			glUniform1i(OGLRef.uniformHasTexture, GL_FALSE);
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
		}
		
		return OGLERROR_NOERR;
	}
	
	// Enable textures if they weren't already enabled
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformHasTexture, GL_TRUE);
	}
	else
	{
		glEnable(GL_TEXTURE_2D);
	}
	
	//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
	TexCacheItem *newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly->texParam, thePoly->texPalette);
	if(newTexture != this->currTexture)
	{
		this->currTexture = newTexture;
		//has the ogl renderer initialized the texture?
		if(!this->currTexture->deleteCallback)
		{
			this->currTexture->deleteCallback = texDeleteCallback;
			
			if(OGLRef.freeTextureIDs.empty())
			{
				this->ExpandFreeTextures();
			}
			
			this->currTexture->texid = (u64)OGLRef.freeTextureIDs.front();
			OGLRef.freeTextureIDs.pop();
			
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (params.enableRepeatS ? (params.enableMirroredRepeatS ? OGLRef.stateTexMirroredRepeat : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (params.enableRepeatT ? (params.enableMirroredRepeatT ? OGLRef.stateTexMirroredRepeat : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
						 this->currTexture->sizeX, this->currTexture->sizeY, 0,
						 GL_RGBA, GL_UNSIGNED_BYTE, this->currTexture->decoded);
		}
		else
		{
			//otherwise, just bind it
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
		}
		
		if (this->isShaderSupported)
		{
			glUniform2f(OGLRef.uniformTexScale, this->currTexture->invSizeX, this->currTexture->invSizeY);
		}
		else
		{
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glScalef(this->currTexture->invSizeX, this->currTexture->invSizeY, 1.0f);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupViewport(const POLY *thePoly)
{
	VIEWPORT viewport;
	viewport.decode(thePoly->viewport);
	glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::Reset()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	this->gpuScreen3DHasNewData[0] = false;
	this->gpuScreen3DHasNewData[1] = false;
	
	glFinish();
	
	for (unsigned int i = 0; i < 2; i++)
	{
		memset(this->GPU_screen3D[i], 0, sizeof(this->GPU_screen3D[i]));
	}
	
	memset(currentToonTable16, 0, sizeof(currentToonTable16));
	this->toonTableNeedsUpdate = true;
	
	if(this->isShaderSupported)
	{
		glUniform1f(OGLRef.uniformPolyAlpha, 1.0f);
		glUniform2f(OGLRef.uniformTexScale, 1.0f, 1.0f);
		glUniform1i(OGLRef.uniformPolyID, 0);
		glUniform1i(OGLRef.uniformHasTexture, GL_FALSE);
		glUniform1i(OGLRef.uniformPolygonMode, 0);
		glUniform1i(OGLRef.uniformToonShadingMode, 0);
		glUniform1i(OGLRef.uniformWBuffer, 0);
		glUniform1i(OGLRef.uniformEnableAlphaTest, GL_TRUE);
		glUniform1f(OGLRef.uniformAlphaTestRef, 0.0f);
	}
	else
	{
		glEnable(GL_NORMALIZE);
		glEnable(GL_TEXTURE_1D);
		glEnable(GL_TEXTURE_2D);
		glAlphaFunc(GL_GREATER, 0);
		glEnable(GL_ALPHA_TEST);
		
		memset(OGLRef.color4fBuffer, 0, VERTLIST_SIZE * 4 * sizeof(GLfloat));
	}
	
	memset(OGLRef.vertIndexBuffer, 0, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort));
	this->currTexture = NULL;
	this->doubleBufferIndex = 0;
	this->clearImageStencilValue = 0;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderFinish()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isPBOSupported)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		
		const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if (mappedBufferPtr != NULL)
		{
			this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
			glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	else
	{
		this->DownsampleFBO();
		
		u32 *__restrict workingBuffer = this->GPU_screen3D[i];
		glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, workingBuffer);
		this->ConvertFramebuffer(workingBuffer, (u32 *)gfx3d_convertedScreen);
	}
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::CreateToonTable()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// The toon table is a special 1D texture where each pixel corresponds
	// to a specific color in the toon table.
	glGenTextures(1, &OGLRef.texToonTableID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
	
	glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_1D, 0);
	
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::DestroyToonTable()
{
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTexture(GL_TEXTURE0);
	glDeleteTextures(1, &this->ref->texToonTableID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::UploadToonTable(const GLushort *toonTableBuffer)
{
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, this->ref->texToonTableID);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, toonTableBuffer);
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::CreateClearImage()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenTextures(1, &OGLRef.texClearImageColorID);
	glGenTextures(1, &OGLRef.texClearImageDepthStencilID);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::DestroyClearImage()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glDeleteTextures(1, &OGLRef.texClearImageColorID);
	glDeleteTextures(1, &OGLRef.texClearImageDepthStencilID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::UploadClearImage(const GLushort *clearImageColorBuffer, const GLint *clearImageDepthBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, clearImageColorBuffer);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, clearImageDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_4::InitFinalRenderStates(const std::set<std::string> *oglExtensionSet)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	bool isBlendEquationSeparateSupported = this->IsExtensionPresent(oglExtensionSet, "GL_EXT_blend_equation_separate");
	
	// Blending Support
	if (isBlendEquationSeparateSupported)
	{
		// we want to use alpha destination blending so we can track the last-rendered alpha value
		// test: new super mario brothers renders the stormclouds at the beginning
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
		glBlendEquationSeparateEXT(GL_FUNC_ADD, GL_MAX);
	}
	else
	{
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
	}
	
	// Mirrored Repeat Mode Support
	OGLRef.stateTexMirroredRepeat = GL_MIRRORED_REPEAT;
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Map the vertex list's colors with 4 floats per color. This is being done
	// because OpenGL needs 4-colors per vertex to support translucency. (The DS
	// uses 3-colors per vertex, and adds alpha through the poly, so we can't
	// simply reference the colors+alpha from just the vertices by themselves.)
	OGLRef.color4fBuffer = this->isShaderSupported ? NULL : new GLfloat[VERTLIST_SIZE * 4];
	
	return OGLERROR_NOERR;
}

OpenGLRenderer_1_5::~OpenGLRenderer_1_5()
{
	glFinish();
	
	DestroyVAOs();
	DestroyVBOs();
	DestroyPBOs();
}

Render3DError OpenGLRenderer_1_5::CreateVBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(1, &OGLRef.vboVertexID);
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
	glBufferData(GL_ARRAY_BUFFER, VERTLIST_SIZE * sizeof(VERT), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glGenBuffers(1, &OGLRef.iboIndexID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_5::DestroyVBOs()
{
	if (!this->isVBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &OGLRef.vboVertexID);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &OGLRef.iboIndexID);
	
	this->isVBOSupported = false;
}

Render3DError OpenGLRenderer_1_5::CreatePBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(2, OGLRef.pboRenderDataID);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER_ARB, GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT * sizeof(u32), NULL, GL_STREAM_READ);
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_5::DestroyPBOs()
{
	if (!this->isPBOSupported)
	{
		return;
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	glDeleteBuffers(2, this->ref->pboRenderDataID);
	
	this->isPBOSupported = false;
}

Render3DError OpenGLRenderer_1_5::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoMainStatesID);
	glBindVertexArray(OGLRef.vaoMainStatesID);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glEnableVertexAttribArray(OGLVertexAttributeID_Color);
	
	glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
	glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoMainStatesID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
			
			glEnableVertexAttribArray(OGLVertexAttributeID_Position);
			glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glEnableVertexAttribArray(OGLVertexAttributeID_Color);
			
			glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
			glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
			glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
		}
		else
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
			glVertexPointer(4, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
			glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::DisableVertexAttributes()
{
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glDisableVertexAttribArray(OGLVertexAttributeID_Position);
			glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glDisableVertexAttribArray(OGLVertexAttributeID_Color);
		}
		else
		{
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::ReadBackPixels()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (this->isPBOSupported)
	{
		this->DownsampleFBO();
		
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::RenderFinish()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isPBOSupported)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		
		const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
		if (mappedBufferPtr != NULL)
		{
			this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	else
	{
		this->DownsampleFBO();
		
		u32 *__restrict workingBuffer = this->GPU_screen3D[i];
		glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, workingBuffer);
		this->ConvertFramebuffer(workingBuffer, (u32 *)gfx3d_convertedScreen);
	}
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::InitExtensions()
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// Initialize OpenGL
	this->InitTables();
	
	// Load and create shaders. Return on any error, since a v2.0 driver will assume that shaders are available.
	this->isShaderSupported	= true;
	
	std::string vertexShaderProgram;
	std::string fragmentShaderProgram;
	error = this->LoadShaderPrograms(&vertexShaderProgram, &fragmentShaderProgram);
	if (error != OGLERROR_NOERR)
	{
		this->isShaderSupported = false;
		return error;
	}
	
	error = this->CreateShaders(&vertexShaderProgram, &fragmentShaderProgram);
	if (error != OGLERROR_NOERR)
	{
		this->isShaderSupported = false;
		return error;
	}
	
	this->CreateToonTable();
	
	this->isVBOSupported = true;
	this->CreateVBOs();
	
#if	!defined(GL_ARB_pixel_buffer_object) && !defined(GL_EXT_pixel_buffer_object)
	this->isPBOSupported = false;
#else
	this->isPBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_pixel_buffer_object"));
#endif
	if (this->isPBOSupported)
	{
		this->CreatePBOs();
	}
	
#if !defined(GL_ARB_vertex_array_object) && !defined(GL_APPLE_vertex_array_object)
	this->isVAOSupported = false;
#else
	this->isVAOSupported	= this->isShaderSupported &&
							  this->isVBOSupported &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_array_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_APPLE_vertex_array_object"));
#endif
	if (this->isVAOSupported)
	{
		this->CreateVAOs();
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)		|| \
	!defined(GL_EXT_framebuffer_blit)		|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	this->isFBOSupported = false;
#else
	this->isFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil");
#endif
	if (this->isFBOSupported)
	{
		error = this->CreateFBOs();
		if (error != OGLERROR_NOERR)
		{
			OGLRef.fboFinalOutputID = 0;
			this->isFBOSupported = false;
		}
	}
	else
	{
		OGLRef.fboFinalOutputID = 0;
		INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)			|| \
	!defined(GL_EXT_framebuffer_multisample)	|| \
	!defined(GL_EXT_framebuffer_blit)			|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	this->isMultisampledFBOSupported = false;
#else
	this->isMultisampledFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
#endif
	if (this->isMultisampledFBOSupported)
	{
		error = this->CreateMultisampledFBO();
		if (error != OGLERROR_NOERR)
		{
			OGLRef.selectedRenderingFBO = 0;
			this->isMultisampledFBOSupported = false;
		}
	}
	else
	{
		OGLRef.selectedRenderingFBO = 0;
		INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
	}
	
	this->InitTextures();
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::InitFinalRenderStates(const std::set<std::string> *oglExtensionSet)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// we want to use alpha destination blending so we can track the last-rendered alpha value
	// test: new super mario brothers renders the stormclouds at the beginning
	
	// Blending Support
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
	
	// Mirrored Repeat Mode Support
	OGLRef.stateTexMirroredRepeat = GL_MIRRORED_REPEAT;
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Ignore our color buffer since we'll transfer the polygon alpha through a uniform.
	OGLRef.color4fBuffer = NULL;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, unsigned int *outIndexCount)
{
	const unsigned int polyCount = polyList->count;
	unsigned int vertIndexCount = 0;
	
	for(unsigned int i = 0; i < polyCount; i++)
	{
		const POLY *poly = &polyList->list[indexList->list[i]];
		const unsigned int polyType = poly->type;
		
		for(unsigned int j = 0; j < polyType; j++)
		{
			const GLushort vertIndex = poly->vertIndexes[j];
			
			// While we're looping through our vertices, add each vertex index to
			// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
			// vertices here to convert them to GL_TRIANGLES, which are much easier
			// to work with and won't be deprecated in future OpenGL versions.
			outIndexBuffer[vertIndexCount++] = vertIndex;
			if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
			{
				if (j == 2)
				{
					outIndexBuffer[vertIndexCount++] = vertIndex;
				}
				else if (j == 3)
				{
					outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
				}
			}
		}
	}
	
	*outIndexCount = vertIndexCount;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoMainStatesID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
		
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glEnableVertexAttribArray(OGLVertexAttributeID_Color);
		
		glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
		glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::DisableVertexAttributes()
{
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		glDisableVertexAttribArray(OGLVertexAttributeID_Position);
		glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glDisableVertexAttribArray(OGLVertexAttributeID_Color);
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::BeginRender(const GFX3D_State *renderState)
{
	OGLRenderRef &OGLRef = *this->ref;
	this->doubleBufferIndex = (this->doubleBufferIndex + 1) & 0x01;
	
	this->SelectRenderingFramebuffer();
	
	glUniform1i(OGLRef.uniformEnableAlphaTest, renderState->enableAlphaTest ? GL_TRUE : GL_FALSE);
	glUniform1f(OGLRef.uniformAlphaTestRef, divide5bitBy31_LUT[renderState->alphaTestRef]);
	glUniform1i(OGLRef.uniformToonShadingMode, renderState->shading);
	glUniform1i(OGLRef.uniformWBuffer, renderState->wbuffer);
	
	if(renderState->enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	glDepthMask(GL_TRUE);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	OGLRenderRef &OGLRef = *this->ref;
	unsigned int vertIndexCount = 0;
	
	this->SetupVertices(vertList, polyList, indexList, OGLRef.vertIndexBuffer, &vertIndexCount);
	this->EnableVertexAttributes(vertList, OGLRef.vertIndexBuffer, vertIndexCount);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupPolygon(const POLY *thePoly)
{
	static unsigned int lastTexBlendMode = 0;
	static int lastStencilState = -1;
	
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonAttributes attr = thePoly->getAttributes();
	
	// Set up polygon ID
	glUniform1i(OGLRef.uniformPolyID, attr.polygonID);
	
	// Set up alpha value
	const GLfloat thePolyAlpha = (!attr.isWireframe && attr.isTranslucent) ? divide5bitBy31_LUT[attr.alpha] : 1.0f;
	glUniform1f(OGLRef.uniformPolyAlpha, thePolyAlpha);
	
	// Set up depth test mode
	static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
	glDepthFunc(oglDepthFunc[attr.enableDepthTest]);
	
	// Set up culling mode
	static const GLenum oglCullingMode[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
	GLenum cullingMode = oglCullingMode[attr.surfaceCullingMode];
	
	if (cullingMode == 0)
	{
		xglDisable(GL_CULL_FACE);
	}
	else
	{
		xglEnable(GL_CULL_FACE);
		glCullFace(cullingMode);
	}
	
	// Set up depth write
	GLboolean enableDepthWrite = GL_TRUE;
	
	// Handle shadow polys. Do this after checking for depth write, since shadow polys
	// can change this too.
	if(attr.polygonMode == 3)
	{
		xglEnable(GL_STENCIL_TEST);
		if(attr.polygonID == 0)
		{
			enableDepthWrite = GL_FALSE;
			if(lastStencilState != 0)
			{
				lastStencilState = 0;
				//when the polyID is zero, we are writing the shadow mask.
				//set stencilbuf = 1 where the shadow volume is obstructed by geometry.
				//do not write color or depth information.
				glStencilFunc(GL_ALWAYS, 65, 255);
				glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			}
		}
		else
		{
			enableDepthWrite = GL_TRUE;
			if(lastStencilState != 1)
			{
				lastStencilState = 1;
				//when the polyid is nonzero, we are drawing the shadow poly.
				//only draw the shadow poly where the stencilbuf==1.
				//I am not sure whether to update the depth buffer here--so I chose not to.
				glStencilFunc(GL_EQUAL, 65, 255);
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
		}
	}
	else
	{
		xglEnable(GL_STENCIL_TEST);
		if(attr.isTranslucent)
		{
			lastStencilState = 3;
			glStencilFunc(GL_NOTEQUAL, attr.polygonID, 255);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		else if(lastStencilState != 2)
		{
			lastStencilState = 2;
			glStencilFunc(GL_ALWAYS, 64, 255);
			glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
	}
	
	if(attr.isTranslucent && !attr.enableAlphaDepthWrite)
	{
		enableDepthWrite = GL_FALSE;
	}
	
	glDepthMask(enableDepthWrite);
	
	// Set up texture blending mode
	if(attr.polygonMode != lastTexBlendMode)
	{
		lastTexBlendMode = attr.polygonMode;
		glUniform1i(OGLRef.uniformPolygonMode, attr.polygonMode);
		
		// Update the toon table if necessary
		if (this->toonTableNeedsUpdate && attr.polygonMode == 2)
		{
			this->UploadToonTable(this->currentToonTable16);
			this->toonTableNeedsUpdate = false;
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupTexture(const POLY *thePoly, bool enableTexturing)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonTexParams params = thePoly->getTexParams();
	
	// Check if we need to use textures
	if (thePoly->texParam == 0 || params.texFormat == TEXMODE_NONE || !enableTexturing)
	{
		glUniform1i(OGLRef.uniformHasTexture, GL_FALSE);
		return OGLERROR_NOERR;
	}
	
	glUniform1i(OGLRef.uniformHasTexture, GL_TRUE);
	
	//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
	TexCacheItem *newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly->texParam, thePoly->texPalette);
	if(newTexture != this->currTexture)
	{
		this->currTexture = newTexture;
		//has the ogl renderer initialized the texture?
		if(!this->currTexture->deleteCallback)
		{
			this->currTexture->deleteCallback = texDeleteCallback;
			
			if(OGLRef.freeTextureIDs.empty())
			{
				this->ExpandFreeTextures();
			}
			
			this->currTexture->texid = (u64)OGLRef.freeTextureIDs.front();
			OGLRef.freeTextureIDs.pop();
			
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (params.enableRepeatS ? (params.enableMirroredRepeatS ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (params.enableRepeatT ? (params.enableMirroredRepeatT ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
						 this->currTexture->sizeX, this->currTexture->sizeY, 0,
						 GL_RGBA, GL_UNSIGNED_BYTE, this->currTexture->decoded);
		}
		else
		{
			//otherwise, just bind it
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
		}
		
		glUniform2f(OGLRef.uniformTexScale, this->currTexture->invSizeX, this->currTexture->invSizeY);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::ReadBackPixels()
{
	const unsigned int i = this->doubleBufferIndex;
	
	this->DownsampleFBO();
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i]);
	glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::RenderFinish()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i]);
	
	const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
	if (mappedBufferPtr != NULL)
	{
		this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}


#ifdef X432R_CUSTOMRENDERER_ENABLED
#include "GPU.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1700)
#ifdef X432R_PPL_TEST
#include <ppl.h>
#elif defined(X432R_CPP_AMP_TEST)
#include <amp.h>
#endif
#endif

namespace X432R
{
/*	bool CheckOpenGLExtensionSupported(std::string extension_name)
	{
		if( (oglrender_init == NULL) || !oglrender_init() ) return false;
		
		static std::set<std::string> supportedOpenGLExtensions;
		
		if( supportedOpenGLExtensions.size() == 0 )
		{
			std::string oglExtensionString = std::string( (const char *)glGetString(GL_EXTENSIONS) );
			
			size_t start_location = 0;
			size_t delimiter_location = oglExtensionString.find_first_of(' ', start_location);
			std::string extension_name;
			
			while(delimiter_location != std::string::npos)
			{
				extension_name = oglExtensionString.substr(start_location, delimiter_location - start_location);
				supportedOpenGLExtensions.insert(extension_name);
				
				start_location = delimiter_location + 1;
				delimiter_location = oglExtensionString.find_first_of(' ', start_location);
			}
			
			if( ( start_location - oglExtensionString.length() ) > 0 )
			{
				extension_name = oglExtensionString.substr( start_location, ( oglExtensionString.length() - start_location) );
				supportedOpenGLExtensions.insert(extension_name);
			}
		}
		
		if( supportedOpenGLExtensions.size() == 0 )
			return false;
		
		return ( supportedOpenGLExtensions.find(extension_name) != supportedOpenGLExtensions.end() );
	}
	
	bool CheckOpenGLExtensionSupported_PBO()
	{
		#if	!defined(GL_ARB_pixel_buffer_object) && !defined(GL_EXT_pixel_buffer_object)
		return false;
		#else
		return ( CheckOpenGLExtensionSupported("GL_ARB_vertex_buffer_object") || CheckOpenGLExtensionSupported("GL_ARB_pixel_buffer_object") || CheckOpenGLExtensionSupported("GL_EXT_pixel_buffer_object") );
		#endif
	}
*/	
	
	
	template <u32 RENDER_MAGNIFICATION>
	static char OGLInit(void)
	{
		X432R_STATIC_RENDER_MAGNIFICATION_CHECK();
		
		ClearBuffers();
		
		
		char result = 0;
		Render3DError error = OGLERROR_NOERR;
		
		if( (oglrender_init == NULL) || !oglrender_init() )
			return result;
		
		result = Default3D_Init();
		
		if(result == 0)
			return result;
		
		if( !BEGINGL() )
		{
			INFO("OpenGL 2.1: Could not initialize -- BEGINGL() failed.\n");
			result = 0;
			return result;
		}
		
		// Get OpenGL info
		const char *oglVersionString = (const char *)glGetString(GL_VERSION);
		const char *oglVendorString = (const char *)glGetString(GL_VENDOR);
		const char *oglRendererString = (const char *)glGetString(GL_RENDERER);
		
		// Writing to gl_FragDepth causes the driver to fail miserably on systems equipped 
		// with a Intel G965 graphic card. Warn the user and fail gracefully.
		// http://forums.desmume.org/viewtopic.php?id=9286
		if(!strcmp(oglVendorString,"Intel") && strstr(oglRendererString,"965")) 
		{
			INFO("Incompatible graphic card detected. Disabling OpenGL support.\n");
			result = 0;
			return result;
		}
		
		// Check the driver's OpenGL version
		OGLGetDriverVersion(oglVersionString, &_OGLDriverVersion.major, &_OGLDriverVersion.minor, &_OGLDriverVersion.revision);
		
		// If the renderer doesn't initialize with OpenGL v3.2 or higher, fall back
		// to one of the lower versions.
		OGLLoadEntryPoints_Legacy();
		
		OpenGLRenderer_X432 *renderer = NULL;
		
		if( IsVersionSupported(2, 1, 0) )
		{
			renderer = new OpenGLRenderer_X432();
			renderer->SetVersion(2, 1, 0);
			
			_OGLRenderer = renderer;
		}
		
		if(renderer == NULL)
		{
			INFO("OpenGL 2.1: Renderer did not initialize. Disabling 3D renderer.\n");
			result = 0;
			return result;
		}
		
		// Initialize OpenGL extensions
		error = renderer->InitExtensions<RENDER_MAGNIFICATION>();
		
		if(error != OGLERROR_NOERR)
		{
			if( IsVersionSupported(2, 0, 0) &&
				( (error == OGLERROR_SHADER_CREATE_ERROR) ||
				  (error == OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR) ||
				  (error == OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR) ) )
			{
				INFO("OpenGL: Shaders are not working, even though they should be. Disabling 3D renderer.\n");
				result = 0;
				return result;
			}
			else if( IsVersionSupported(3, 0, 0) && (error == OGLERROR_FBO_CREATE_ERROR) && (OGLLoadEntryPoints_3_2_Func != NULL) )
			{
				INFO("OpenGL: FBOs are not working, even though they should be. Disabling 3D renderer.\n");
				result = 0;
				return result;
			}
		}
		
		// Initialization finished -- reset the renderer
		renderer->Reset();
		
		ENDGL();
		
		INFO("OpenGL 2.1: Renderer initialized successfully\n");
		
		return result;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	static void OGLRender()
	{
		X432R_STATIC_RENDER_MAGNIFICATION_CHECK();
		
		if( !BEGINGL() ) return;
		
		#ifdef X432R_PROCESSTIME_CHECK
		AutoStopTimeCounter timecounter(timeCounter_3D);
		#endif
		
		OpenGLRenderer_X432 *renderer = dynamic_cast<OpenGLRenderer_X432 *>(_OGLRenderer);
		
		if(renderer != NULL)
			renderer->Render<RENDER_MAGNIFICATION>(&gfx3d.renderState, gfx3d.vertlist, gfx3d.polylist, &gfx3d.indexlist, gfx3d.frameCtr);
		
		ENDGL();
	}
	
	template <u32 RENDER_MAGNIFICATION>
	static void OGLRenderFinish()
	{
		X432R_STATIC_RENDER_MAGNIFICATION_CHECK();
		
		if( !BEGINGL() ) return;
		
		OpenGLRenderer_X432 *renderer = dynamic_cast<OpenGLRenderer_X432 *>(_OGLRenderer);
		
		if(renderer != NULL)
			renderer->RenderFinish<RENDER_MAGNIFICATION>();
		
		ENDGL();
	}
	
	
	OpenGLRenderer_X432::OpenGLRenderer_X432()
	{
		highResolutionFramebuffer = 0;
		highResolutionRenderbuffer_Color = 0;
		highResolutionRenderbuffer_DepthStencil = 0;
		
		#ifdef X432R_OPENGL_FOG_ENABLED
		uniformFogEnabled = 0;
//		uniformFogAlphaOnly = 0;
		uniformFogOffset = 0;
		uniformFogStep = 0;
		uniformFogDensity = 0;
		#endif
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::CreatePBOs()
	{
		OGLRenderRef &OGLRef = *this->ref;
		
		glGenBuffers(2, OGLRef.pboRenderDataID);
		
		for (unsigned int i = 0; i < 2; i++)
		{
			glBindBuffer( GL_PIXEL_PACK_BUFFER, OGLRef.pboRenderDataID[i] );
			glBufferData( GL_PIXEL_PACK_BUFFER, sizeof(u32) * 256 * 192 * RENDER_MAGNIFICATION * RENDER_MAGNIFICATION, NULL, GL_STREAM_READ );
		}
		
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		
		return OGLERROR_NOERR;
	}
	
	void OpenGLRenderer_X432::DestroyPBOs()
	{
//		if( !this->isPBOSupported ) return;
		
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glDeleteBuffers(2, this->ref->pboRenderDataID);
		
		this->isPBOSupported = false;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::CreateFBOs()
	{
		OGLRenderRef &OGLRef = *this->ref;
		
		#ifdef X432R_CUSTOMRENDERER_CLEARIMAGE_ENABLED
		// Set up FBO render targets
		this->CreateClearImage<RENDER_MAGNIFICATION>();
		
		// Set up FBOs
		glGenFramebuffersEXT(1, &OGLRef.fboClearImageID);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texClearImageColorID, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID, 0);
		
		if( glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT )
		{
			INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
			
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			glDeleteFramebuffersEXT(1, &OGLRef.fboClearImageID);
			this->DestroyClearImage();
			
			this->isFBOSupported = false;
			return OGLERROR_FBO_CREATE_ERROR;
		}
		#endif
		
		
		// 高解像度3Dレンダリング用FBO作成
		glGenRenderbuffersEXT(1, &highResolutionRenderbuffer_Color);
		glGenRenderbuffersEXT(1, &highResolutionRenderbuffer_DepthStencil);
		
		glGenFramebuffersEXT(1, &highResolutionFramebuffer);
		
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, highResolutionRenderbuffer_Color);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION);
		
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, highResolutionRenderbuffer_DepthStencil);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, highResolutionFramebuffer);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, highResolutionRenderbuffer_Color);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, highResolutionRenderbuffer_DepthStencil);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, highResolutionRenderbuffer_DepthStencil);
	
		if( glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT )
		{
			INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
			
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			
			glDeleteFramebuffersEXT(1, &highResolutionFramebuffer);
			glDeleteRenderbuffersEXT(1, &highResolutionRenderbuffer_Color);
			glDeleteRenderbuffersEXT(1, &highResolutionRenderbuffer_DepthStencil);
			
			this->isFBOSupported = false;
			return OGLERROR_FBO_CREATE_ERROR;
		}
		
		
		// Set up final output FBO
		OGLRef.fboFinalOutputID = 0;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
		
		INFO("OpenGL: Successfully created FBOs.\n");
		
		return OGLERROR_NOERR;
	}
	
	void OpenGLRenderer_X432::DestroyFBOs()
	{
		if( !this->isFBOSupported ) return;
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		
		
		#ifdef X432R_CUSTOMRENDERER_CLEARIMAGE_ENABLED
		OGLRenderRef &OGLRef = *this->ref;
		
		glDeleteFramebuffersEXT(1, &OGLRef.fboClearImageID);
		this->DestroyClearImage();
		#endif
		
		
		glDeleteFramebuffersEXT(1, &highResolutionFramebuffer);
		glDeleteRenderbuffersEXT(1, &highResolutionRenderbuffer_Color);
		glDeleteRenderbuffersEXT(1, &highResolutionRenderbuffer_DepthStencil);
		highResolutionFramebuffer = 0;
		highResolutionRenderbuffer_Color = 0;
		highResolutionRenderbuffer_DepthStencil = 0;
		
		this->isFBOSupported = false;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::CreateMultisampledFBO()
	{
		// Check the maximum number of samples that the driver supports and use that.
		// Since our target resolution is only 256x192 pixels, using the most samples
		// possible is the best thing to do.
		GLint msaa_samples = 0;
		
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &msaa_samples);
		
		if(msaa_samples < 2)
		{
			INFO("OpenGL: Driver does not support at least 2x multisampled FBOs. Multisample antialiasing will be disabled.\n");
			return OGLERROR_FEATURE_UNSUPPORTED;
		}
		
		#ifndef X432R_LOWQUALITYMODE_TEST
		if(msaa_samples > OGLRENDER_MAX_MULTISAMPLES)
			msaa_samples = OGLRENDER_MAX_MULTISAMPLES;
		#else
		#ifdef X432R_CUSTOMRENDERER_DEBUG
		const u32 max_samples = msaa_samples;
		#endif
		
		if(lowQualityMsaaEnabled)
		{
			if(msaa_samples > 4)
				msaa_samples = 4;
			
			else if(msaa_samples > 2)
				msaa_samples = 2;
		}
		else if(msaa_samples > OGLRENDER_MAX_MULTISAMPLES)
			msaa_samples = OGLRENDER_MAX_MULTISAMPLES;
		
		#ifdef X432R_CUSTOMRENDERER_DEBUG
		ShowDebugMessage( "MSAA Samples: " + std::to_string(msaa_samples) + " (max:" + std::to_string(max_samples) + ")" );
		#endif
		#endif
		
		OGLRenderRef &OGLRef = *this->ref;
		
		// Set up FBO render targets
		glGenRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
		glGenRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
		
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleColorID);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, msaa_samples, GL_RGBA, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, msaa_samples, GL_DEPTH24_STENCIL8_EXT, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION);
		
		// Set up multisampled rendering FBO
		glGenFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMultisampleRenderID);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleColorID);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
		
		if( glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT )
		{
			INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
			
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			glDeleteFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
			glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
			glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
			
			return OGLERROR_FBO_CREATE_ERROR;
		}
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		INFO("OpenGL: Successfully created multisampled FBO.\n");
		
		return OGLERROR_NOERR;
	}
	
	void OpenGLRenderer_X432::DestroyMultisampledFBO()
	{
		if( !this->isMultisampledFBOSupported ) return;
		
		OGLRenderRef &OGLRef = *this->ref;
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
		
		this->isMultisampledFBOSupported = false;
	}
	
	
	Render3DError OpenGLRenderer_X432::SelectRenderingFramebuffer()
	{
		OGLRenderRef &OGLRef = *this->ref;
		
		OGLRef.selectedRenderingFBO = (this->isMultisampledFBOSupported && CommonSettings.GFX3D_Renderer_Multisample) ? OGLRef.fboMultisampleRenderID : highResolutionFramebuffer;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
		
		return OGLERROR_NOERR;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::DownsampleFBO()
	{
		OGLRenderRef &OGLRef = *this->ref;
		
		if(OGLRef.selectedRenderingFBO == OGLRef.fboMultisampleRenderID)
		{
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, highResolutionFramebuffer);
			glBlitFramebufferEXT(0, 0, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, 0, 0, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, highResolutionFramebuffer);
		}
		
		return OGLERROR_NOERR;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::ReadBackPixels()
	{
		const unsigned int i = this->doubleBufferIndex;
		
		this->DownsampleFBO<RENDER_MAGNIFICATION>();
		
		glBindBuffer( GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i] );
		glReadPixels(0, 0, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		
		this->gpuScreen3DHasNewData[i] = true;
		
		return OGLERROR_NOERR;
	}
	
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::Render(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, const u64 frameCount)
	{
		Render3DError error = RENDER3DERROR_NOERR;
		
		error = this->BeginRender(renderState);
		
		if(error != RENDER3DERROR_NOERR) return error;
		
		this->UpdateToonTable(renderState->u16ToonTable);
		this->ClearFramebuffer<RENDER_MAGNIFICATION>(renderState);
		
		this->PreRender(renderState, vertList, polyList, indexList);
		this->DoRender<RENDER_MAGNIFICATION>(renderState, vertList, polyList, indexList);
		this->PostRender();
		
		this->EndRender<RENDER_MAGNIFICATION>(frameCount);
		
		return error;
	}
	
	Render3DError OpenGLRenderer_X432::BeginRender(const GFX3D_State *renderState)
	{
		OpenGLRenderer_2_0::BeginRender(renderState);
		
		
		#ifdef X432R_OPENGL_FOG_ENABLED
		const u8 * const fog_density_pointer = MMU.MMU_MEM[ARMCPU_ARM9][0x40] + 0x360;
		
		if( !renderState->enableFog || ( ( fog_density_pointer[0] == 0 ) && ( fog_density_pointer[31] == 0 ) ) )
		{
			glFogEnabled = false;
//			glDisable(GL_FOG);
			glUniform1i(uniformFogEnabled, 0);
		}
		else
		{
			const float fog_offset = (float)renderState->fogOffset / 32768.0f;
			const float fog_step = (float)(0x0400 >> renderState->fogShift) / 32768.0f;
			
			const bool alpha_only = renderState->enableFogAlphaOnly;
			const u32 color = renderState->fogColor;
			
			#if 0
			const float fog_color[4] = 
			{
				(float)(color & 0x1F) / (float)0x1F,
				(float)( (color >> 5) & 0x1F ) / (float)0x1F,
				(float)( (color >> 10) & 0x1F ) / (float)0x1F,
				(float)( (color >> 16) & 0x1F ) / (float)0x1F
			};
			#elif 0
			const u32 clearcolor = renderState->clearColor;
			
			const float fog_color[4] = 
			{
				alpha_only ? ( (float)(clearcolor & 0x1F) / (float)0x1F ) : ( (float)(color & 0x1F) / (float)0x1F ),
				alpha_only ? ( (float)( (clearcolor >> 5) & 0x1F ) / (float)0x1F ) : ( (float)( (color >> 5) & 0x1F ) / (float)0x1F ),
				alpha_only ? ( (float)( (clearcolor >> 10) & 0x1F ) / (float)0x1F ) : ( (float)( (color >> 10) & 0x1F ) / (float)0x1F ),
				alpha_only ? ( (float)( (clearcolor >> 16) & 0x1F ) / (float)0x1F ) : ( (float)( (color >> 16) & 0x1F ) / (float)0x1F ),
			};
			#else
			const u32 clearcolor = renderState->clearColor;
			
			const float fog_color[4] = 
			{
				alpha_only ? ( (float)(clearcolor & 0x1F) / (float)0x1F ) : ( (float)(color & 0x1F) / (float)0x1F ),
				alpha_only ? ( (float)( (clearcolor >> 5) & 0x1F ) / (float)0x1F ) : ( (float)( (color >> 5) & 0x1F ) / (float)0x1F ),
				alpha_only ? ( (float)( (clearcolor >> 10) & 0x1F ) / (float)0x1F ) : ( (float)( (color >> 10) & 0x1F ) / (float)0x1F ),
				1.0f
			};
			#endif
			
			float fog_density[32];
			
			for(u32 i = 0; i < 32; ++i)
			{
				fog_density[i] = 1.0f - ( (float)fog_density_pointer[i] / 128.0f );
			}
			
			glFogEnabled = true;
			
//			glEnable(GL_FOG);
			glUniform1i(uniformFogEnabled, 1);
//			glUniform1i(uniformFogAlphaOnly, alpha_only ? 1 : 0);
			
			glUniform1f(uniformFogOffset, fog_offset);
			glUniform1f(uniformFogStep, fog_step);
			glUniform1fv(uniformFogDensity, 32, fog_density);
			
			glFogfv(GL_FOG_COLOR, fog_color);
		}
		#endif
		
		
		return OGLERROR_NOERR;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::DoRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
	{
		OGLRenderRef &OGLRef = *this->ref;
		u32 lastTexParams = 0;
		u32 lastTexPalette = 0;
		u32 lastPolyAttr = 0;
		u32 lastViewport = 0xFFFFFFFF;
		const unsigned int polyCount = polyList->count;
		GLushort *indexBufferPtr = this->isVBOSupported ? 0 : OGLRef.vertIndexBuffer;
		
		// Map GFX3D_QUADS and GFX3D_QUAD_STRIP to GL_TRIANGLES since we will convert them.
		//
		// Also map GFX3D_TRIANGLE_STRIP to GL_TRIANGLES. This is okay since this is actually
		// how the POLY struct stores triangle strip vertices, which is in sets of 3 vertices
		// each. This redefinition is necessary since uploading more than 3 indices at a time
		// will cause glDrawElements() to draw the triangle strip incorrectly.
		static const GLenum oglPrimitiveType[]	= {GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES,
												   GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP};
		
		static const unsigned int indexIncrementLUT[] = {3, 6, 3, 6, 3, 4, 3, 4};
		
		
		#if defined(X432R_OPENGL_CUSTOMSTENCILTEST) && ( !defined(X432R_OPENGL_2PASSSHADOW_TEST) || defined(X432R_CUSTOMRENDERER_DEBUG) )
		#ifdef X432R_CUSTOMRENDERER_DEBUG
		u32 shadow_count = 0;
		u32 shadow_group_count = 0;
//		bool alphaDepthWriteDisabled = false;
		#endif
		
		const POLY *poly;
		u8 poly_id;
		
		shadowPolygonIDs.clear();
		
		for(u32 i = 0; i < polyCount; ++i)
		{
			poly = &polyList->list[i];
			
//			if( !alphaDepthWriteDisabled && !poly->getAttributeEnableAlphaDepthWrite() )
//				alphaDepthWriteDisabled = true;
			
			if( poly->getAttributePolygonMode() != 3 ) continue;
			
			poly_id = poly->getAttributePolygonID();
			
			if(poly_id == 0) continue;
			
			#ifdef X432R_CUSTOMRENDERER_DEBUG
			++shadow_count;
			#endif
			
			if( std::find( shadowPolygonIDs.begin(), shadowPolygonIDs.end(), poly_id ) != shadowPolygonIDs.end() ) continue;
			
			shadowPolygonIDs.push_back(poly_id);
		}
		
		#ifdef X432R_CUSTOMRENDERER_DEBUG
		shadow_group_count = shadowPolygonIDs.size();
		
		if(shadow_count > 0)
			ShowDebugMessage( "ShadowPolygon:" + std::to_string(shadow_count) + "/" + std::to_string(shadow_group_count) );
		
//		if(alphaDepthWriteDisabled)
//			ShowDebugMessage("OpenGL AlphaDepthWrite: Disabled");
		#endif
		#endif
		
		#if defined(X432R_OPENGL_CUSTOMSTENCILTEST) && defined(X432R_OPENGL_2PASSSHADOW_TEST)
		bool is_shadow;
		#endif
		
		for(u32 i = 0; i < polyCount; ++i)
		{
			const POLY *poly = &polyList->list[ indexList->list[i] ];
			
			#if defined(X432R_OPENGL_CUSTOMSTENCILTEST) && defined(X432R_OPENGL_2PASSSHADOW_TEST)
			is_shadow = ( poly->getAttributePolygonMode() == 3 );
			#endif
			
			#ifndef X432R_OPENGL_CUSTOMSTENCILTEST2
			// Set up the polygon if it changed
			if( (lastPolyAttr != poly->polyAttr) || (i == 0) )
			{
				lastPolyAttr = poly->polyAttr;
				
				#if !defined(X432R_OPENGL_CUSTOMSTENCILTEST) || !defined(X432R_OPENGL_2PASSSHADOW_TEST)
				this->SetupPolygon(poly);
				#else
				if(is_shadow)
					this->SetupShadowPolygon(poly, true);
				else
					this->SetupPolygon(poly);
				#endif
			}
			
			// Set up the texture if it changed
			if( (lastTexParams != poly->texParam) || (lastTexPalette != poly->texPalette) || (i == 0) )
			{
				lastTexParams = poly->texParam;
				lastTexPalette = poly->texPalette;
				
				this->SetupTexture<RENDER_MAGNIFICATION>(poly, renderState->enableTexturing);
			}
			#else
			// Set up the texture if it changed
			if( (lastTexParams != poly->texParam) || (lastTexPalette != poly->texPalette) || (i == 0) )
			{
				lastTexParams = poly->texParam;
				lastTexPalette = poly->texPalette;
				
				this->SetupTexture<RENDER_MAGNIFICATION>(poly, renderState->enableTexturing);
			}
			
			// Set up the polygon if it changed
			if( (lastPolyAttr != poly->polyAttr) || (i == 0) )
			{
				lastPolyAttr = poly->polyAttr;
				
				#if !defined(X432R_OPENGL_CUSTOMSTENCILTEST) || !defined(X432R_OPENGL_2PASSSHADOW_TEST)
				this->SetupPolygon(poly);
				#else
				if(is_shadow)
					this->SetupShadowPolygon(poly, true);
				else
					this->SetupPolygon(poly);
				#endif
			}
			#endif
			
			// Set up the viewport if it changed
			if( (lastViewport != poly->viewport) || (i == 0) )
			{
				lastViewport = poly->viewport;
				this->SetupViewport<RENDER_MAGNIFICATION>(poly);
			}
			
			// In wireframe mode, redefine all primitives as GL_LINE_LOOP rather than
			// setting the polygon mode to GL_LINE though glPolygonMode(). Not only is
			// drawing more accurate this way, but it also allows GFX3D_QUADS and
			// GFX3D_QUAD_STRIP primitives to properly draw as wireframe without the
			// extra diagonal line.
			const GLenum polyPrimitive = !poly->isWireframe() ? oglPrimitiveType[poly->vtxFormat] : GL_LINE_LOOP;
			
			// Render the polygon
			const unsigned int vertIndexCount = indexIncrementLUT[poly->vtxFormat];
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			
			
			#if defined(X432R_OPENGL_CUSTOMSTENCILTEST) && defined(X432R_OPENGL_2PASSSHADOW_TEST)
			if( is_shadow && ( poly->getAttributePolygonID() != 0 ) )
			{
				this->SetupShadowPolygon(poly, false);
				
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			}
			#endif
			
			
			indexBufferPtr += vertIndexCount;
		}
		
		return OGLERROR_NOERR;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::EndRender(const u64 frameCount)
	{
/*		#ifdef X432R_OPENGL_FOG_ENABLED
		glFogEnabled = false;
//		glDisable(GL_FOG);
		glUniform1i(uniformFogEnabled, 0);
		#endif
*/		
		//needs to happen before endgl because it could free some textureids for expired cache items
		TexCache_EvictFrame();
		
		this->ReadBackPixels<RENDER_MAGNIFICATION>();
		
		return OGLERROR_NOERR;
	}
	
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::ClearFramebuffer(const GFX3D_State *renderState)
	{
		Render3DError error = RENDER3DERROR_NOERR;
		
/*		struct GFX3D_ClearColor
		{
			u8 r;
			u8 g;
			u8 b;
			u8 a;
		} clearColor;
		
		clearColor.r = renderState->clearColor & 0x1F;
		clearColor.g = (renderState->clearColor >> 5) & 0x1F;
		clearColor.b = (renderState->clearColor >> 10) & 0x1F;
		clearColor.a = (renderState->clearColor >> 16) & 0x1F;
*/		
		const u8 r = renderState->clearColor & 0x1F;
		const u8 g = (renderState->clearColor >> 5) & 0x1F;
		const u8 b = (renderState->clearColor >> 10) & 0x1F;
		const u8 a = (renderState->clearColor >> 16) & 0x1F;
		
		const u8 clearStencil = (renderState->clearColor >> 24) & 0x3F;
		
		
		#ifdef X432R_OPENGL_FOG_ENABLED
/*		const bool fog_enabled = glFogEnabled && BIT15(gfx3d.renderState.clearColor);
		
		if(fog_enabled)
		{
			
		}
*/		#endif
		
		
		#ifdef X432R_CUSTOMRENDERER_CLEARIMAGE_ENABLED
		if(renderState->enableClearImage)
		{
			const u16 *__restrict clearColorBuffer = (u16 *__restrict)MMU.texInfo.textureSlotAddr[2];
			const u16 *__restrict clearDepthBuffer = (u16 *__restrict)MMU.texInfo.textureSlotAddr[3];
			const u16 scrollBits = T1ReadWord(MMU.ARM9_REG, 0x356); //CLRIMAGE_OFFSET
			const u8 xScroll = scrollBits & 0xFF;
			const u8 yScroll = (scrollBits >> 8) & 0xFF;
			
			#ifdef X432R_CUSTOMRENDERER_DEBUG
			X432R::ShowDebugMessage("OpenGL ClearImage");
			#endif
			
			error = this->UpdateClearImage<RENDER_MAGNIFICATION>(clearColorBuffer, clearDepthBuffer, clearStencil, xScroll, yScroll);
			
			if(error == RENDER3DERROR_NOERR)
				error = this->ClearUsingImage<RENDER_MAGNIFICATION>();
			
			else
				error = this->ClearUsingValues(r, g, b, a, renderState->clearDepth, clearStencil);
		}
		
		else
		#endif
			error = this->ClearUsingValues(r, g, b, a, renderState->clearDepth, clearStencil);
		
		return error;
	}
	
	#ifdef X432R_CUSTOMRENDERER_CLEARIMAGE_ENABLED
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::CreateClearImage()
	{
		OGLRenderRef &OGLRef = *this->ref;
		
		glGenTextures(1, &OGLRef.texClearImageColorID);
		glGenTextures(1, &OGLRef.texClearImageDepthStencilID);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
		
		glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
		
		glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
		
		glActiveTexture(GL_TEXTURE0);
		
		return OGLERROR_NOERR;
	}
	
	Render3DError OpenGLRenderer_X432::DestroyClearImage()
	{
		OGLRenderRef &OGLRef = *this->ref;
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
		glDeleteTextures(1, &OGLRef.texClearImageColorID);
		glDeleteTextures(1, &OGLRef.texClearImageDepthStencilID);
		
		return OGLERROR_NOERR;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::UploadClearImage(const GLushort *clearImageColorBuffer, const GLint *clearImageDepthBuffer)
	{
		#ifdef X432R_CUSTOMRENDERER_CLEARIMAGE_ENABLED
		OGLRenderRef &OGLRef = *this->ref;
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
		
		glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, clearImageColorBuffer);
		glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, clearImageDepthBuffer);
		glBindTexture(GL_TEXTURE_2D, 0);
		
		glActiveTexture(GL_TEXTURE0);
		#endif
		
		return OGLERROR_NOERR;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::UpdateClearImage(const u16 *__restrict colorBuffer, const u16 *__restrict depthBuffer, const u8 clearStencil, const u8 xScroll, const u8 yScroll)
	{
		static const u32 pixelsPerLine = 256 * RENDER_MAGNIFICATION;
		static const u32 lineCount = 192 * RENDER_MAGNIFICATION;
		static const u32 totalPixelCount = pixelsPerLine * lineCount;
		static const u32 bufferSize = totalPixelCount * sizeof(u16);
		
		const u32 xscroll = xScroll * RENDER_MAGNIFICATION;
		const u32 yscroll = yScroll * RENDER_MAGNIFICATION;
		
		static CACHE_ALIGN GLushort clearImageColorBuffer[totalPixelCount] = {0};
		static CACHE_ALIGN GLint clearImageDepthBuffer[totalPixelCount] = {0};
		static CACHE_ALIGN u16 lastColorBuffer[totalPixelCount] = {0};
		static CACHE_ALIGN u16 lastDepthBuffer[totalPixelCount] = {0};
		static u8 lastXScroll = 0;
		static u8 lastYScroll = 0;
		
		if( !this->isFBOSupported )
			return OGLERROR_FEATURE_UNSUPPORTED;
		
		if(lastXScroll != xscroll ||
			lastYScroll != yscroll ||
			memcmp(colorBuffer, lastColorBuffer, bufferSize) ||
			memcmp(depthBuffer, lastDepthBuffer, bufferSize) )
		{
			lastXScroll = xscroll;
			lastYScroll = yscroll;
			memcpy(lastColorBuffer, colorBuffer, bufferSize);
			memcpy(lastDepthBuffer, depthBuffer, bufferSize);
			
			u32 dd = totalPixelCount - pixelsPerLine;
			u32 x, y, adr, ix, iy;
			
			for(iy = 0; iy < lineCount; ++iy)
			{
				y = std::max( (iy + yscroll), (lineCount - 1) ) * pixelsPerLine;
				
				for(ix = 0; ix < pixelsPerLine; ++ix)
				{
					x = std::max( (ix + xscroll), (pixelsPerLine - 1) );
					adr = y + x;
					
					clearImageColorBuffer[dd] = colorBuffer[adr];
					clearImageDepthBuffer[dd] = dsDepthToD24S8_LUT[depthBuffer[adr] & 0x7FFF] | clearStencil;
					
					dd++;
				}
				
				dd -= pixelsPerLine * 2;
			}
			
			this->UploadClearImage<RENDER_MAGNIFICATION>(clearImageColorBuffer, clearImageDepthBuffer);
		}
		
		this->clearImageStencilValue = clearStencil;
		
		return OGLERROR_NOERR;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::ClearUsingImage() const
	{
		static u8 lastClearStencil = 0;
		
		if( !this->isFBOSupported )
			return OGLERROR_FEATURE_UNSUPPORTED;
		
		OGLRenderRef &OGLRef = *this->ref;
		
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
		glBlitFramebufferEXT(0, 0, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, 0, 0, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
		
		// It might seem wasteful to be doing a separate glClear(GL_STENCIL_BUFFER_BIT) instead
		// of simply blitting the stencil buffer with everything else.
		//
		// We do this because glBlitFramebufferEXT() for GL_STENCIL_BUFFER_BIT has been tested
		// to be unsupported on ATI/AMD GPUs running in compatibility mode. So we do the separate
		// glClear() for GL_STENCIL_BUFFER_BIT to keep these GPUs working.
		if(lastClearStencil == this->clearImageStencilValue)
		{
			lastClearStencil = this->clearImageStencilValue;
			glClearStencil(lastClearStencil);
		}
		
		glClear(GL_STENCIL_BUFFER_BIT);
		
		return OGLERROR_NOERR;
	}
	#endif
	
	Render3DError OpenGLRenderer_X432::ClearUsingValues(const u8 r, const u8 g, const u8 b, const u8 a, const u32 clearDepth, const u8 clearStencil) const
	{
		static u8 last_r = 0;
		static u8 last_g = 0;
		static u8 last_b = 0;
		static u8 last_a = 0;
		static u32 lastClearDepth = 0;
		static u8 lastClearStencil = 0;
		
		#ifdef X432R_OPENGL_FOG_ENABLED
/*		const bool clear_fogged = BIT15(gfx3d.renderState.clearColor);
		const u32 clear_depth = clearDepth;
		
		u32 fog_color = ;
		
		r = ;
		g = ;
		b = ;
		a = ;
*/		#endif
		
		if( (r != last_r) || (g != last_g) || (b != last_b) || (a != last_a) )
		{
			last_r = r;
			last_g = g;
			last_b = b;
			last_a = a;
			
			if(a == 0)
				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			else
				glClearColor( divide5bitBy31_LUT[r], divide5bitBy31_LUT[g], divide5bitBy31_LUT[b], divide5bitBy31_LUT[a] );
		}
		
		if(clearDepth != lastClearDepth)
		{
			lastClearDepth = clearDepth;
			glClearDepth( (GLfloat)clearDepth / (GLfloat)0x00FFFFFF );
		}
		
		#ifndef X432R_OPENGL_CUSTOMSTENCILTEST
		if(clearStencil != lastClearStencil)
		{
			lastClearStencil = clearStencil;
			glClearStencil(clearStencil);
		}
		#elif !defined(X432R_OPENGL_2PASSSHADOW_TEST)
		if(lastClearStencil != 0xFF)
		{
			lastClearStencil = 0xFF;
			glClearStencil(0xFF);
		}
		#else
		if(clearStencil != lastClearStencil)
		{
			lastClearStencil = clearStencil;
			glClearStencil(clearStencil << 2);
		}
		#endif
		
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
		return OGLERROR_NOERR;
	}
	
	
	Render3DError OpenGLRenderer_X432::SetupPolygon(const POLY *thePoly)
	{
//		static unsigned int lastTexBlendMode = 0;
		
		OGLRenderRef &OGLRef = *this->ref;
		const PolygonAttributes attr = thePoly->getAttributes();
		
		// Set up polygon ID
		glUniform1i(OGLRef.uniformPolyID, attr.polygonID);
		
		// Set up alpha value
		const GLfloat thePolyAlpha = ( !attr.isWireframe && attr.isTranslucent ) ? divide5bitBy31_LUT[attr.alpha] : 1.0f;
		glUniform1f(OGLRef.uniformPolyAlpha, thePolyAlpha);
		
		
		// Set up depth test mode
		static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
		glDepthFunc( oglDepthFunc[attr.enableDepthTest] );
		
		// Set up culling mode
		static const GLenum oglCullingMode[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
		GLenum cullingMode = oglCullingMode[attr.surfaceCullingMode];
		
		if(cullingMode == 0)
		{
			xglDisable(GL_CULL_FACE);
		}
		else
		{
			xglEnable(GL_CULL_FACE);
			glCullFace(cullingMode);
		}
		
		
		#ifndef X432R_OPENGL_CUSTOMSTENCILTEST
		static int lastStencilState = -1;
		
		// Set up depth write
		GLboolean enableDepthWrite = GL_TRUE;
		
		// Handle shadow polys. Do this after checking for depth write, since shadow polys
		// can change this too.
		if(attr.polygonMode == 3)
		{
			xglEnable(GL_STENCIL_TEST);
			if(attr.polygonID == 0)
			{
				enableDepthWrite = GL_FALSE;
				if(lastStencilState != 0)
				{
					lastStencilState = 0;
					//when the polyID is zero, we are writing the shadow mask.
					//set stencilbuf = 1 where the shadow volume is obstructed by geometry.
					//do not write color or depth information.
					glStencilFunc(GL_ALWAYS, 65, 255);
					glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
					glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				}
			}
			else
			{
				enableDepthWrite = GL_TRUE;
				if(lastStencilState != 1)
				{
					lastStencilState = 1;
					//when the polyid is nonzero, we are drawing the shadow poly.
					//only draw the shadow poly where the stencilbuf==1.
					//I am not sure whether to update the depth buffer here--so I chose not to.
					glStencilFunc(GL_EQUAL, 65, 255);
					glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
					glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				}
			}
		}
		else
		{
			xglEnable(GL_STENCIL_TEST);
			if(attr.isTranslucent)
			{
				lastStencilState = 3;
				glStencilFunc(GL_NOTEQUAL, attr.polygonID, 255);
				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
			else if(lastStencilState != 2)
			{
				lastStencilState = 2;
				glStencilFunc(GL_ALWAYS, 64, 255);
				glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
		}
		
		if(attr.isTranslucent && !attr.enableAlphaDepthWrite)
		{
			enableDepthWrite = GL_FALSE;
		}
		
		glDepthMask(enableDepthWrite);
		
		#elif !defined(X432R_OPENGL_2PASSSHADOW_TEST)
		// memo: SoftRastでは5つのステンシルバッファが使用されている
		// 
		// Fragment.polyid.opaque
		// 初期値: (renderState->clearColor >> 24) & 0x3F
		// 不透明ポリゴン描画時にpolyIDをセット
		// 
		// Fragment.polyid.translucent
		// 初期値: 0xFF
		// 半透明ポリゴン描画時にpolyIDをセット, 同一IDなら描画をskip
		// 
		// Fragment.stencil
		// 初期値: 0
		// shadow用
		// (ID == 0) 描画時に+1
		// (ID != 0) 描画時に-1 (ただしFragment.polyid.opaqueに同一IDがセットされている場合は描画をskip)
		// 
		// Fragment.fogged
		// 初期値: BIT15(gfx3d.renderState.clearColor)
		// フォグ描画フラグ
		// SoftRastでは全ポリゴンを描画後、フレームバッファのFragment.depthとFragment.foggedを用いてフォグを合成する
		// OpenGLのフラグメントシェーダはポリゴン毎に処理を行うため、フォグのalpha値をポリゴンに反映させると正しく描画できない
		// フォグを完全に再現するには
		// 
		// Fragment.isTranslucentPoly
		// 初期値: 0
		// EdgeMarking用？
		// 半透明ポリゴンならEdgeMarkingの処理をskip
		
		if(attr.polygonMode == 3)		// polygonMode 0:modulate 1:decal 3:toon/highlight(gfx3d.renderState.shadingでtoon/highlight選択) 3:shadow
		{
			xglEnable(GL_STENCIL_TEST);
			
			if(attr.polygonID == 0)
			{
				glStencilFunc(GL_NOTEQUAL, 0x80, 0xFF);
				glStencilOp(GL_KEEP, GL_ZERO, GL_KEEP);						// デプステストfail時のみステンシル値に0をセット(clearStencilは0xFFに固定してしまう)
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glDepthMask(GL_FALSE);
			}
			else
			{
				glStencilFunc(GL_EQUAL, 0, 0xFF);							// ステンシル値に0がセットされている部分のみシャドウを描画
//				glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);					// 1回シャドウを描画したらステンシル値をリセット(0 → 0xFF)
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);						// ステンシル値をリセットしない(複数回シャドウを描画可能)
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask(GL_TRUE);
			}
		}
		else if( attr.isTranslucent || ( std::find( shadowPolygonIDs.begin(), shadowPolygonIDs.end(), attr.polygonID ) == shadowPolygonIDs.end() ) )
		{
			xglDisable(GL_STENCIL_TEST);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			
			#ifndef X432R_OPENGL_CUSTOMSTENCILTEST2
			#if 1
			if( !attr.isTranslucent || ( (attr.polygonMode == 1) && attr.isOpaque ) || attr.enableAlphaDepthWrite )		// modeがdecalの場合はテクスチャのアルファ値が無視されるため attr.isTranslucent && attr.isOpaque ならisTranslucentを無視
				glDepthMask(GL_TRUE);
			else
				glDepthMask(GL_FALSE);
			#else
			if(attr.isOpaque || attr.enableAlphaDepthWrite)		// 一部のポリゴンが正常に描画されない問題が改善するが、副作用あり
				glDepthMask(GL_TRUE);							// SoftRastではフラグメント毎のポリゴン・テクスチャ色の合成結果を見てDepth値書き込みの可否を判定しているが、OpenGLではポリゴン毎にON/OFFしているため完全に再現できていない
			else
				glDepthMask(GL_FALSE);
			#endif
			#else
			if( !attr.isTranslucent || ( (attr.polygonMode == 1) && attr.isOpaque ) || !isCurrentTextureTranslucent || attr.enableAlphaDepthWrite )		// modeがdecalの場合はテクスチャのアルファ値が無視されるため attr.isTranslucent && attr.isOpaque ならisTranslucentを無視
				glDepthMask(GL_TRUE);
			else
				glDepthMask(GL_FALSE);
			#endif
		}
		else
		{
			// shadowポリゴンと同一IDグループの場合はステンシル値に0x80をセット（shadowポリゴンが複数ある場合、正常に描画されない可能性あり）
			xglEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_ALWAYS, 0x80, 0xFF);
//			glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_TRUE);
		}
		#else
		// OpenGL用：ステンシルバッファ8bit
		// 1111 1100	bit2-7: polygonID 0〜3F				func:GL_ALWAYS		ref:(polygonID << 2)		mask:0xFF
		// 000 00011	bit0-1: shadowフラグ
		
		if( attr.isTranslucent && ( (attr.polygonMode != 1) || !attr.isOpaque ) )		// modeがdecalの場合はテクスチャのアルファ値が無視されるため attr.isTranslucent && attr.isOpaque ならisTranslucentを無視
		{
			xglDisable(GL_STENCIL_TEST);
			
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(attr.enableAlphaDepthWrite ? GL_TRUE : GL_FALSE);
		}
		else
		{
			xglEnable(GL_STENCIL_TEST);
			
			glStencilFunc( GL_ALWAYS, (attr.polygonID << 2), 0xFF );
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_TRUE);
		}
		#endif
		
		
/*		// (clearcolor.A == 0)の場合でもclearcolorとfragmentcolorが合成されてしまうため表示色がおかしくなる現象を回避
		if( (attr.polygonMode != 3) && (attr.alpha > 0) && (attr.alpha < 31) )
		{
			glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_SRC_ALPHA, GL_DST_ALPHA);
//			glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
		}
		else
		{
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
//			glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
		}
*/		
		
		// Set up texture blending mode
//		if(attr.polygonMode != lastTexBlendMode)
//		{
//			lastTexBlendMode = attr.polygonMode;
			glUniform1i(OGLRef.uniformPolygonMode, attr.polygonMode);
			
			// Update the toon table if necessary
			if( this->toonTableNeedsUpdate && (attr.polygonMode == 2) )
			{
				this->UploadToonTable(this->currentToonTable16);
				this->toonTableNeedsUpdate = false;
			}
//		}
		
		
		#ifdef X432R_OPENGL_FOG_ENABLED
		if( !glFogEnabled || !attr.enableRenderFog || (attr.alpha == 0) || ( (attr.polygonMode == 3) && (attr.polygonID == 0) ) )
		{
//			glDisable(GL_FOG);
			glUniform1i(uniformFogEnabled, 0);
		}
		else
		{
//			glEnable(GL_FOG);
			glUniform1i(uniformFogEnabled, 1);
		}
		#endif
		
		
		return OGLERROR_NOERR;
	}
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::SetupTexture(const POLY *thePoly, bool enableTexturing)
	{
		OGLRenderRef &OGLRef = *this->ref;
		const PolygonTexParams params = thePoly->getTexParams();
		
		// Check if we need to use textures
		if (thePoly->texParam == 0 || params.texFormat == TEXMODE_NONE || !enableTexturing)
		{
			glUniform1i(OGLRef.uniformHasTexture, GL_FALSE);
			return OGLERROR_NOERR;
		}
		
		glUniform1i(OGLRef.uniformHasTexture, GL_TRUE);
		
		//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
		TexCacheItem *newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly->texParam, thePoly->texPalette);
		if(newTexture != this->currTexture)
		{
			this->currTexture = newTexture;
			//has the ogl renderer initialized the texture?
			if(!this->currTexture->deleteCallback)
			{
				this->currTexture->deleteCallback = texDeleteCallback;
				
				if(OGLRef.freeTextureIDs.empty())
				{
					this->ExpandFreeTextures();
				}
				
				this->currTexture->texid = (u64)OGLRef.freeTextureIDs.front();
				OGLRef.freeTextureIDs.pop();
				
				glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (params.enableRepeatS ? (params.enableMirroredRepeatS ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (params.enableRepeatT ? (params.enableMirroredRepeatT ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
				
				
				#ifdef X432R_OPENGL_CUSTOMSTENCILTEST2
				isCurrentTextureTranslucent = newTexture->IsTranslucent;
				#endif
				
				#ifndef X432R_HIGHRESO_TEXTURE_TEST
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->currTexture->sizeX, this->currTexture->sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, this->currTexture->decoded);
				#else
				const u32 *highreso_texture_data = NULL;
				
				if(newTexture->IsDisplayCapturedTexture)
				{
					highreso_texture_data = X432R::backBuffer.GetHighResolutionTextureBuffer();
					
					#ifdef X432R_CUSTOMRENDERER_DEBUG
					X432R::ShowDebugMessage("displaycapture->texture");
					#endif
				}
				
				if(highreso_texture_data == NULL)
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->currTexture->sizeX, this->currTexture->sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, this->currTexture->decoded);
				else
//					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, 0, GL_RGBA, GL_UNSIGNED_BYTE, highreso_texture_data);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION, 0, GL_BGRA, GL_UNSIGNED_BYTE, highreso_texture_data);
				#endif
			}
			else
			{
				//otherwise, just bind it
				glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
			}
			
			glUniform2f(OGLRef.uniformTexScale, this->currTexture->invSizeX, this->currTexture->invSizeY);
		}
		
		return OGLERROR_NOERR;
	}
	
	#ifdef X432R_OPENGL_2PASSSHADOW_TEST
	void OpenGLRenderer_X432::SetupShadowPolygon(const POLY *thePoly, bool first_pass)
	{
		OGLRenderRef &OGLRef = *this->ref;
		const PolygonAttributes attr = thePoly->getAttributes();
		
		
		if(first_pass)
		{
			// Set up polygon ID
			glUniform1i(OGLRef.uniformPolyID, 0);
			
			
			// Set up alpha value
			const GLfloat thePolyAlpha = ( !attr.isWireframe && attr.isTranslucent ) ? divide5bitBy31_LUT[attr.alpha] : 1.0f;
			glUniform1f(OGLRef.uniformPolyAlpha, thePolyAlpha);
			
			
			// Set up depth test mode
			static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
			glDepthFunc( oglDepthFunc[attr.enableDepthTest] );
			
			
			// Set up culling mode
			static const GLenum oglCullingMode[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
			GLenum cullingMode = oglCullingMode[attr.surfaceCullingMode];
		
			if(cullingMode == 0)
			{
				xglDisable(GL_CULL_FACE);
			}
			else
			{
				xglEnable(GL_CULL_FACE);
				glCullFace(cullingMode);
			}
			
			
			xglEnable(GL_STENCIL_TEST);
			
			if(attr.polygonID == 0)
			{
//				glStencilFunc(GL_ALWAYS, 0, 0);
				glStencilFunc(GL_NOTEQUAL, 1, 0x01);						// shadowフラグがOFFの場合のみ反映
				glStencilOp(GL_KEEP, GL_INCR, GL_KEEP);						// デプステストfail時にステンシル値を+1してbit0のshadowフラグをON
			}
			else
			{
				glStencilFunc(GL_EQUAL, (attr.polygonID << 2) + 1, 0xFF);
				glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);						// IDグループが同一のポリゴンが存在する場合はshadowフラグをOFF
				// 同一箇所にシャドウを複数回描画した場合に問題が出るかも
			}
			
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);			// Color／Depthバッファへの書き込みを無効化
			glDepthMask(GL_FALSE);
			
			
			// Set up texture blending mode
			glUniform1i(OGLRef.uniformPolygonMode, attr.polygonMode);
		}
		else
		{
			// Set up polygon ID
			glUniform1i(OGLRef.uniformPolyID, attr.polygonID);
			
			
			xglEnable(GL_STENCIL_TEST);
			
//			if(attr.polygonID != 0)
//			{
				glStencilFunc(GL_EQUAL, 1, 0x01);
//				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
				
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask(GL_TRUE);
//			}
		}
	}
	#endif
	
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::SetupViewport(const POLY *thePoly)
	{
		if(thePoly->viewport == 0xBFFF0000)		// x == 0, y == 0, width == 256, height == 192
		{
			glViewport(0, 0, 256 * RENDER_MAGNIFICATION, 192 * RENDER_MAGNIFICATION);
			return OGLERROR_NOERR;
		}
		
		VIEWPORT viewport;
		
		viewport.decode(thePoly->viewport);
		
		glViewport(viewport.x * RENDER_MAGNIFICATION, viewport.y * RENDER_MAGNIFICATION,
					viewport.width * RENDER_MAGNIFICATION, viewport.height * RENDER_MAGNIFICATION);
		
		return OGLERROR_NOERR;
	}
	
	
	#if !defined(_MSC_VER) || (_MSC_VER < 1700) || ( !defined(X432R_PPL_TEST) && !defined(X432R_CPP_AMP_TEST) )
	template <u32 RENDER_MAGNIFICATION>
	void OpenGLRenderer_X432::DownscaleFramebuffer(const u32 * const sourcebuffer_begin)
	{
		u32 * const highresobuffer_begin = backBuffer.GetHighResolution3DBuffer();
		u32 *highreso_buffer = highresobuffer_begin;
		u32 * const gfx3d_buffer = (u32 *)gfx3d_convertedScreen;
		const u32 *source_buffer;
		
		u32 x, y, remainder_x, remainder_y, downscaled_index;
		RGBA8888 color_rgba8888, color_tiletopleft, color_tiletopright, color_tilebottomleft;
		u32 tiletop_index, tilebottom_index, tileleft_x;
		u32 r, g, b, a;
		
		
		#ifdef X432R_PROCESSTIME_CHECK
		AutoStopTimeCounter timecounter(timeCounter_3DFinish1);
		#endif
		
		
		for( y = 0; y < (192 * RENDER_MAGNIFICATION); ++y )
		{
			remainder_y = (y % RENDER_MAGNIFICATION);
			downscaled_index = (y / RENDER_MAGNIFICATION) * 256;
			
			source_buffer = sourcebuffer_begin + ( ( ( (192 * RENDER_MAGNIFICATION) - 1 ) - y ) * (256 * RENDER_MAGNIFICATION) );		// Y軸を反転
			
			for( x = 0; x < (256 * RENDER_MAGNIFICATION); ++x, ++source_buffer, ++highreso_buffer )
			{
				remainder_x = (x % RENDER_MAGNIFICATION);
				
				color_rgba8888 = *source_buffer;
				
				if(color_rgba8888.A == 0)
					color_rgba8888 = 0;
				
				*highreso_buffer = color_rgba8888.Color;
				
				// Bilinear
				if( ( remainder_x != (RENDER_MAGNIFICATION - 1) ) || ( remainder_y != (RENDER_MAGNIFICATION - 1) ) ) continue;
				
				tiletop_index = ( y - (RENDER_MAGNIFICATION - 1) ) * (256 * RENDER_MAGNIFICATION);
				tilebottom_index = y * (256 * RENDER_MAGNIFICATION);
				tileleft_x = x - (RENDER_MAGNIFICATION - 1);
				
				color_tiletopleft = highresobuffer_begin[tiletop_index + tileleft_x];
				color_tiletopright = highresobuffer_begin[tiletop_index + x];
				color_tilebottomleft = highresobuffer_begin[tilebottom_index + tileleft_x];
				
				r = color_tiletopleft.R + color_tiletopright.R + color_tilebottomleft.R + color_rgba8888.R;
				g = color_tiletopleft.G + color_tiletopright.G + color_tilebottomleft.G + color_rgba8888.G;
				b = color_tiletopleft.B + color_tiletopright.B + color_tilebottomleft.B + color_rgba8888.B;
				a = color_tiletopleft.A + color_tiletopright.A + color_tilebottomleft.A + color_rgba8888.A;
				
				color_rgba8888.R = (u8)std::min<u32>(r / 4, 0xFF);
				color_rgba8888.G = (u8)std::min<u32>(g / 4, 0xFF);
				color_rgba8888.B = (u8)std::min<u32>(b / 4, 0xFF);
				color_rgba8888.A = (u8)std::min<u32>(a / 4, 0xFF);
				
				#ifdef WORDS_BIGENDIAN
				gfx3d_buffer[ downscaled_index + (x / RENDER_MAGNIFICATION) ] = BGRA8888_32_To_RGBA6665_32(color_rgba8888.Color);
				#else
				gfx3d_buffer[ downscaled_index + (x / RENDER_MAGNIFICATION) ] = BGRA8888_32Rev_To_RGBA6665_32Rev(color_rgba8888.Color);		// ピクセルフォーマットをRGBA6665に変換して値を保存
				#endif
			}
		}
	}
	#elif defined(X432R_PPL_TEST)
	template <u32 RENDER_MAGNIFICATION>
	void OpenGLRenderer_X432::DownscaleFramebuffer(const u32 * const sourcebuffer_begin)
	{
		u32 * const highresobuffer_begin = backBuffer.GetHighResolution3DBuffer();
		u32 * const gfx3d_buffer = (u32 *)gfx3d_convertedScreen;
		
		
		#ifdef X432R_PROCESSTIME_CHECK
		AutoStopTimeCounter timecounter(timeCounter_3DFinish1);
		#endif
		
		
		concurrency::parallel_for( (u32)0, RENDER_MAGNIFICATION, [&](const u32 offset)
		{
			const u32 y_begin = 192 * offset;
			const u32 y_end = y_begin + 192;
			
			u32 *highreso_buffer = highresobuffer_begin + (y_begin * 256 * RENDER_MAGNIFICATION);
			const u32 *source_buffer;
			
			u32 x, y, remainder_x, remainder_y, downscaled_index;
			RGBA8888 color_rgba8888, color_tiletopleft, color_tiletopright, color_tilebottomleft;
			u32 tiletop_index, tilebottom_index, tileleft_x;
			u32 r, g, b, a;
			
			for(y = y_begin; y < y_end; ++y)
			{
				remainder_y = (y % RENDER_MAGNIFICATION);
				downscaled_index = (y / RENDER_MAGNIFICATION) * 256;
				
				source_buffer = sourcebuffer_begin + ( ( ( (192 * RENDER_MAGNIFICATION) - 1 ) - y ) * (256 * RENDER_MAGNIFICATION) );		// Y軸を反転
				
				for( x = 0; x < (256 * RENDER_MAGNIFICATION); ++x, ++source_buffer, ++highreso_buffer )
				{
					remainder_x = (x % RENDER_MAGNIFICATION);
					
					color_rgba8888 = *source_buffer;
					
					if(color_rgba8888.A == 0)
						color_rgba8888 = 0;
					
					*highreso_buffer = color_rgba8888.Color;
					
					// Bilinear
					if( ( remainder_x != (RENDER_MAGNIFICATION - 1) ) || ( remainder_y != (RENDER_MAGNIFICATION - 1) ) ) continue;
					
					tiletop_index = ( y - (RENDER_MAGNIFICATION - 1) ) * (256 * RENDER_MAGNIFICATION);
					tilebottom_index = y * (256 * RENDER_MAGNIFICATION);
					tileleft_x = x - (RENDER_MAGNIFICATION - 1);
					
					color_tiletopleft = highresobuffer_begin[tiletop_index + tileleft_x];
					color_tiletopright = highresobuffer_begin[tiletop_index + x];
					color_tilebottomleft = highresobuffer_begin[tilebottom_index + tileleft_x];
					
					r = color_tiletopleft.R + color_tiletopright.R + color_tilebottomleft.R + color_rgba8888.R;
					g = color_tiletopleft.G + color_tiletopright.G + color_tilebottomleft.G + color_rgba8888.G;
					b = color_tiletopleft.B + color_tiletopright.B + color_tilebottomleft.B + color_rgba8888.B;
					a = color_tiletopleft.A + color_tiletopright.A + color_tilebottomleft.A + color_rgba8888.A;
					
					color_rgba8888.R = (u8)std::min<u32>(r / 4, 0xFF);
					color_rgba8888.G = (u8)std::min<u32>(g / 4, 0xFF);
					color_rgba8888.B = (u8)std::min<u32>(b / 4, 0xFF);
					color_rgba8888.A = (u8)std::min<u32>(a / 4, 0xFF);
					
					#ifdef WORDS_BIGENDIAN
					gfx3d_buffer[ downscaled_index + (x / RENDER_MAGNIFICATION) ] = BGRA8888_32_To_RGBA6665_32(color_rgba8888.Color);
					#else
					gfx3d_buffer[ downscaled_index + (x / RENDER_MAGNIFICATION) ] = BGRA8888_32Rev_To_RGBA6665_32Rev(color_rgba8888.Color);		// ピクセルフォーマットをRGBA6665に変換して値を保存
					#endif
				}
			}
		});
	}
	#else
	static concurrency::array<u32, 2> ampTestBuffer1(768, 1024);
	static concurrency::array<u32, 2> ampTestBuffer2(768, 1024);
	static concurrency::array<u32, 2> ampTestBuffer3(192, 256);
	
	template <u32 RENDER_MAGNIFICATION>
	void OpenGLRenderer_X432::DownscaleFramebuffer(const u32 *source_buffer)
	{
		#ifdef X432R_PROCESSTIME_CHECK
		AutoStopTimeCounter timecounter(timeCounter_3DFinish1);
		#endif
		
		const concurrency::array_view<u32, 2> highreso_buffer = ampTestBuffer1.section(0, 0, 192 * RENDER_MAGNIFICATION, 256 * RENDER_MAGNIFICATION);
		const concurrency::array_view<u32, 2> highreso_buffer2( 192 * RENDER_MAGNIFICATION, 256 * RENDER_MAGNIFICATION, backBuffer.GetHighResolution3DBuffer() );
		
		const concurrency::array_view<u32, 2> source_buffer2 = ampTestBuffer2.section(0, 0, 192 * RENDER_MAGNIFICATION, 256 * RENDER_MAGNIFICATION);
		const concurrency::array_view<u32, 2> downscaled_buffer = ampTestBuffer3.section(0, 0, 192, 256);
		
		const concurrency::array_view<u32, 2> source_buffer3( 192 * RENDER_MAGNIFICATION, 256 * RENDER_MAGNIFICATION, (u32 *)source_buffer );
		const concurrency::array_view<u32, 2> downscaled_buffer2( 192, 256, (u32 *)gfx3d_convertedScreen );
		
		source_buffer3.copy_to(source_buffer2);
		
/*		const concurrency::array_view<u32, 2> highreso_buffer( 192 * RENDER_MAGNIFICATION, 256 * RENDER_MAGNIFICATION, backBuffer.GetHighResolution3DBuffer() );
		const concurrency::array_view<const u32, 2> source_buffer2(192 * RENDER_MAGNIFICATION, 256 * RENDER_MAGNIFICATION, source_buffer);
		const concurrency::array_view<u32, 2> downscaled_buffer( 192, 256, (u32 *)gfx3d_convertedScreen );
*/		
		
		highreso_buffer.discard_data();		// バッファを書き込み専用に設定
		downscaled_buffer.discard_data();
		
		concurrency::parallel_for_each
		(
			highreso_buffer.extent.tile<RENDER_MAGNIFICATION, RENDER_MAGNIFICATION>(),
			[=](concurrency::tiled_index<RENDER_MAGNIFICATION, RENDER_MAGNIFICATION> index) restrict(amp)
			{
				u32 source_color = source_buffer2[index.global];
				
				highreso_buffer[ ( (192 * RENDER_MAGNIFICATION) - 1 ) - index.global[0] ][ index.global[1] ] = source_color;
				
				
				// Bilinear縮小処理
				tile_static u32 colors[4];
				const u32 combined_index = ( index.local[0] << 16 ) | index.local[1];
				
				switch(combined_index)
				{
					case 0:
						colors[0] = source_color;
						break;
					
					case (RENDER_MAGNIFICATION - 1):
						colors[1] = source_color;
						break;
					
					case ( (RENDER_MAGNIFICATION - 1) << 16 ):
						colors[2] = source_color;
						break;
					
					case ( ( (RENDER_MAGNIFICATION - 1) << 16 ) | (RENDER_MAGNIFICATION - 1) ):
						colors[3] = source_color;
						break;
				}
				
				index.barrier.wait();				// 処理範囲のデータ抽出が終わるのを待つ
				
				if(combined_index != 0) return;
				
				u32 a = 0;
				u32 r = 0;
				u32 g = 0;
				u32 b = 0;
				
				for each(u32 color in colors)
				{
					a += color >> 24;
					r += (color >> 16) & 0xFF;
					g += (color >> 8) & 0xFF;
					b += color & 0xFF;
				}
				
				// RGBA8888 → RGBA6665
				a >>= 5;		// (a / 4) >> 3
				r >>= 4;		// (a / 4) >> 2
				g >>= 4;
				b >>= 4;
				
				downscaled_buffer[ 191 - index.tile[0] ][ index.tile[1] ] = (a << 24) | (b << 16) | (g << 8) | r;
			}
		);
		
/*		highreso_buffer.synchronize();
		downscaled_buffer.synchronize();
*/		
/*		highreso_buffer.synchronize_async();
		downscaled_buffer.synchronize_async();
*/		
		downscaled_buffer.copy_to(downscaled_buffer2);
		highreso_buffer.copy_to(highreso_buffer2);
	}
	#endif
	
	
	#if 1
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::RenderFinish()
	{
		const unsigned int i = this->doubleBufferIndex;
		
		if( !this->gpuScreen3DHasNewData[i] )
			return OGLERROR_NOERR;
		
		
		// PBO + glMapBuffer
		#ifdef X432R_PROCESSTIME_CHECK
		timeCounter_3DFinish2.Start();
		#endif
		
//		glBindBuffer( GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i] );
//		const u32 * const mapped_buffer = (u32 *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, this->ref->pboRenderDataID[i] );
		const u32 * const mapped_buffer = (u32 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_ONLY);
		
		#ifdef X432R_PROCESSTIME_CHECK
		timeCounter_3DFinish2.Stop();
		#endif
		
		if(mapped_buffer != NULL)
			DownscaleFramebuffer<RENDER_MAGNIFICATION>(mapped_buffer);
		
//		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
//		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		
		this->gpuScreen3DHasNewData[i] = false;
		
		
		#ifdef X432R_CUSTOMRENDERER_DEBUG
		if(glFogEnabled && gfx3d.renderState.enableFogAlphaOnly)
			X432R::ShowDebugMessage("OpenGL FogAlphaOnly");
		
//		if(gfx3d.renderState.wbuffer)
//			ShowDebugMessage("OpenGL Depth: W-value");
		#endif
		
		
		return OGLERROR_NOERR;
	}
	#else
	static u32 temp3DBuffer[1024 * 768];
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::RenderFinish()
	{
		const unsigned int i = this->doubleBufferIndex;
		
		if( !this->gpuScreen3DHasNewData[i] )
			return OGLERROR_NOERR;
		
		
		// PBO + glGetBufferSubData
		#ifdef X432R_PROCESSTIME_CHECK
		timeCounter_3DFinish2.Start();
		#endif
		
//		glBindBuffer( GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i] );
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, this->ref->pboRenderDataID[i] );
		
		static const u32 buffer_size = sizeof(u32) * 256 * 192 * RENDER_MAGNIFICATION * RENDER_MAGNIFICATION;
		
		glGetBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, buffer_size, temp3DBuffer);
		
		#ifdef X432R_PROCESSTIME_CHECK
		timeCounter_3DFinish2.Stop();
		#endif
		
		DownscaleFramebuffer<RENDER_MAGNIFICATION>(temp3DBuffer);
		
//		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		
		
		this->gpuScreen3DHasNewData[i] = false;
		
		
		#ifdef X432R_CUSTOMRENDERER_DEBUG
		if(glFogEnabled && gfx3d.renderState.enableFogAlphaOnly)
			X432R::ShowDebugMessage("OpenGL FogAlphaOnly");
		
//		if(gfx3d.renderState.wbuffer)
//			ShowDebugMessage("OpenGL Depth: W-value");
		#endif
		
		
		return OGLERROR_NOERR;
	}
	#endif
	
	#ifdef X432R_OPENGL_FOG_ENABLED
	// Vertex Shader GLSL 1.00
	static const char *vertexShader_X432 = 
	{"\
		attribute vec4 inPosition; \n\
		attribute vec2 inTexCoord0; \n\
		attribute vec3 inColor; \n\
		\n\
		uniform float polyAlpha; \n\
		uniform vec2 texScale; \n\
		\n\
		varying vec4 vtxPosition; \n\
		varying vec2 vtxTexCoord; \n\
		varying vec4 vtxColor; \n\
		\n\
		uniform int oglWBuffer; \n\
		\n\
		uniform int fogEnabled; \n\
		uniform float fogOffset; \n\
		uniform float fogStep; \n\
		uniform float fogDensity[32]; \n\
		varying float fogFactor; \n\
		\n\
		void main() \n\
		{ \n\
			mat2 texScaleMtx = mat2( vec2(texScale.x, 0.0), vec2(0.0, texScale.y) ); \n\
			\n\
			vtxPosition = inPosition; \n\
			vtxTexCoord = texScaleMtx * inTexCoord0; \n\
			vtxColor = vec4(inColor * 4.0, polyAlpha); \n\
			\n\
			gl_Position = vtxPosition; \n\
			\n\
			\n\
			if(fogEnabled == 0) \n\
			{ \n\
				fogFactor = 1.0; \n\
			} \n\
			else \n\
			{ \n\
				float z; \n\
				if(oglWBuffer == 1) \n\
				{ \n\
					z = vtxPosition.w / 4096.0; \n\
				} \n\
				else \n\
				{ \n\
//					z = ( (vtxPosition.z / vtxPosition.w) * 0.5 ) + 0.5; \n\
					z = ( vtxPosition.z / (vtxPosition.w * 2.0) ) + 0.5; \n\
				} \n\
				\n\
				if(z <= fogOffset) \n\
				{ \n\
					fogFactor = fogDensity[0]; \n\
				} \n\
				else if(fogStep == 0.0) \n\
				{ \n\
					fogFactor = fogDensity[31]; \n\
				} \n\
				else \n\
				{ \n\
					float fog_index = (z - fogOffset) / fogStep; \n\
					fog_index = clamp(fog_index, 0.0, 31.0); \n\
					fogFactor = fogDensity[ int(fog_index) ]; \n\
				} \n\
			} \n\
		} \n\
	"};
	
	// Fragment Shader GLSL 1.00
	static const char *fragmentShader_X432 =
	{"\
		varying vec4 vtxPosition; \n\
		varying vec2 vtxTexCoord; \n\
		varying vec4 vtxColor; \n\
		\n\
		uniform sampler2D texMainRender; \n\
		uniform sampler1D texToonTable; \n\
		uniform int polyID; \n\
		uniform bool hasTexture; \n\
		uniform int polygonMode; \n\
		uniform int toonShadingMode; \n\
		uniform int oglWBuffer; \n\
		uniform bool enableAlphaTest; \n\
		uniform float alphaTestRef; \n\
		\n\
//		uniform int fogAlphaOnly; \n\
		varying float fogFactor; \n\
		\n\
		void main() \n\
		{ \n\
			vec4 texColor = vec4(1.0, 1.0, 1.0, 1.0); \n\
			vec4 fragColor; \n\
			\n\
			if(hasTexture) \n\
			{ \n\
				texColor = texture2D(texMainRender, vtxTexCoord); \n\
			} \n\
			\n\
			fragColor = texColor; \n\
			\n\
			if(polygonMode == 0) \n\
			{ \n\
				fragColor = vtxColor * texColor; \n\
			} \n\
			else if(polygonMode == 1) \n\
			{ \n\
				if( (texColor.a == 0.0) || !hasTexture ) \n\
				{ \n\
					fragColor.rgb = vtxColor.rgb; \n\
				} \n\
				else if(texColor.a == 1.0) \n\
				{ \n\
					fragColor.rgb = texColor.rgb; \n\
				} \n\
				else \n\
				{ \n\
					fragColor.rgb = (texColor.rgb * texColor.a) + ( vtxColor.rgb * (1.0 - texColor.a) ); \n\
				} \n\
				\n\
				fragColor.a = vtxColor.a; \n\
			} \n\
			else if(polygonMode == 2) \n\
			{ \n\
				if(toonShadingMode == 0) \n\
				{ \n\
					vec3 toonColor = vec3(texture1D(texToonTable, vtxColor.r).rgb); \n\
					fragColor.rgb = texColor.rgb * toonColor.rgb;\n\
					fragColor.a = texColor.a * vtxColor.a;\n\
				} \n\
				else \n\
				{ \n\
					vec3 toonColor = vec3(texture1D(texToonTable, vtxColor.r).rgb); \n\
					fragColor.rgb = texColor.rgb * vtxColor.rgb + toonColor.rgb; \n\
					fragColor.a = texColor.a * vtxColor.a; \n\
				} \n\
			} \n\
			else if(polygonMode == 3) \n\
			{ \n\
				if(polyID != 0) \n\
				{ \n\
					fragColor = vtxColor; \n\
				} \n\
			} \n\
			\n\
			if( (fragColor.a == 0.0) || ( enableAlphaTest && (fragColor.a < alphaTestRef) ) ) \n\
			{ \n\
				discard; \n\
			} \n\
			\n\
			if(oglWBuffer == 1) \n\
			{ \n\
				// The w component is in 1.12 format, normalize it to [-1;+1] \n\
//				gl_FragDepth = ( (vtxPosition.w / 4096.0) * 0.5 ) + 0.5; \n\
//				gl_FragDepth = (vtxPosition.w / 8192.0) + 0.5; \n\
				gl_FragDepth = vtxPosition.w / 4096.0; \n\
			} \n\
			else \n\
			{ \n\
//				gl_FragDepth = ( (vtxPosition.z / vtxPosition.w) * 0.5 ) + 0.5; \n\
				gl_FragDepth = ( vtxPosition.z / (vtxPosition.w * 2.0) ) + 0.5; \n\
			} \n\
			\n\
			if(fogFactor >= 1.0) \n\
			{ \n\
				gl_FragColor = fragColor; \n\
			} \n\
/*			else if(fogAlphaOnly == 1) \n\
			{ \n\
				gl_FragColor.rgb = fragColor.rgb; \n\
				gl_FragColor.a = mix(gl_Fog.color.a, fragColor.a, fogFactor); \n\
			} \n\
*/			else \n\
			{ \n\
//				gl_FragColor = mix(gl_Fog.color, fragColor, fogFactor); \n\
				gl_FragColor.rgb = mix(gl_Fog.color.rgb, fragColor.rgb, fogFactor); \n\
				gl_FragColor.a = fragColor.a; \n\
			} \n\
		} \n\
	"};
	#endif
	
	
	template <u32 RENDER_MAGNIFICATION>
	Render3DError OpenGLRenderer_X432::InitExtensions()
	{
		Render3DError error = OGLERROR_NOERR;
		OGLRenderRef &OGLRef = *this->ref;
		
		// Get OpenGL extensions
		std::set<std::string> oglExtensionSet;
		this->GetExtensionSet(&oglExtensionSet);
		
		// Initialize OpenGL
		this->InitTables();
		
		// Load and create shaders. Return on any error, since a v2.0 driver will assume that shaders are available.
		this->isShaderSupported	= true;
		
		std::string vertexShaderProgram;
		std::string fragmentShaderProgram;
		
		
		#ifndef X432R_OPENGL_FOG_ENABLED
		error = this->LoadShaderPrograms(&vertexShaderProgram, &fragmentShaderProgram);
		#else
		vertexShaderProgram = vertexShader_X432;
		fragmentShaderProgram = fragmentShader_X432;
		#endif
		
		
/*		if(error != OGLERROR_NOERR)
		{
			this->isShaderSupported = false;
			return error;
		}
*/		
		error = this->CreateShaders(&vertexShaderProgram, &fragmentShaderProgram);
		
		if(error != OGLERROR_NOERR)
		{
			this->isShaderSupported = false;
			return error;
		}
		
		
		#ifdef X432R_OPENGL_FOG_ENABLED
		uniformFogEnabled = glGetUniformLocation(OGLRef.shaderProgram, "fogEnabled");
//		uniformFogAlphaOnly = glGetUniformLocation(OGLRef.shaderProgram, "fogAlphaOnly");
		uniformFogOffset = glGetUniformLocation(OGLRef.shaderProgram, "fogOffset");
		uniformFogStep = glGetUniformLocation(OGLRef.shaderProgram, "fogStep");
		uniformFogDensity = glGetUniformLocation(OGLRef.shaderProgram, "fogDensity");
		#endif
		
		
		this->CreateToonTable();
		
		this->isVBOSupported = true;
		this->CreateVBOs();
		
#if	!defined(GL_ARB_pixel_buffer_object) && !defined(GL_EXT_pixel_buffer_object)
		this->isPBOSupported = false;
#else
		this->isPBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
								 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
								  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_pixel_buffer_object"));
#endif
		if(this->isPBOSupported)
		{
			this->CreatePBOs<RENDER_MAGNIFICATION>();
		}
	
#if !defined(GL_ARB_vertex_array_object) && !defined(GL_APPLE_vertex_array_object)
		this->isVAOSupported = false;
#else
		this->isVAOSupported	= this->isShaderSupported &&
								  this->isVBOSupported &&
								 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_array_object") ||
								  this->IsExtensionPresent(&oglExtensionSet, "GL_APPLE_vertex_array_object"));
#endif
		if(this->isVAOSupported)
		{
			this->CreateVAOs();
		}
	
		// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)		|| \
	!defined(GL_EXT_framebuffer_blit)		|| \
	!defined(GL_EXT_packed_depth_stencil)
		
		this->isFBOSupported = false;
#else
		this->isFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
								  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
								  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil");
#endif
		if(this->isFBOSupported)
		{
			error = this->CreateFBOs<RENDER_MAGNIFICATION>();
			if(error != OGLERROR_NOERR)
			{
				OGLRef.fboFinalOutputID = 0;
				this->isFBOSupported = false;
			}
		}
		else
		{
			OGLRef.fboFinalOutputID = 0;
			INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
		}
		
		// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)			|| \
	!defined(GL_EXT_framebuffer_multisample)	|| \
	!defined(GL_EXT_framebuffer_blit)			|| \
	!defined(GL_EXT_packed_depth_stencil)
		
		this->isMultisampledFBOSupported = false;
#else
		this->isMultisampledFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
											  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
											  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil") &&
											  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
#endif
		if(this->isMultisampledFBOSupported)
		{
			error = this->CreateMultisampledFBO<RENDER_MAGNIFICATION>();
			
			if(error != OGLERROR_NOERR)
			{
				OGLRef.selectedRenderingFBO = 0;
				this->isMultisampledFBOSupported = false;
			}
		}
		else
		{
			OGLRef.selectedRenderingFBO = 0;
			INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
		}
		
		this->InitTextures();
		this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
		
		
		if( !isMultisampledFBOSupported )
		{
			CommonSettings.GFX3D_Renderer_Multisample = false;
			#ifdef X432R_LOWQUALITYMODE_TEST
			lowQualityMsaaEnabled = false;
			#endif
		}
		
		return OGLERROR_NOERR;
	}
	
	
	GPU3DInterface gpu3Dgl_X2 =
	{
		"OpenGL X2 (512x384)",
		X432R::OGLInit<2>,
		OGLReset,
		OGLClose,
		X432R::OGLRender<2>,
		X432R::OGLRenderFinish<2>,
		OGLVramReconfigureSignal
	};
	
	GPU3DInterface gpu3Dgl_X3 =
	{
		"OpenGL X3 (768x576)",
		X432R::OGLInit<3>,
		OGLReset,
		OGLClose,
		X432R::OGLRender<3>,
		X432R::OGLRenderFinish<3>,
		OGLVramReconfigureSignal
	};
	
	GPU3DInterface gpu3Dgl_X4 =
	{
		"OpenGL X4 (1024x768)",
		X432R::OGLInit<4>,
		OGLReset,
		OGLClose,
		X432R::OGLRender<4>,
		X432R::OGLRenderFinish<4>,
		OGLVramReconfigureSignal
	};
}

#endif
