#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GL/gl.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

class ComputeShader
{
public:
    unsigned int ID;
    // ------------------------------------------------------------------------
    ComputeShader() {} // default constructor

    ComputeShader(const std::filesystem::path& path)
    {
        ID = loadShader(path);
    }

    uint32_t loadShader(const std::filesystem::path& path) {
        std::ifstream file(path);

        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << path.string() << std::endl;
        }
    
        std::ostringstream contentStream;
        contentStream << file.rdbuf();
        std::string shaderSource = contentStream.str();
    
        GLuint shaderHandle = glCreateShader(GL_COMPUTE_SHADER);
    
        const GLchar* source = (const GLchar*)shaderSource.c_str();
        glShaderSource(shaderHandle, 1, &source, 0);
    
        glCompileShader(shaderHandle);
    
        GLint isCompiled = 0;
        glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &maxLength);
    
            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(shaderHandle, maxLength, &maxLength, &infoLog[0]);
    
            std::cerr << infoLog.data() << std::endl;
    
            glDeleteShader(shaderHandle);
            return -1;
        }
    
        GLuint program = glCreateProgram();
        glAttachShader(program, shaderHandle);
        glLinkProgram(program);
    
        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
    
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
            
            std::cerr << infoLog.data() << std::endl;
    
            glDeleteProgram(program);
            glDeleteShader(shaderHandle);
            return -1;
        }
    
        glDetachShader(program, shaderHandle);
        return program;
    }

    // activate the shader
    // ------------------------------------------------------------------------
    void use() 
    { 
        glUseProgram(ID); 
    }
    // utility uniform functions
    // ------------------------------------------------------------------------
    void setBool(const std::string &name, bool value) const
    {         
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value); 
    }
    // ------------------------------------------------------------------------
    void setInt(const std::string &name, int value) const
    { 
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value); 
    }
    // ------------------------------------------------------------------------
    void setFloat(const std::string &name, float value) const
    { 
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value); 
    }

    void setVec3(const std::string &name, const glm::vec3 &value) const
    { 
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); 
    }

    void setVec2(const std::string &name, const glm::vec2 &value) const
    { 
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); 
    }

private:
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void checkCompileErrors(unsigned int shader, std::string type)
    {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};