#include "frameBuffer.hpp"

#include "error.hpp"
#include <cstdio>

FrameBuffer::FrameBuffer(
    uint32_t w, uint32_t h, const std::vector<TextureParams> &texParams,
    GLenum depthFormat, GLenum depthAttachment)
: _depthRbo(0)
{
    // Generate and bind frame buffer object
    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);

    std::vector<GLenum> drawBuffers;
    _texIDs.resize(texParams.size());
    glGenTextures((GLsizei)texParams.size(), _texIDs.data());
    for (auto i = 0u; i < texParams.size(); ++i)
    {
        _texParams.emplace_back(texParams[i]);
        // Generate texture
        glBindTexture(GL_TEXTURE_2D, _texIDs[i]);
        glTexImage2D(
            GL_TEXTURE_2D, 0, texParams[i].internalFormat, w, h, 0,
            texParams[i].inputFormat, texParams[i].type, 0);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texParams[i].minFilter);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texParams[i].magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texParams[i].wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texParams[i].wrapT);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Bind to fbo
        glFramebufferTexture(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, _texIDs[i], 0);
        drawBuffers.emplace_back(GL_COLOR_ATTACHMENT0 + i);
    }
    glDrawBuffers((GLsizei)drawBuffers.size(), drawBuffers.data());

    if (depthFormat != 0 || depthAttachment != 0)
    {
        _depthFormat = depthFormat;
        // Generate and bind depth buffer
        glGenRenderbuffers(1, &_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, _depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, depthFormat, w, h);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, depthAttachment, GL_RENDERBUFFER, _depthRbo);
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        reportError("[framebuffer] Init failed");
        reportError("[framebuffer] Status: %u" + std::to_string(status));
    }
}

FrameBuffer::~FrameBuffer()
{
    glDeleteTextures((GLsizei)_texIDs.size(), _texIDs.data());
    glDeleteFramebuffers(1, &_fbo);
    glDeleteRenderbuffers(1, &_depthRbo);
}

FrameBuffer::FrameBuffer(FrameBuffer &&other)
: _fbo(other._fbo)
, _texIDs(other._texIDs)
, _texParams(other._texParams)
, _depthRbo(other._depthRbo)
, _depthFormat(other._depthFormat)
{
    other._fbo = 0;
    other._texIDs.clear();
    other._depthRbo = 0;
}

void FrameBuffer::bindWrite() { glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo); }

void FrameBuffer::bindRead(uint32_t texNum, GLenum texUnit, GLint uniform)
{
    if (texNum < _texIDs.size())
    {
        glActiveTexture(texUnit);
        glBindTexture(GL_TEXTURE_2D, _texIDs[texNum]);
        glUniform1i(uniform, texUnit - GL_TEXTURE0);
    }
}

void FrameBuffer::bindBlitRead()
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo);
}

void FrameBuffer::genMipmap(uint32_t texNum)
{
    glBindTexture(GL_TEXTURE_2D, _texIDs.at(texNum));
    glGenerateMipmap(GL_TEXTURE_2D);
}

void FrameBuffer::resize(uint32_t w, uint32_t h)
{
    for (auto i = 0u; i < _texIDs.size(); ++i)
    {
        glBindTexture(GL_TEXTURE_2D, _texIDs[i]);
        glTexImage2D(
            GL_TEXTURE_2D, 0, _texParams[i].internalFormat, w, h, 0,
            _texParams[i].inputFormat, _texParams[i].type, 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    if (_depthRbo != 0)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, _depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, _depthFormat, w, h);
    }
}
