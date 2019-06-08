#ifndef MARCHED_HPP
#define MARCHED_HPP

#include <functional>

#include <GL/gl3w.h>
#include <glm/glm.hpp>

class Marched
{
public:
    Marched(std::function<float(const glm::vec3& pos, const float time)> sdf);
    ~Marched();

    Marched(const Marched& other) = delete;
    Marched(Marched&& other);
    Marched operator=(const Marched& other) = delete;

    void render() const;
    void update(const glm::uvec3& res, const glm::vec3& min, const glm::vec3& max, const float time);

private:
    GLuint _vao;
    GLuint _vbo;
    GLuint _ibo;
    size_t _vertCount;
    std::function<float(const glm::vec3& pos, const float time)> _sdf;
};

#endif // MARCHED_HPP
