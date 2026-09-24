#include "gpu/shader_program.h"

#include "core/log.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <regex>
#include <sstream>

namespace {
std::string infoLog(GLuint object, bool isProgram) {
    GLint length = 0;
    isProgram ? glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length) : glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    GLsizei     written = 0;
    isProgram ? glGetProgramInfoLog(object, length, &written, log.data()) : glGetShaderInfoLog(object, length, &written, log.data());
    log.resize(static_cast<std::size_t>(written));
    return log;
}
} // namespace

ShaderProgram::ShaderProgram(std::vector<Stage> stages) : m_stages(std::move(stages)) {
    for (const Stage& stage : m_stages) {
        watch(stage.path);
    }
    if (auto program = build()) {
        m_program = std::move(*program);
    } else {
        Log::error("Shader build failed ({}):\n{}", label(), program.error());
    }
}

void ShaderProgram::use() const {
    glUseProgram(id());
}

void ShaderProgram::setBool(std::string_view name, bool v) const {
    glUniform1i(getLocation(name), static_cast<int>(v));
}

void ShaderProgram::setInt(std::string_view name, int v) const {
    glUniform1i(getLocation(name), v);
}

void ShaderProgram::setUInt(std::string_view name, unsigned int v) const {
    glUniform1ui(getLocation(name), v);
}

void ShaderProgram::setFloat(std::string_view name, float v) const {
    glUniform1f(getLocation(name), v);
}

void ShaderProgram::setVec2(std::string_view name, const glm::vec2& v) const {
    glUniform2fv(getLocation(name), 1, &v[0]);
}

void ShaderProgram::setIVec2(std::string_view name, int x, int y) const {
    glUniform2i(getLocation(name), x, y);
}

void ShaderProgram::setVec3(std::string_view name, const glm::vec3& v) const {
    glUniform3fv(getLocation(name), 1, &v[0]);
}

void ShaderProgram::setMat4(std::string_view name, const glm::mat4& m) const {
    glUniformMatrix4fv(getLocation(name), 1, GL_FALSE, &m[0][0]);
}

bool ShaderProgram::reloadIfChanged() {
    bool anyChanged = false;
    for (WatchedFile& file : m_watched) {
        std::error_code ec;
        const auto      t = std::filesystem::last_write_time(file.path, ec);
        if (!ec && t != file.writeTime) {
            file.writeTime = t; // advance even on failure so we don't spam errors
            anyChanged = true;
        }
    }
    if (!anyChanged || m_stages.empty()) {
        return false;
    }

    Log::info("Reloading shader: {}", label());
    auto program = build();
    if (!program) {
        Log::error("Shader reload failed ({}), keeping the previous program:\n{}", label(), program.error());
        return false;
    }
    m_program = std::move(*program);
    m_locationCache.clear();
    Log::info("Shader reloaded successfully");
    return true;
}

void ShaderProgram::watch(const std::filesystem::path& path) {
    std::string canonical = std::filesystem::weakly_canonical(path).string();
    if (std::ranges::any_of(m_watched, [&](const WatchedFile& f) { return f.canonical == canonical; })) {
        return;
    }
    std::error_code ec;
    m_watched.push_back({path, std::move(canonical), std::filesystem::last_write_time(path, ec)});
}

std::string ShaderProgram::label() const {
    std::string out;
    for (const Stage& stage : m_stages) {
        if (!out.empty()) {
            out += " + ";
        }
        out += stage.path.filename().string();
    }
    return out;
}

GLint ShaderProgram::getLocation(std::string_view name) const {
    if (auto it = m_locationCache.find(name); it != m_locationCache.end()) {
        return it->second;
    }
    const std::string key(name);
    const GLint       loc = glGetUniformLocation(id(), key.c_str());
    m_locationCache.emplace(key, loc); // -1 caches the miss too
    if (loc == -1) {
        static thread_local std::unordered_set<std::string> warned;
        if (warned.insert(label() + ":" + key).second) {
            Log::warn("Uniform '{}' not found in {} (unused or typo)", key, label());
        }
    }
    return loc;
}

std::string ShaderProgram::preprocessIncludes(const std::filesystem::path& path, std::unordered_set<std::string>& seen) {
    if (!seen.insert(std::filesystem::weakly_canonical(path).string()).second) { // include guard
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

    static const std::regex inc(R"(^\s*#include\s+\"([^\"]+)\")");
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        ++lineno;
        std::smatch m;
        if (std::regex_search(line, m, inc)) {
            const auto child = path.parent_path() / m[1].str();
            out << "// >>> " << child.string() << "\n";
            out << preprocessIncludes(child, seen);
            out << "// <<< " << child.string() << "\n";
            // Compile errors then point at the right line of this file.
            out << "#line " << (lineno + 1) << "\n";
        } else {
            out << line << "\n";
        }
    }

    return out.str();
}

std::expected<ShaderHandle, std::string> ShaderProgram::compileStage(const Stage& stage) {
    std::unordered_set<std::string> seen;
    const std::string               source = preprocessIncludes(stage.path, seen);
    // Watch the headers even if the compile below fails — the point is to notice the *next*
    // edit to the header that broke it.
    for (const std::string& file : seen) {
        watch(file);
    }
    if (source.empty()) {
        return std::unexpected(std::format("{}: could not read source", stage.path.string()));
    }
    if (source.find("#version") == std::string::npos) {
        return std::unexpected(std::format("{}: no #version directive", stage.path.string()));
    }

    ShaderHandle  shader(glCreateShader(stage.type));
    const GLchar* src = source.c_str();
    glShaderSource(shader.get(), 1, &src, nullptr);
    glCompileShader(shader.get());

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        return std::unexpected(std::format("{}: compile error\n{}", stage.path.string(), infoLog(shader.get(), false)));
    }
    return shader;
}

std::expected<ProgramHandle, std::string> ShaderProgram::link(std::span<const ShaderHandle> stages) {
    ProgramHandle program(glCreateProgram());
    for (const ShaderHandle& s : stages) {
        glAttachShader(program.get(), s.get());
    }
    glLinkProgram(program.get());

    GLint linked = GL_FALSE;
    glGetProgramiv(program.get(), GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        return std::unexpected(std::format("link error\n{}", infoLog(program.get(), true)));
    }
    for (const ShaderHandle& s : stages) {
        glDetachShader(program.get(), s.get());
    }
    return program;
}

std::expected<ProgramHandle, std::string> ShaderProgram::build() {
    std::vector<ShaderHandle> compiled;
    compiled.reserve(m_stages.size());
    for (const Stage& stage : m_stages) {
        auto shader = compileStage(stage);
        if (!shader) {
            return std::unexpected(std::move(shader.error()));
        }
        compiled.push_back(std::move(*shader));
    }
    return link(compiled);
}
