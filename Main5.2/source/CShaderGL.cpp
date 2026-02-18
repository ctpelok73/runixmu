#include "stdafx.h"
#include "CShaderGL.h"

#include <cmath>

#include "Utilities/Log/muConsoleDebug.h"

namespace
{
	const char* kVertexShader120 = R"GLSL(
#version 120

attribute vec3 aPos;
attribute vec2 aUV;
attribute vec4 aColor;

uniform mat4 uMVP;
uniform mat4 uModelView;

varying vec2 vUV;
varying vec4 vColor;
varying float vFogCoord;

void main()
{
	gl_Position = uMVP * vec4(aPos, 1.0);
	vUV = aUV;
	vColor = aColor;
	vec4 eyePos = uModelView * vec4(aPos, 1.0);
	vFogCoord = length(eyePos.xyz);
}
)GLSL";

	const char* kFragmentShader120 = R"GLSL(
#version 120

uniform sampler2D uTex;
uniform int uUseTex;
uniform int uAlphaTest;
uniform float uAlphaRef;
uniform vec4 uColorMul;
uniform int uFogEnable;
uniform vec3 uFogColor;
uniform float uFogEnd;
uniform float uFogScale;

varying vec2 vUV;
varying vec4 vColor;
varying float vFogCoord;

void main()
{
	vec4 tex = (uUseTex != 0) ? texture2D(uTex, vUV) : vec4(1.0);
	vec4 outColor = tex * vColor;
	outColor *= uColorMul;
	if (uAlphaTest != 0)
	{
		if (outColor.a <= uAlphaRef)
		{
			discard;
		}
	}
	if (uFogEnable != 0)
	{
		float fogFactor = clamp((uFogEnd - vFogCoord) * uFogScale, 0.0, 1.0);
		outColor.rgb = mix(uFogColor.rgb, outColor.rgb, fogFactor);
	}
	outColor = clamp(outColor, 0.0, 1.0);
	gl_FragColor = outColor;
}
)GLSL";

	inline float DegToRad(float deg)
	{
		return deg * 3.14159265358979323846f / 180.0f;
	}
}

CShaderGL::CShaderGL()
	: m_programId(0),
	  m_uMvpLoc(-1),
	  m_uTexLoc(-1),
	  m_uUseTexLoc(-1),
	  m_uAlphaTestLoc(-1),
	  m_uAlphaRefLoc(-1),
	  m_uColorMulLoc(-1),
	  m_uFogEnableLoc(-1),
	  m_uModelViewLoc(-1),
	  m_uFogColorLoc(-1),
	  m_uFogEndLoc(-1),
	  m_uFogScaleLoc(-1),
	  m_dirtyMvp(true)
{
	Mat4Identity(m_projection);
	Mat4Identity(m_view);
	Mat4Identity(m_modelView);
	Mat4Identity(m_mvp);
}

CShaderGL::~CShaderGL()
{
	Shutdown();
}

