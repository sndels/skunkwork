#include "shader.hpp"

#include "error.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stack>
#include <sys/stat.h>

namespace
{
// Get last time modified for file
time_t getMod(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
    {
        reportError("[shader] stat failed for " + path);
        return (time_t)-1;
    }
    return sb.st_mtime;
}

std::string toString(UniformType type)
{
    switch (type)
    {
    case UniformType::Bool:
        return "bool";
    case UniformType::Float:
        return "float";
    case UniformType::Vec2:
        return "vec2";
    case UniformType::Vec3:
        return "vec3";
    default:
        return "toString(type) unimplemented";
    }
}
} // namespace

Shader::Shader(
    const std::string &name, sync_device *rocket, const std::string &vertPath,
    const std::string &fragPath, const std::string &geomPath)
: _progID(0)
, _name(name)
, _rocket(rocket)
{
    setVendor();
    GLuint progID = loadProgram(vertPath, fragPath, geomPath);
    if (progID != 0)
        _progID = progID;
}

Shader::Shader(
    const std::string &name, sync_device *rocket, const std::string &compPath)
: _progID(0)
, _name(name)
, _rocket(rocket)
{
    setVendor();
    GLuint progID = loadProgram(compPath);
    if (progID != 0)
        _progID = progID;
}

Shader::~Shader() { glDeleteProgram(_progID); }

Shader::Shader(Shader &&other)
: _progID(other._progID)
, _vendor(other._vendor)
, _vertPaths(other._vertPaths)
, _fragPaths(other._fragPaths)
, _geomPaths(other._geomPaths)
, _compPaths(other._compPaths)
, _vertMods(other._vertMods)
, _fragMods(other._fragMods)
, _geomMods(other._geomMods)
, _compMods(other._compMods)
, _uniforms(other._uniforms)
, _dynamicUniforms(other._dynamicUniforms)
, _name(other._name)
, _rocket(other._rocket)
, _rocketUniforms(other._rocketUniforms)
{
    other._progID = 0;
}

void Shader::bind(double syncRow)
{
    glUseProgram(_progID);
    setDynamicUniforms();
    if (_rocket != nullptr)
        setRocketUniforms(syncRow);
}

bool Shader::reload()
{
    if (!_compPaths.empty())
    {
        for (auto i = 0u; i < _compPaths.size(); ++i)
        {
            std::string const &path = _compPaths[i];
            if (_compMods[i] != getMod(path))
            {
                // First path is the root file
                GLuint progID = loadProgram(_compPaths[0]);
                if (progID != 0)
                {
                    glDeleteProgram(_progID);
                    _progID = progID;
                    return false;
                }
                return true;
            }
        }
        return false;
    }

    auto fn = [&](std::vector<std::string> const &paths,
                  std::vector<time_t> const &mods) -> bool
    {
        for (auto i = 0u; i < paths.size(); ++i)
        {
            std::string const &path = paths[i];
            if (mods[i] != getMod(path))
            {
                // First path is the root file
                GLuint progID = loadProgram(
                    !_vertPaths.empty() ? _vertPaths[0] : "",
                    !_fragPaths.empty() ? _fragPaths[0] : "",
                    !_geomPaths.empty() ? _geomPaths[0] : "");
                if (progID != 0)
                {
                    glDeleteProgram(_progID);
                    _progID = progID;
                    return false;
                }
                return true;
            }
        }
        return false;
    };

    bool reloaded = fn(_fragPaths, _fragMods);
    reloaded |= fn(_vertPaths, _vertMods);
    reloaded |= fn(_geomPaths, _geomMods);

    return reloaded;
}

std::unordered_map<std::string, Uniform> &Shader::dynamicUniforms()
{
    return _dynamicUniforms;
}

const std::string &Shader::name() const { return _name; }

void Shader::setFloat(const std::string &name, GLfloat value)
{
    GLint location = getUniform(name, UniformType::Float);
    glUniform1f(location, value);
}

void Shader::setVec2(const std::string &name, GLfloat x, GLfloat y)
{
    GLint location = getUniform(name, UniformType::Vec2);
    GLfloat value[2] = {x, y};
    glUniform2fv(location, 1, value);
}

