#include "stdafx.h"
#include "CShaderGL.h"

#ifdef SHADER_VERSION_TEST
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Utilities/Log/muConsoleDebug.h"

bool g_EnableShaderVersionTest = false;
extern bool TextureEnable;
extern bool AlphaTestEnable;

bool IsShaderProgramReady()
{
	return gShaderGL->CheckedShader();
}

CShaderGL::CShaderGL()
{
	shader_id = 0;
	m_projection = glm::mat4(1.0f);
	has_projection = false;
	u_projection = -1;
	u_view = -1;
	u_model = -1;
	u_texture1 = -1;
	u_useTexture = -1;
	u_alphaTextureMode = -1;
	u_alphaTestEnabled = -1;
	u_alphaTestRef = -1;
	u_fogEnabled = -1;
	u_fogColor = -1;
	u_fogStart = -1;
	u_fogEnd = -1;
}

CShaderGL::~CShaderGL()
{
	if (shader_id != 0)
	{
		glDeleteProgram(shader_id);
	}
}

void CShaderGL::Init()
{
	std::string vertex_shader;
	bool vertex_ok = readshader("Shaders\\shader.vs", vertex_shader);

	if (!vertex_ok)
	{
		vertex_shader =
			"#version 330 core\n"
			"layout (location = 0) in vec3 aPos;\n"
			"layout (location = 1) in vec2 aTex;\n"
			"layout (location = 2) in vec4 aColor;\n"
			"uniform mat4 projection;\n"
			"uniform mat4 view;\n"
			"uniform mat4 model;\n"
			"out vec2 TexCoord;\n"
			"out vec4 VertColor;\n"
			"out float FogCoord;\n"
			"void main(){\n"
			"  TexCoord = aTex;\n"
			"  VertColor = aColor;\n"
			"  vec4 eyePos = view * model * vec4(aPos, 1.0);\n"
			"  FogCoord = abs(eyePos.z);\n"
			"  gl_Position = projection * eyePos;\n"
			"}\n";
	}

	std::string frgmen_shader;
	bool fragment_ok = readshader("Shaders\\shader.fs", frgmen_shader);

	if (!fragment_ok)
	{
		frgmen_shader =
			"#version 330 core\n"
			"in vec2 TexCoord;\n"
			"in vec4 VertColor;\n"
			"in float FogCoord;\n"
			"out vec4 FragColor;\n"
			"uniform sampler2D texture1;\n"
			"uniform int useTexture;\n"
			"uniform int alphaTextureMode;\n"
			"uniform int alphaTestEnabled;\n"
			"uniform float alphaTestRef;\n"
			"uniform int fogEnabled;\n"
			"uniform vec4 fogColor;\n"
			"uniform float fogStart;\n"
			"uniform float fogEnd;\n"
			"void main(){\n"
			"  vec4 tex = (useTexture != 0) ? texture(texture1, TexCoord) : vec4(1.0);\n"
			"  if (alphaTextureMode != 0) tex = vec4(1.0, 1.0, 1.0, tex.a);\n"
			"  FragColor = tex * VertColor;\n"
			"  if (alphaTestEnabled != 0 && FragColor.a <= alphaTestRef) discard;\n"
			"  if (fogEnabled != 0) {\n"
			"    float denom = max(fogEnd - fogStart, 0.0001);\n"
			"    float fogFactor = clamp((fogEnd - FogCoord) / denom, 0.0, 1.0);\n"
			"    FragColor = mix(fogColor, FragColor, fogFactor);\n"
			"  }\n"
			"}\n";
	}

	GLuint shader_vertex = run_shader(vertex_shader.data(), GL_VERTEX_SHADER);

	GLuint shader_frgmen = run_shader(frgmen_shader.data(), GL_FRAGMENT_SHADER);

	if (shader_vertex == 0 || shader_frgmen == 0)
	{
		if (shader_vertex != 0) glDeleteShader(shader_vertex);
		if (shader_frgmen != 0) glDeleteShader(shader_frgmen);
		return;
	}

	shader_id = glCreateProgram();
	glAttachShader(shader_id, shader_vertex);
	glAttachShader(shader_id, shader_frgmen);
	glLinkProgram(shader_id);

	int success;
	glGetProgramiv(shader_id, GL_LINK_STATUS, &success);

	if (!success)
	{
		char infoLog[512];
		glGetProgramInfoLog(shader_id, 512, NULL, infoLog);
		g_ConsoleDebug->Write(5, "Error al enlazar el Shader Program:");
		g_ConsoleDebug->Write(5, infoLog);
		glDeleteProgram(shader_id);
		shader_id = 0;
	}
	else
	{
		CacheUniformLocations();
	}

	// Eliminar los shaders compilados
	glDeleteShader(shader_vertex);
	glDeleteShader(shader_frgmen);
}