void CShaderGL::Init()
{
	if (m_programId != 0)
	{
		return;
	}

	std::string vsError;
	std::string fsError;
	std::string linkError;

	const GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader120, &vsError);
	const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader120, &fsError);

	if (vs == 0 || fs == 0)
	{
		if (!vsError.empty()) g_ConsoleDebug->Write(5, vsError.c_str());
		if (!fsError.empty()) g_ConsoleDebug->Write(5, fsError.c_str());
		if (vs) glDeleteShader(vs);
		if (fs) glDeleteShader(fs);
		return;
	}

	m_programId = LinkProgram(vs, fs, &linkError);
	glDeleteShader(vs);
	glDeleteShader(fs);

	if (m_programId == 0)
	{
		if (!linkError.empty()) g_ConsoleDebug->Write(5, linkError.c_str());
		return;
	}

	m_uMvpLoc = glGetUniformLocation(m_programId, "uMVP");
	m_uTexLoc = glGetUniformLocation(m_programId, "uTex");
	m_uUseTexLoc = glGetUniformLocation(m_programId, "uUseTex");
	m_uAlphaTestLoc = glGetUniformLocation(m_programId, "uAlphaTest");
	m_uAlphaRefLoc = glGetUniformLocation(m_programId, "uAlphaRef");
	m_uColorMulLoc = glGetUniformLocation(m_programId, "uColorMul");
	m_uFogEnableLoc = glGetUniformLocation(m_programId, "uFogEnable");
	m_uModelViewLoc = glGetUniformLocation(m_programId, "uModelView");
	m_uFogColorLoc = glGetUniformLocation(m_programId, "uFogColor");
	m_uFogEndLoc = glGetUniformLocation(m_programId, "uFogEnd");
	m_uFogScaleLoc = glGetUniformLocation(m_programId, "uFogScale");

	glUseProgram(m_programId);
	if (m_uTexLoc >= 0)
	{
		glUniform1i(m_uTexLoc, 0);
	}
	if (m_uUseTexLoc >= 0)
	{
		glUniform1i(m_uUseTexLoc, 1);
	}
	if (m_uAlphaTestLoc >= 0)
	{
		glUniform1i(m_uAlphaTestLoc, 0);
	}
	if (m_uAlphaRefLoc >= 0)
	{
		glUniform1f(m_uAlphaRefLoc, 0.25f);
	}
	if (m_uColorMulLoc >= 0)
	{
		glUniform4f(m_uColorMulLoc, 1.0f, 1.0f, 1.0f, 1.0f);
	}
	if (m_uFogEnableLoc >= 0)
	{
		glUniform1i(m_uFogEnableLoc, 0);
	}
	if (m_uFogColorLoc >= 0)
	{
		glUniform3f(m_uFogColorLoc, 0.0f, 0.0f, 0.0f);
	}
	if (m_uFogEndLoc >= 0)
	{
		glUniform1f(m_uFogEndLoc, 1.0f);
	}
	if (m_uFogScaleLoc >= 0)
	{
		glUniform1f(m_uFogScaleLoc, 1.0f);
	}
	glUseProgram(0);
}

void CShaderGL::Shutdown()
{
	if (m_programId != 0)
	{
		glDeleteProgram(m_programId);
		m_programId = 0;
	}

	m_uMvpLoc = -1;
	m_uTexLoc = -1;
	m_uUseTexLoc = -1;
	m_uAlphaTestLoc = -1;
	m_uAlphaRefLoc = -1;
	m_uColorMulLoc = -1;
	m_uFogEnableLoc = -1;
	m_uModelViewLoc = -1;
	m_uFogColorLoc = -1;
	m_uFogEndLoc = -1;
	m_uFogScaleLoc = -1;
	m_dirtyMvp = true;
}

bool CShaderGL::IsReady() const
{
	return m_programId != 0;
}

GLuint CShaderGL::GetProgramId() const
{
	return m_programId;
}

void CShaderGL::Use()
{
	if (m_programId != 0)
	{
		glUseProgram(m_programId);
	}
}

void CShaderGL::Unuse()
{
	glUseProgram(0);
}

void CShaderGL::SetPerspective(float fovDeg, float aspect, float zNear, float zFar)
{
	Mat4Perspective(m_projection, fovDeg, aspect, zNear, zFar);
	m_dirtyMvp = true;
}

void CShaderGL::SetViewFromCamera(const float* cameraPosition3, const float* cameraAngle3, bool cameraTopViewEnable)
{
	Mat4Identity(m_view);
	Mat4RotateY(m_view, cameraAngle3[1]);
	if (!cameraTopViewEnable)
	{
		Mat4RotateX(m_view, cameraAngle3[0]);
	}
	Mat4RotateZ(m_view, cameraAngle3[2]);
	Mat4Translate(m_view, -cameraPosition3[0], -cameraPosition3[1], -cameraPosition3[2]);

	m_dirtyMvp = true;
}