GLint Shader::getUniformLocation(const std::string &name) const
{
    GLint uniformLocation = glGetUniformLocation(_progID, name.c_str());
    if (uniformLocation == -1)
        reportError("[shader] '" + name + "' is not a valid shader variable");
    return uniformLocation;
}

void Shader::setVendor()
{
    const char *vendor = (const char *)glGetString(GL_VENDOR);
    if (strcmp(vendor, "NVIDIA Corporation") == 0)
        _vendor = Vendor::Nvidia;
    else if (strcmp(vendor, "Intel Inc.") == 0)
        _vendor = Vendor::Intel;
    else if (strcmp(vendor, "AMD") == 0)
        _vendor = Vendor::AMD;
    else
    {
        reportError(
            std::string(
                "[shader] Include aware error parsing not supported for ") +
            vendor);
        _vendor = Vendor::NotSupported;
    }
}

GLuint Shader::loadProgram(
    std::string vertPath, std::string fragPath, std::string geomPath)
{
    // These will be refilled by loadShader so at least the root shader should
    // always remain afterwards.
    _vertPaths.clear();
    _fragPaths.clear();
    _geomPaths.clear();
    _vertMods.clear();
    _fragMods.clear();
    _geomMods.clear();

    // Get a program id
    GLuint progID = glCreateProgram();

    // Load and attacth shaders
    bool compileFailed = false;
    GLuint vertexShader = loadShader(vertPath, GL_VERTEX_SHADER);
    if (vertexShader == 0)
    {
        glDeleteProgram(progID);
        progID = 0;
        compileFailed = true;
    }
    else
        glAttachShader(progID, vertexShader);

    GLuint geometryShader = 0;
    if (!geomPath.empty())
    {
        geometryShader = loadShader(geomPath, GL_GEOMETRY_SHADER);
        if (geometryShader == 0)
        {
            glDeleteShader(vertexShader);
            glDeleteProgram(progID);
            progID = 0;
            compileFailed = true;
        }
        else
            glAttachShader(progID, geometryShader);
    }

    GLuint fragmentShader = loadShader(fragPath, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        glDeleteShader(geometryShader);
        glDeleteProgram(progID);
        progID = 0;
        compileFailed = true;
    }
    else
        glAttachShader(progID, fragmentShader);

    // Only return after trying all the shaders so that we refill tracking for
    // all of them even if the first couldn't compile
    if (compileFailed)
        return 0;

    // Link program
    glLinkProgram(progID);
    GLint programSuccess = GL_FALSE;
    glGetProgramiv(progID, GL_LINK_STATUS, &programSuccess);
    if (programSuccess == GL_FALSE)
    {
        reportError("[shader] Error linking program " + std::to_string(progID));
        reportError("[shader] Error code: " + std::to_string(programSuccess));
        printProgramLog(progID);
        glDeleteShader(vertexShader);
        glDeleteShader(geometryShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(progID);
        progID = 0;
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(geometryShader);
    glDeleteShader(fragmentShader);

    collectUniforms(progID);

    printf("[shader] Shader %s loaded\n", fragPath.c_str());

    return progID;
}

GLuint Shader::loadProgram(std::string compPath)
{
    // These will be refilled by loadShader so at least the root shader should
    // always remain afterwards.
    _compPaths.clear();
    _compMods.clear();

    // Get a program id
    GLuint progID = glCreateProgram();

    // Load and attacth shaders
    bool compileFailed = false;
    GLuint shader = loadShader(compPath, GL_COMPUTE_SHADER);
    if (shader == 0)
    {
        glDeleteProgram(progID);
        progID = 0;
        return false;
    }
    else
        glAttachShader(progID, shader);

    // Link program
    glLinkProgram(progID);
    GLint programSuccess = GL_FALSE;
    glGetProgramiv(progID, GL_LINK_STATUS, &programSuccess);
    if (programSuccess == GL_FALSE)
    {
        reportError("[shader] Error linking program " + std::to_string(progID));
        reportError("[shader] Error code: " + std::to_string(programSuccess));
        printProgramLog(progID);
        glDeleteShader(shader);
        glDeleteProgram(progID);
        progID = 0;
        return 0;
    }

    glDeleteShader(shader);

    collectUniforms(progID);

    printf("[shader] Shader %s loaded\n", compPath.c_str());

    return progID;
}

void Shader::collectUniforms(GLuint progID)
{
    // Query uniforms
    GLint uCount;
    glGetProgramiv(progID, GL_ACTIVE_UNIFORMS, &uCount);
    _uniforms.clear();
    for (GLint i = 0; i < uCount; ++i)
    {
        char name[64];
        GLenum glType;
        GLint size;
        glGetActiveUniform(progID, i, sizeof(name), NULL, &size, &glType, name);
        UniformType type;
        switch (glType)
        {
        case GL_BOOL:
            type = UniformType::Bool;
            break;
        case GL_FLOAT:
            type = UniformType::Float;
            break;
        case GL_UNSIGNED_INT:
            type = UniformType::Uint;
            break;
        case GL_INT:
            type = UniformType::Int;
            break;
        case GL_FLOAT_VEC2:
            type = UniformType::Vec2;
            break;
        case GL_FLOAT_VEC3:
            type = UniformType::Vec3;
            break;
        case GL_FLOAT_VEC4:
            type = UniformType::Vec4;
            break;
        case GL_UNSIGNED_INT_VEC2:
            type = UniformType::UVec2;
            break;
        case GL_UNSIGNED_INT_VEC3:
            type = UniformType::UVec3;
            break;
        case GL_UNSIGNED_INT_VEC4:
            type = UniformType::UVec4;
            break;
        case GL_INT_VEC2:
            type = UniformType::IVec2;
            break;
        case GL_INT_VEC3:
            type = UniformType::IVec3;
            break;
        case GL_INT_VEC4:
            type = UniformType::IVec4;
            break;
        default:
            reportError(
                "[shader] Unknown uniform type " + std::to_string(glType));
            break;
        }
        _uniforms.insert(
            {name, std::make_pair(type, glGetUniformLocation(progID, name))});
    }

    // Rebuild uniforms
    std::unordered_map<std::string, Uniform> newDynamics;
    std::unordered_map<std::string, const sync_track *> newRockets;
    for (auto &u : _uniforms)
    {
        std::string name = u.first;

        // Rocket uniforms use hungarian notation starting with 'r', e.g. rTime
        if (_rocket && name.length() > 1 && name[0] == 'r' && isupper(name[1]))
        {
            // Check type
            UniformType type = u.second.first;
            if (type != UniformType::Float)
            {
                reportError(
                    "[shader] '" + name +
                    "' should be float to use it with Rocket");
                continue;
            }
            // Add existing value if present
            if (auto existing = _rocketUniforms.find(name);
                existing != _rocketUniforms.end())
            {
                newRockets.insert(*existing);
                continue;
            }

            // Init new
            newRockets.insert(
                {name, sync_get_track(_rocket, (_name + ":" + name).c_str())});
        }

        // Skip uniforms not labelled as dynamic
        if (name.length() < 2 || name[0] != 'd' || !isupper(name[1]))
            continue;

        // Add existing value if types match
        if (auto existing = _dynamicUniforms.find(name);
            existing != _dynamicUniforms.end() &&
            existing->second.type == u.second.first)
        {
            newDynamics.insert(*existing);
            continue;
        }

        // Init new uniform
        Uniform uniform{
            .type = u.second.first,
        };
        int tmp = 0;
        switch (uniform.type)
        {
        case UniformType::Bool:
            glGetUniformiv(progID, u.second.second, &tmp);
            uniform.value.b = tmp == 1;
            break;
        case UniformType::Int:
        case UniformType::IVec2:
        case UniformType::IVec3:
        case UniformType::IVec4:
            glGetUniformiv(progID, u.second.second, uniform.value.i);
            break;
        case UniformType::Float:
        case UniformType::Vec2:
        case UniformType::Vec3:
        case UniformType::Vec4:
            glGetUniformfv(progID, u.second.second, uniform.value.f);
            break;
        case UniformType::Uint:
        case UniformType::UVec2:
        case UniformType::UVec3:
        case UniformType::UVec4:
            glGetUniformuiv(progID, u.second.second, uniform.value.u);
            break;
        default:
            reportError(
                "[shader] Unimplemented dynamic uniform of type " +
                toString(uniform.type));
            // uniform.value is left uninitialized
            break;
        }
        newDynamics.insert({u.first, uniform});
    }
    _dynamicUniforms = newDynamics;
    _rocketUniforms = newRockets;
}

GLuint Shader::loadShader(const std::string &mainPath, GLenum shaderType)
{
    GLuint shaderID = 0;
    std::string shaderStr = parseFromFile(mainPath, shaderType);
    if (!shaderStr.empty())
    {
        shaderID = glCreateShader(shaderType);
        const GLchar *shaderSource = shaderStr.c_str();
        glShaderSource(shaderID, 1, &shaderSource, NULL);
        glCompileShader(shaderID);
        GLint shaderCompiled = GL_FALSE;
        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &shaderCompiled);
        if (shaderCompiled == GL_FALSE)
        {
            reportError(
                "[shader] Unable to compile shader " +
                std::to_string(shaderID));
            reportError("[shader] " + mainPath);
            printShaderLog(shaderID);
            shaderID = 0;
        }
    }
    return shaderID;
}

std::string Shader::parseFromFile(
    const std::string &filePath, GLenum shaderType)
{
    std::ifstream sourceFile(filePath.c_str());
    std::string shaderStr;
    if (sourceFile)
    {
        // Push filepath and timestamp to vectors
        if (shaderType == GL_FRAGMENT_SHADER)
        {
            _fragPaths.emplace_back(filePath);
            _fragMods.emplace_back(getMod(filePath));
        }
        else if (shaderType == GL_VERTEX_SHADER)
        {
            _vertPaths.emplace_back(filePath);
            _vertMods.emplace_back(getMod(filePath));
        }
        else if (shaderType == GL_GEOMETRY_SHADER)
        {
            _geomPaths.emplace_back(filePath);
            _geomMods.emplace_back(getMod(filePath));
        }
        else if (shaderType == GL_COMPUTE_SHADER)
        {
            _compPaths.emplace_back(filePath);
            _compMods.emplace_back(getMod(filePath));
        }
        else
            assert(!"Unexpected shader type");

        // Get directory path for the file for possible includes
        std::string dirPath(filePath);
        dirPath.erase(dirPath.find_last_of('/') + 1);

        // Mark file start for error parsing
        shaderStr += "// File: " + filePath + '\n';

        // Parse lines
        for (std::string line; std::getline(sourceFile, line);)
        {
            // Handle recursive includes, expect correct syntax
            if (line.compare(0, 9, "#include ") == 0)
            {
                line.erase(0, 10);
                line.pop_back();
                line = parseFromFile(dirPath + line, shaderType);
                if (line.empty())
                    return "";
            }
            shaderStr += line + '\n';
        }

        // Mark file end for error parsing
        shaderStr += "// File: " + filePath + '\n';
    }
    else
    {
        reportError("[shader] Unable to open file " + filePath);
    }
    return shaderStr;
}

void Shader::printProgramLog(GLuint program) const
{
    if (glIsProgram(program) == GL_TRUE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        char *errorLog = new char[maxLength];
        glGetProgramInfoLog(program, maxLength, &maxLength, errorLog);
        reportError(errorLog);
        delete[] errorLog;
    }
    else
    {
        reportError(
            "[shader] ID " + std::to_string(program) + " is not a program");
    }
}

void Shader::printShaderLog(GLuint shader) const
{
    if (glIsShader(shader) == GL_TRUE)
    {
        // Get errors
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        char *errorLog = new char[maxLength];
        glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog);

        if (_vendor == Vendor::NotSupported)
        {
            reportError(errorLog);
            delete[] errorLog;
            return;
        }

        // Convert error string to a stream for easy line access
        std::istringstream errorStream(errorLog);

        // Get source string
        glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &maxLength);
        char *shaderStr = new char[maxLength];
        glGetShaderSource(shader, maxLength, &maxLength, shaderStr);

        // Set up vendor specific parsing
        std::string linePrefix;
        char lineNumCutoff;
        if (_vendor == Vendor::Nvidia)
        {
            linePrefix = "0(";
            lineNumCutoff = ')';
        }
        else if (_vendor == Vendor::Intel)
        {
            linePrefix = "ERROR: 0:";
            lineNumCutoff = ':';
        }
        else if (_vendor == Vendor::AMD)
        {
            linePrefix = "0:";
            lineNumCutoff = '(';
        }
        else
        {
            reportError("[shader] Unimplemented vendor parsing");
            reportError(errorLog);
            delete[] errorLog;
            delete[] shaderStr;
            return;
        }

        std::string lastFile;
        // Parse correct file and line numbers to errors
        for (std::string errLine; std::getline(errorStream, errLine);)
        {

            // Only parse if error points to a line
            if (errLine.compare(0, linePrefix.length(), linePrefix) == 0)
            {
                // Extract error line in parsed source
                auto lineNumEnd =
                    errLine.find(lineNumCutoff, linePrefix.length() + 1);
                uint32_t lineNum = std::stoi(
                    errLine.substr(linePrefix.length(), lineNumEnd - 1));

                std::stack<std::string> files;
                std::stack<uint32_t> lines;
                // Parse the source to error, track file and line in file
                std::istringstream sourceStream(shaderStr);
                for (auto i = 0u; i < lineNum; ++i)
                {
                    std::string srcLine;
                    std::getline(sourceStream, srcLine);
                    if (srcLine.compare(0, 9, "// File: ") == 0)
                    {
                        srcLine.erase(0, 9);
                        // If include-block ends, pop file and it's lines from
                        // stacks
                        if (!files.empty() && srcLine.compare(files.top()) == 0)
                        {
                            files.pop();
                            lines.pop();
                        }
                        else
                        { // Push new file block to stacks
                            files.push(srcLine);
                            lines.push(0);
                        }
                    }
                    else
                    {
                        ++lines.top();
                    }
                }

                // Insert the correct file:line_number to error and print
                errLine.erase(0, lineNumEnd);
                errLine.insert(
                    0, files.top() + ':' + std::to_string(lines.top()));
            }
            reportError(errLine);
        }
        reportError("");

        delete[] errorLog;
        delete[] shaderStr;
    }
    else
    {
        reportError(
            "[shader] ID " + std::to_string(shader) + " is not a shader");
    }
}

