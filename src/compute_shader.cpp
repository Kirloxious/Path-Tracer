#include "compute_shader.h"

ComputeShader::ComputeShader(const std::filesystem::path& path) {
    trackSource(path);
    ID = buildProgram();
}

GLuint ComputeShader::buildProgram() const {
    if (m_sources.empty()) {
        return 0;
    }
    GLuint cs = compileStage(GL_COMPUTE_SHADER, m_sources[0].path);
    if (!cs) {
        return 0;
    }
    return linkProgram({cs});
}
