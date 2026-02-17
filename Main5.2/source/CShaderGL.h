#pragma once

#include <string>

class CShaderGL
{
public:
	CShaderGL();
	virtual ~CShaderGL();

	void Init();
	void Shutdown();

	bool IsReady() const;
	GLuint GetProgramId() const;

	void Use();
	void Unuse();

	void SetPerspective(float fovDeg, float aspect, float zNear, float zFar);
	void SetViewFromCamera(const float* cameraPosition3, const float* cameraAngle3, bool cameraTopViewEnable);
	void SetMVPFromOpenGL();
	void SetAlphaTestFromOpenGL();
	void SetColorMulFromOpenGL();
	void SetColorMulIdentity();
	void SetFogFromOpenGL();
	void SetUseTexture(bool enable);

	void ApplyMVP();

	static CShaderGL* Instance();

private:
	static void Mat4Identity(float* out16);
	static void Mat4Multiply(float* out16, const float* a16, const float* b16);
	static void Mat4Perspective(float* out16, float fovDeg, float aspect, float zNear, float zFar);
	static void Mat4Translate(float* inOut16, float tx, float ty, float tz);
	static void Mat4RotateX(float* inOut16, float angleDeg);
	static void Mat4RotateY(float* inOut16, float angleDeg);
	static void Mat4RotateZ(float* inOut16, float angleDeg);

	static GLuint CompileShader(GLenum type, const char* source, std::string* outError);
	static GLuint LinkProgram(GLuint vs, GLuint fs, std::string* outError);

	void UpdateMVPIfDirty();

private:
	GLuint m_programId;
	GLint m_uMvpLoc;
	GLint m_uTexLoc;
	GLint m_uUseTexLoc;
	GLint m_uAlphaTestLoc;
	GLint m_uAlphaRefLoc;
	GLint m_uColorMulLoc;
	GLint m_uFogEnableLoc;

	bool m_dirtyMvp;
	float m_projection[16];
	float m_view[16];
	float m_mvp[16];
};

#define gShaderGL (CShaderGL::Instance())