GLint Shader::getUniform(const std::string &name, UniformType type) const
{
    if (_progID == 0)
        return -1;

    auto uniform = _uniforms.find(name);
    if (uniform == _uniforms.end())
    {
        reportError("[shader] Uniform '" + name + "' not found");
        return -1;
    }

    auto [actualType, location] = uniform->second;
    if (type != actualType)
    {
        reportError(
            "[shader] Uniform '" + name + "' is not of type " + toString(type));
        return -1;
    }
    return location;
}

void Shader::setUniform(const std::string &name, const Uniform &uniform)
{
    GLint location = getUniform(name, uniform.type);
    switch (uniform.type)
    {
    case UniformType::Bool:
        glUniform1i(location, uniform.value.b);
        break;
    case UniformType::Float:
        glUniform1f(location, *uniform.value.f);
        break;
    case UniformType::Vec2:
        glUniform2fv(location, 1, uniform.value.f);
        break;
    case UniformType::Vec3:
        glUniform3fv(location, 1, uniform.value.f);
        break;
    case UniformType::Vec4:
        glUniform4fv(location, 1, uniform.value.f);
        break;
    case UniformType::Uint:
        glUniform1ui(location, *uniform.value.u);
        break;
    case UniformType::UVec2:
        glUniform2uiv(location, 1, uniform.value.u);
        break;
    case UniformType::UVec3:
        glUniform3uiv(location, 1, uniform.value.u);
        break;
    case UniformType::UVec4:
        glUniform4uiv(location, 1, uniform.value.u);
        break;
    case UniformType::Int:
        glUniform1i(location, *uniform.value.i);
        break;
    case UniformType::IVec2:
        glUniform2iv(location, 1, uniform.value.i);
        break;
    case UniformType::IVec3:
        glUniform3iv(location, 1, uniform.value.i);
        break;
    case UniformType::IVec4:
        glUniform4iv(location, 1, uniform.value.i);
        break;
    default:
        reportError(
            "[shader] Setting uniform of type '" + toString(uniform.type) +
            "' is unimplemented");
        break;
    }
}

void Shader::setDynamicUniforms()
{
    for (auto &u : _dynamicUniforms)
    {
        setUniform(u.first, u.second);
    }
}

void Shader::setRocketUniforms(double syncRow)
{
    for (auto &u : _rocketUniforms)
        setFloat(u.first, (GLfloat)sync_get_val(u.second, syncRow));
}
