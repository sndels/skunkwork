#ifndef SKUNKWORK_SHADER_HPP
#define SKUNKWORK_SHADER_HPP

#include <GL/gl3w.h>
#include <string>
#include <sync.h>
#include <unordered_map>
#include <utility>
#include <vector>

enum class UniformType
{
    Bool,
    Float,
    Uint,
    Int,
    Vec2,
    Vec3,
    Vec4,
    UVec2,
    UVec3,
    UVec4,
    IVec2,
    IVec3,
    IVec4
};

struct Uniform
{
    UniformType type;
    union
    {
        bool b;
        float f[4];
        unsigned int u[4];
        int i[4];
    } value;
};

class Shader
{
    enum class Vendor
    {
        Nvidia,
        Intel,
        AMD,
        NotSupported
    };

  public:
    Shader(
        const std::string &name, sync_device *rocket,
        const std::string &vertPath, const std::string &fragPath,
        const std::string &geomPath = "");
    Shader(
        const std::string &name, sync_device *rocket,
        const std::string &compPath);

    ~Shader();

    Shader(const Shader &other) = delete;
    Shader(Shader &&other);
    Shader operator=(const Shader &other);

    void bind(double syncRow);
    bool reload();
    void setFloat(const std::string &name, GLfloat value);
    void setVec2(const std::string &name, GLfloat x, GLfloat y);
    GLint getUniformLocation(const std::string &name) const;
    std::unordered_map<std::string, Uniform> &dynamicUniforms();
    const std::string &name() const;

  private:
    void setVendor();
    // Strings by value because they come from the held vectors on reload() and
    // loadProgram() will clear the vectors
    GLuint loadProgram(
        std::string vertPath, std::string fragPath, std::string geomPath);
    // Strings by value because they come from the held vectors on reload() and
    // loadProgram() will clear the vectors
    GLuint loadProgram(std::string compPath);
    GLuint loadShader(const std::string &mainPath, GLenum shaderType);
    void collectUniforms(GLuint progID);
    std::string parseFromFile(const std::string &filePath, GLenum shaderType);
    void printProgramLog(GLuint program) const;
    void printShaderLog(GLuint shader) const;
    GLint getUniform(const std::string &name, UniformType type) const;
    void setUniform(const std::string &name, const Uniform &uniform);
    void setDynamicUniforms();
    void setRocketUniforms(double syncRow);

    GLuint _progID;
    Vendor _vendor;
    std::vector<std::string> _vertPaths;
    std::vector<std::string> _fragPaths;
    std::vector<std::string> _geomPaths;
    std::vector<std::string> _compPaths;
    std::vector<time_t> _vertMods;
    std::vector<time_t> _fragMods;
    std::vector<time_t> _geomMods;
    std::vector<time_t> _compMods;
    std::unordered_map<std::string, std::pair<UniformType, GLint>> _uniforms;
    std::unordered_map<std::string, Uniform> _dynamicUniforms;
    std::string _name;
    sync_device *_rocket;
    std::unordered_map<std::string, const sync_track *> _rocketUniforms;
};

#endif // SKUNKWORK_SHADER_HPP