void CShaderGL::RenderShader()
{
	if (this->CheckedShader())
	{
		glUseProgram(shader_id);
	}
}

bool CShaderGL::CheckedShader()
{
	return (shader_id != 0);
}

GLuint CShaderGL::GetShaderId()
{
	return shader_id;
}

bool CShaderGL::readshader(const char* filename, std::string& shader_text)
{
	FILE* compressedFile = fopen(filename, "rb");

	if (compressedFile)
	{
		fseek(compressedFile, 0, SEEK_END);
		long fileSize = ftell(compressedFile);
		fseek(compressedFile, 0, SEEK_SET);

		shader_text.resize(fileSize, 0);
		fread(shader_text.data(), 1, fileSize, compressedFile);
		fclose(compressedFile);

		return true;
	}

	return false;
}

GLuint CShaderGL::run_shader(const char* shader_text, GLenum type)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &shader_text, NULL);
	glCompileShader(shader);

	// Verificar errores de compilaci�n
	int success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		g_ConsoleDebug->Write(5, "Error al compilar shader:");
		g_ConsoleDebug->Write(5, infoLog);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

void CShaderGL::CacheUniformLocations()
{
	if (shader_id == 0)
	{
		u_projection = -1;
		u_view = -1;
		u_model = -1;
		u_texture1 = -1;
		u_useTexture = -1;
		u_alphaTextureMode = -1;
		u_alphaTestEnabled = -1;
		u_alphaTestRef = -1;
		u_fogEnabled = -1;
		u_fogColor = -1;
		u_fogStart = -1;
		u_fogEnd = -1;
		return;
	}

	u_projection = glGetUniformLocation(shader_id, "projection");
	u_view = glGetUniformLocation(shader_id, "view");
	u_model = glGetUniformLocation(shader_id, "model");
	u_texture1 = glGetUniformLocation(shader_id, "texture1");
	u_useTexture = glGetUniformLocation(shader_id, "useTexture");
	u_alphaTextureMode = glGetUniformLocation(shader_id, "alphaTextureMode");
	u_alphaTestEnabled = glGetUniformLocation(shader_id, "alphaTestEnabled");
	u_alphaTestRef = glGetUniformLocation(shader_id, "alphaTestRef");
	u_fogEnabled = glGetUniformLocation(shader_id, "fogEnabled");
	u_fogColor = glGetUniformLocation(shader_id, "fogColor");
	u_fogStart = glGetUniformLocation(shader_id, "fogStart");
	u_fogEnd = glGetUniformLocation(shader_id, "fogEnd");
}