void CShaderGL::SetMVPFromOpenGL()
{
	if (m_programId == 0)
	{
		return;
	}

	float proj[16];
	float model[16];
	glGetFloatv(GL_PROJECTION_MATRIX, proj);
	glGetFloatv(GL_MODELVIEW_MATRIX, model);

	for (int i = 0; i < 16; i++)
	{
		m_modelView[i] = model[i];
	}

	Mat4Multiply(m_mvp, proj, model);
	m_dirtyMvp = false;
}

void CShaderGL::SetAlphaTestFromOpenGL()
{
	if (m_programId == 0)
	{
		return;
	}

	const GLboolean enabled = glIsEnabled(GL_ALPHA_TEST);
	if (m_uAlphaTestLoc >= 0)
	{
		glUniform1i(m_uAlphaTestLoc, enabled ? 1 : 0);
	}

	if (enabled && m_uAlphaRefLoc >= 0)
	{
		GLfloat ref = 0.0f;
		glGetFloatv(GL_ALPHA_TEST_REF, &ref);
		glUniform1f(m_uAlphaRefLoc, ref);
	}
}

void CShaderGL::SetColorMulFromOpenGL()
{
	if (m_programId == 0 || m_uColorMulLoc < 0)
	{
		return;
	}

	GLfloat c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glGetFloatv(GL_CURRENT_COLOR, c);
	glUniform4f(m_uColorMulLoc, c[0], c[1], c[2], c[3]);
}

void CShaderGL::SetColorMulIdentity()
{
	if (m_programId == 0 || m_uColorMulLoc < 0)
	{
		return;
	}
	glUniform4f(m_uColorMulLoc, 1.0f, 1.0f, 1.0f, 1.0f);
}

void CShaderGL::SetFogFromOpenGL()
{
	if (m_programId == 0 || m_uFogEnableLoc < 0)
	{
		return;
	}

	const GLboolean enabled = glIsEnabled(GL_FOG);
	glUniform1i(m_uFogEnableLoc, enabled ? 1 : 0);

	if (!enabled)
	{
		return;
	}

	if (m_uFogColorLoc >= 0)
	{
		GLfloat fogColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		glGetFloatv(GL_FOG_COLOR, fogColor);
		glUniform3f(m_uFogColorLoc, fogColor[0], fogColor[1], fogColor[2]);
	}

	GLfloat fogStart = 0.0f;
	GLfloat fogEnd = 1.0f;
	glGetFloatv(GL_FOG_START, &fogStart);
	glGetFloatv(GL_FOG_END, &fogEnd);

	float fogScale = 1.0f;
	const float fogRange = fogEnd - fogStart;
	if (std::fabs(fogRange) > 0.00001f)
	{
		fogScale = 1.0f / fogRange;
	}

	if (m_uFogEndLoc >= 0)
	{
		glUniform1f(m_uFogEndLoc, fogEnd);
	}
	if (m_uFogScaleLoc >= 0)
	{
		glUniform1f(m_uFogScaleLoc, fogScale);
	}
}

void CShaderGL::SetUseTexture(bool enable)
{
	if (m_programId == 0 || m_uUseTexLoc < 0)
	{
		return;
	}

	glUniform1i(m_uUseTexLoc, enable ? 1 : 0);
}

void CShaderGL::ApplyMVP()
{
	if (m_programId == 0)
	{
		return;
	}

	UpdateMVPIfDirty();

	if (m_uMvpLoc >= 0)
	{
		glUniformMatrix4fv(m_uMvpLoc, 1, GL_FALSE, m_mvp);
	}
	if (m_uModelViewLoc >= 0)
	{
		glUniformMatrix4fv(m_uModelViewLoc, 1, GL_FALSE, m_modelView);
	}
}

void CShaderGL::Mat4Identity(float* out16)
{
	for (int i = 0; i < 16; i++)
	{
		out16[i] = 0.0f;
	}
	out16[0] = 1.0f;
	out16[5] = 1.0f;
	out16[10] = 1.0f;
	out16[15] = 1.0f;
}

