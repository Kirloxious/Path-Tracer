#include "renderer.h"

#include <iostream>

Texture createTexture(int width, int height){
    Texture texture;
    texture.width = width;
    texture.height = height;

    glCreateTextures(GL_TEXTURE_2D, 1, &texture.handle);

    glTextureStorage2D(texture.handle, 1, GL_RGBA32F, texture.width, texture.height);
 
    glTextureParameteri(texture.handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(texture.handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 
    glTextureParameteri(texture.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texture.width, texture.height, 0, GL_RGBA, GL_FLOAT, NULL);

    return texture;
}

void bindDoubleBufferTexture(const Texture texture){
    glBindImageTexture(0, texture.handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, texture.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
}

FrameBuffer createFrameBuffer(const Texture texture){
    FrameBuffer buffer;

    glCreateFramebuffers(1, &buffer.handle);

    if(!attachTextureToFrameBuffer(texture, buffer)) {
        std::cerr << "Failed to attach texture to framebuffer" << std::endl;
        glDeleteFramebuffers(1, &buffer.handle);
        return {};
    }

    return buffer;
}

bool attachTextureToFrameBuffer( const Texture texture, FrameBuffer& framebuffer){
	glNamedFramebufferTexture(framebuffer.handle, GL_COLOR_ATTACHMENT0, texture.handle, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Framebuffer is not complete!" << std::endl;
		return false;
	}

	framebuffer.texture = texture;
	return true;
}

void blitFrameBuffer(const FrameBuffer frameBuffer){
    glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer.handle);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // (swapchain)

	glBlitFramebuffer(0, 0, frameBuffer.texture.width, frameBuffer.texture.height, // Source rect
		0, 0, frameBuffer.texture.width, frameBuffer.texture.height,               // Destination rect
		GL_COLOR_BUFFER_BIT, GL_NEAREST);
}