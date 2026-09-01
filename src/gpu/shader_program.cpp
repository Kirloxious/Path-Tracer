#include "gpu/shader_program.h"

#include "core/log.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

ShaderProgram::~ShaderProgram() {
    if (ID) {
        glDeleteProgram(ID);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& o) noexcept
    : ID(o.ID), m_sources(std::move(o.m_sources)), m_includes(std::move(o.m_includes)), m_locationCache(std::move(o.m_locationCache)) {
    o.ID = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& o) noexcept {
    if (this != &o) {
        if (ID) {
            glDeleteProgram(ID);
        }
        ID = o.ID;
        m_sources = std::move(o.m_sources);
        m_includes = std::move(o.m_includes);
        m_locationCache = std::move(o.m_locationCache);
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

void ShaderProgram::setIVec2(const std::string& name, int x, int y) const {
    glUniform2i(getLocation(name), x, y);
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
    for (std::vector<Source>* list : {&m_sources, &m_includes}) {
        for (Source& src : *list) {
            std::error_code ec;
            const auto      t = std::filesystem::last_write_time(src.path, ec);
            if (!ec && t != src.writeTime) {
                src.writeTime = t; // advance even on failure so we don't spam errors
                anyChanged = true;
            }
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
    m_locationCache.clear(); // uniforms may have been added, removed, or relocated by the new program
    Log::info("Shader reloaded successfully");
    return true;
}

void ShaderProgram::trackSource(const std::filesystem::path& path) {
    std::error_code ec;
    m_sources.push_back({path, std::filesystem::last_write_time(path, ec)});
}

void ShaderProgram::recordIncludes(const std::unordered_set<std::string>& seen) {
    // `seen` holds weakly_canonical paths and also contains the entry point itself, while
    // m_sources holds the path exactly as the subclass passed it ("shader/foo.comp").
    // Compare canonicalized on both sides, or every entry point gets re-registered as one
    // of its own includes.
    auto alreadyTracked = [this](const std::filesystem::path& canonical) {
        auto same = [&canonical](const Source& s) {
            return std::filesystem::weakly_canonical(s.path) == canonical;
        };
        return std::any_of(m_sources.begin(), m_sources.end(), same) || std::any_of(m_includes.begin(), m_includes.end(), same);
    };

    for (const std::string& entry : seen) {
        const std::filesystem::path path(entry);
        if (alreadyTracked(path)) {
            continue;
        }
        std::error_code ec;
        m_includes.push_back({path, std::filesystem::last_write_time(path, ec)});
    }
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
    if (auto it = m_locationCache.find(name); it != m_locationCache.end()) {
        return it->second;
    }
    GLint loc = glGetUniformLocation(ID, name.c_str());
    m_locationCache.emplace(name, loc); // -1 caches the miss too; we only warn once per program-lifetime via the set below
    if (loc == -1) {
        static thread_local std::unordered_set<std::string> warned;
        if (warned.insert(sourcesLabel() + ":" + name).second) {
            Log::warn("Uniform '{}' not found in {} (unused or typo)", name, sourcesLabel());
        }
    }
    return loc;
}

std::string ShaderProgram::preprocessIncludes(const std::filesystem::path& path, std::unordered_set<std::string>& seen) {
    const auto canonical = std::filesystem::weakly_canonical(path).string();
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
            const auto child = path.parent_path() / m[1].str();
            out << "// >>> " << child.string() << "\n";
            out << preprocessIncludes(child, seen);
            out << "// <<< " << child.string() << "\n";
            // emit a #line so compile errors point at the right file:line
            out << "#line " << (lineno + 1) << "\n";
        } else {
            out << line << "\n";
        }
    }

    return out.str();
}

GLuint ShaderProgram::compileStage(GLenum stage, const std::filesystem::path& path) {
    std::unordered_set<std::string> seen;
    std::string                     source = preprocessIncludes(path, seen);
    // Register the headers we just walked even if the compile below fails — the whole point
    // is to notice the *next* edit to the header that broke it.
    recordIncludes(seen);
    if (source.empty()) {
        return 0;
    }
    if (source.find("#version") == std::string::npos) {
        Log::error("Shader has no #version directive: {}", path.string());
        return 0;
    }

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