void CShaderGL::Mat4Multiply(float* out16, const float* a16, const float* b16)
{
	float r[16];
	for (int col = 0; col < 4; col++)
	{
		for (int row = 0; row < 4; row++)
		{
			r[col * 4 + row] =
				a16[0 * 4 + row] * b16[col * 4 + 0] +
				a16[1 * 4 + row] * b16[col * 4 + 1] +
				a16[2 * 4 + row] * b16[col * 4 + 2] +
				a16[3 * 4 + row] * b16[col * 4 + 3];
		}
	}
	for (int i = 0; i < 16; i++)
	{
		out16[i] = r[i];
	}
}

void CShaderGL::Mat4Perspective(float* out16, float fovDeg, float aspect, float zNear, float zFar)
{
	const float f = 1.0f / std::tanf(DegToRad(fovDeg) * 0.5f);

	for (int i = 0; i < 16; i++)
	{
		out16[i] = 0.0f;
	}

	out16[0] = f / aspect;
	out16[5] = f;
	out16[10] = (zFar + zNear) / (zNear - zFar);
	out16[11] = -1.0f;
	out16[14] = (2.0f * zFar * zNear) / (zNear - zFar);
}

void CShaderGL::Mat4Translate(float* inOut16, float tx, float ty, float tz)
{
	float t[16];
	Mat4Identity(t);
	t[12] = tx;
	t[13] = ty;
	t[14] = tz;

	Mat4Multiply(inOut16, inOut16, t);
}

void CShaderGL::Mat4RotateX(float* inOut16, float angleDeg)
{
	const float r = DegToRad(angleDeg);
	const float c = std::cosf(r);
	const float s = std::sinf(r);

	float m[16];
	Mat4Identity(m);
	m[5] = c;
	m[6] = s;
	m[9] = -s;
	m[10] = c;

	Mat4Multiply(inOut16, inOut16, m);
}

void CShaderGL::Mat4RotateY(float* inOut16, float angleDeg)
{
	const float r = DegToRad(angleDeg);
	const float c = std::cosf(r);
	const float s = std::sinf(r);

	float m[16];
	Mat4Identity(m);
	m[0] = c;
	m[2] = -s;
	m[8] = s;
	m[10] = c;

	Mat4Multiply(inOut16, inOut16, m);
}

void CShaderGL::Mat4RotateZ(float* inOut16, float angleDeg)
{
	const float r = DegToRad(angleDeg);
	const float c = std::cosf(r);
	const float s = std::sinf(r);

	float m[16];
	Mat4Identity(m);
	m[0] = c;
	m[1] = s;
	m[4] = -s;
	m[5] = c;

	Mat4Multiply(inOut16, inOut16, m);
}

GLuint CShaderGL::CompileShader(GLenum type, const char* source, std::string* outError)
{
	const GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == GL_TRUE)
	{
		return shader;
	}

	GLint logLen = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
	std::string log;
	log.resize((logLen > 1) ? (size_t)logLen : 1u, '\0');
	glGetShaderInfoLog(shader, logLen, NULL, log.data());
	glDeleteShader(shader);

	if (outError)
	{
		*outError = log;
	}

	return 0;
}

GLuint CShaderGL::LinkProgram(GLuint vs, GLuint fs, std::string* outError)
{
	const GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);

	glBindAttribLocation(program, 0, "aPos");
	glBindAttribLocation(program, 1, "aUV");
	glBindAttribLocation(program, 2, "aColor");

	glLinkProgram(program);

	GLint success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == GL_TRUE)
	{
		return program;
	}

	GLint logLen = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
	std::string log;
	log.resize((logLen > 1) ? (size_t)logLen : 1u, '\0');
	glGetProgramInfoLog(program, logLen, NULL, log.data());
	glDeleteProgram(program);

	if (outError)
	{
		*outError = log;
	}

	return 0;
}

void CShaderGL::UpdateMVPIfDirty()
{
	if (!m_dirtyMvp)
	{
		return;
	}

	Mat4Multiply(m_mvp, m_projection, m_view);
	m_dirtyMvp = false;
}

CShaderGL* CShaderGL::Instance()
{
	static CShaderGL sInstance;
	return &sInstance;
}
