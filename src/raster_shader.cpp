#include "raster_shader.h"

RasterShader::RasterShader(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath) {
    trackSource(vertPath);
    trackSource(fragPath);
    ID = buildProgram();
}

GLuint RasterShader::buildProgram() const {
    if (m_sources.size() < 2) {
        return 0;
    }
    GLuint vs = compileStage(GL_VERTEX_SHADER, m_sources[0].path);
    if (!vs) {
        return 0;
    }
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, m_sources[1].path);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }
    return linkProgram({vs, fs});
}
