#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>
#include <string>

#include "../Objects/MessageStack.h"

namespace ed
{
	namespace gl
	{
		GLuint CreateSimpleFramebuffer(GLint width, GLint height, GLuint& texColor, GLuint& texDepth);
		void FreeSimpleFramebuffer(GLuint& fbo, GLuint& color, GLuint& depth);

		GLuint CompileShader(GLenum type, const GLchar* str);
		bool CheckShaderCompilationStatus(GLuint shader, GLchar* msg);

		std::vector< MessageStack::Message > ParseMessages(const std::string& owner, int shader, const std::string& str);
		std::vector<MessageStack::Message> ParseHLSLMessages(const std::string& owner, int shader, const std::string& str);
	}
}