void CShaderGL::run_projection()
{
	if (shader_id != 0)
	{
		glUseProgram(shader_id);

		float projection_raw[16] = { 0 };
		float modelview_raw[16] = { 0 };

		glGetFloatv(GL_PROJECTION_MATRIX, projection_raw);
		glGetFloatv(GL_MODELVIEW_MATRIX, modelview_raw);

		glm::mat4 projection = glm::make_mat4(projection_raw);
		glm::mat4 view = glm::make_mat4(modelview_raw);
		glm::mat4 model = glm::mat4(1.0f);

		if (u_projection != -1) glUniformMatrix4fv(u_projection, 1, GL_FALSE, glm::value_ptr(projection));
		if (u_view != -1) glUniformMatrix4fv(u_view, 1, GL_FALSE, glm::value_ptr(view));
		if (u_model != -1) glUniformMatrix4fv(u_model, 1, GL_FALSE, glm::value_ptr(model));

		if (u_texture1 != -1)
		{
			glUniform1i(u_texture1, 0);
		}

		if (u_useTexture != -1) glUniform1i(u_useTexture, TextureEnable ? 1 : 0);
		if (u_alphaTextureMode != -1) glUniform1i(u_alphaTextureMode, 0);
		if (u_alphaTestEnabled != -1) glUniform1i(u_alphaTestEnabled, AlphaTestEnable ? 1 : 0);

		if (u_alphaTestRef != -1)
		{
			float alpha_ref = 0.25f;
#ifdef GL_ALPHA_TEST_REF
			glGetFloatv(GL_ALPHA_TEST_REF, &alpha_ref);
#endif
			glUniform1f(u_alphaTestRef, alpha_ref);
		}

		if (u_fogEnabled != -1)
		{
			GLint fog_enabled = glIsEnabled(GL_FOG) ? 1 : 0;
			glUniform1i(u_fogEnabled, fog_enabled);
		}

		if (u_fogColor != -1)
		{
			float fog_color[4] = { 0.f, 0.f, 0.f, 0.f };
			glGetFloatv(GL_FOG_COLOR, fog_color);
			glUniform4f(u_fogColor, fog_color[0], fog_color[1], fog_color[2], fog_color[3]);
		}

		if (u_fogStart != -1)
		{
			float fog_start = 0.f;
			glGetFloatv(GL_FOG_START, &fog_start);
			glUniform1f(u_fogStart, fog_start);
		}

		if (u_fogEnd != -1)
		{
			float fog_end = 0.f;
			glGetFloatv(GL_FOG_END, &fog_end);
			glUniform1f(u_fogEnd, fog_end);
		}

		glUseProgram(0);
	}
}

void CShaderGL::SetAlphaTextureMode(int mode)
{
	if (shader_id != 0 && u_alphaTextureMode != -1)
	{
		glUniform1i(u_alphaTextureMode, mode);
	}
}

void CShaderGL::SetPerspective(float Fov, float Aspect, float ZNear, float ZFar)
{
	if (shader_id != 0)
	{
		glUseProgram(shader_id);
		glm::mat4 projection = glm::perspective(glm::radians(Fov), Aspect, ZNear, ZFar);
		m_projection = projection;
		has_projection = true;
		this->setMat4("projection", projection);
		glUseProgram(0);
	}
}

// Funciones para establecer uniforms
void CShaderGL::setBool(const char* name, bool value) const
{
	glUniform1i(glGetUniformLocation(shader_id, name), (int)value);
}

void CShaderGL::setInt(const char* name, int value) const
{
	glUniform1i(glGetUniformLocation(shader_id, name), value);
}

void CShaderGL::setFloat(const char* name, float value) const
{
	glUniform1f(glGetUniformLocation(shader_id, name), value);
}

void CShaderGL::setVec2(const char* name, float x, float y) const
{
	glUniform2f(glGetUniformLocation(shader_id, name), x, y);
}

void CShaderGL::setVec3(const char* name, float x, float y, float z) const
{
	glUniform3f(glGetUniformLocation(shader_id, name), x, y, z);
}

void CShaderGL::setVec4(const char* name, float x, float y, float z, float w) const
{
	glUniform4f(glGetUniformLocation(shader_id, name), x, y, z, w);
}

void CShaderGL::setMat4(const char* name, glm::mat4& matrix) const
{
	glUniformMatrix4fv(glGetUniformLocation(shader_id, name), 1, GL_FALSE, glm::value_ptr(matrix));
}

CShaderGL* CShaderGL::Instance()
{
	static CShaderGL sInstance;
	return &sInstance;
}
#endif // SHADER_VERSION_TEST



