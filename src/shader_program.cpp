#include "shader_program.h"

#include "log.h"

#include <fstream>
#include <regex>
#include <sstream>

ShaderProgram::~ShaderProgram() {
    if (ID) {
        glDeleteProgram(ID);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& o) noexcept : ID(o.ID), m_sources(std::move(o.m_sources)) {
    o.ID = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& o) noexcept {
    if (this != &o) {
        if (ID) {
            glDeleteProgram(ID);
        }
        ID = o.ID;
        m_sources = std::move(o.m_sources);
        o.ID = 0;
    }
    return *this;
}

void ShaderProgram::use() const {
    glUseProgram(ID);
}

void ShaderProgram::setBool(const std::string& name, bool v) const {
    glUniform1i(getLocation(name), static_cast<int>(v));
}

void ShaderProgram::setInt(const std::string& name, int v) const {
    glUniform1i(getLocation(name), v);
}

void ShaderProgram::setFloat(const std::string& name, float v) const {
    glUniform1f(getLocation(name), v);
}

void ShaderProgram::setVec2(const std::string& name, const glm::vec2& v) const {
    glUniform2fv(getLocation(name), 1, &v[0]);
}

void ShaderProgram::setVec3(const std::string& name, const glm::vec3& v) const {
    glUniform3fv(getLocation(name), 1, &v[0]);
}

void ShaderProgram::setMat4(const std::string& name, const glm::mat4& m) const {
    glUniformMatrix4fv(getLocation(name), 1, GL_FALSE, &m[0][0]);
}

bool ShaderProgram::reloadIfChanged() {
    if (m_sources.empty()) {
        return false;
    }

    bool anyChanged = false;
    for (auto& src : m_sources) {
        std::error_code ec;
        auto            t = std::filesystem::last_write_time(src.path, ec);
        if (!ec && t != src.writeTime) {
            src.writeTime = t; // advance even on failure so we don't spam errors
            anyChanged = true;
        }
    }

    if (!anyChanged) {
        return false;
    }

    Log::info("Reloading shader: {}", sourcesLabel());
    GLuint newProgram = buildProgram();
    if (newProgram == 0) {
        return false;
    }

    if (ID) {
        glDeleteProgram(ID);
    }
    ID = newProgram;
    Log::info("Shader reloaded successfully");
    return true;
}

void ShaderProgram::trackSource(const std::filesystem::path& path) {
    std::error_code ec;
    m_sources.push_back({path, std::filesystem::last_write_time(path, ec)});
}

std::string ShaderProgram::sourcesLabel() const {
    std::string out;
    for (size_t i = 0; i < m_sources.size(); ++i) {
        if (i) {
            out += " + ";
        }
        out += m_sources[i].path.filename().string();
    }
    return out;
}

GLint ShaderProgram::getLocation(const std::string& name) const {
    GLint loc = glGetUniformLocation(ID, name.c_str());
    if (loc == -1) {
        static thread_local std::unordered_set<std::string> warned;
        if (warned.insert(sourcesLabel() + ":" + name).second) {
            Log::warn("Uniform '{}' not found in {} (unused or typo)", name, sourcesLabel());
        }
    }
    return loc;
}

std::string ShaderProgram::preprocessIncludes(const std::filesystem::path& path, std::unordered_set<std::string>& seen) {
    Log::info("Prepocessing: {}", path.string());
    auto canonical = std::filesystem::weakly_canonical(path).string();
    if (!seen.insert(canonical).second) { // include guard
        return {};
    }

    std::ifstream f(path);
    if (!f.is_open()) {
        Log::error("Failed to open shader: {}", path.string());
        return {};
    }
    std::stringstream out;
    std::string       line;
    int               lineno = 0;

    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        ++lineno;
        // Match: #include "relative/path.glsl"
        static const std::regex inc(R"(^\s*#include\s+\"([^\"]+)\")");
        std::smatch             m;
        if (std::regex_search(line, m, inc)) {
            auto child = path.parent_path() / m[1].str();
            Log::info("include: '{}' -> '{}'", line, m[1].str());
            out << "// >>> " << child.string() << "\n";
            out << preprocessIncludes(child, seen);
            out << "// <<< " << child.string() << "\n";
            // emit a #line so compile errors point at the right file:line
            out << "#line " << (lineno + 1) << "\n";
            Log::info("Shader: Prepocessed {} for {}", m.str(), path.string());
        } else {
            out << line << "\n";
        }
    }

    return out.str();
}

GLuint ShaderProgram::compileStage(GLenum stage, const std::filesystem::path& path) {
    std::unordered_set<std::string> seen;
    std::string                     source = preprocessIncludes(path, seen);
    if (source.empty()) {
        return 0;
    }
    if (source.find("#version") == std::string::npos) {
        Log::error("Shader has no #version directive: {}", path.string());
        return 0;
    }

    Log::info("compileStage: {} (stage = {})", path.string(), stage);
    // Log::info("Source string: {}", source);
    const GLchar* src = source.c_str();
    GLuint        shader = glCreateShader(stage);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(static_cast<std::size_t>(maxLength));
        glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());
        Log::error("Shader compile error ({}):\n{}", path.string(), infoLog.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint ShaderProgram::linkProgram(const std::vector<GLuint>& stages) {
    GLuint program = glCreateProgram();
    for (GLuint s : stages) {
        glAttachShader(program, s);
    }
    glLinkProgram(program);

    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(static_cast<std::size_t>(maxLength));
        glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
        Log::error("Program link error:\n{}", infoLog.data());
        glDeleteProgram(program);
        for (GLuint s : stages) {
            glDeleteShader(s);
        }
        return 0;
    }

    for (GLuint s : stages) {
        glDetachShader(program, s);
        glDeleteShader(s);
    }
    return program;
}
