#include "Context.h"

#include "GL.h"
#include "GLCtx.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
void DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message,
                   const void *userParam) {
    auto *self = reinterpret_cast<const Context *>(userParam);
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        if (id != 131154 /* pixel-path performance warning */) {
            self->log()->Error("%s", message);
        }
    } else if (type != GL_DEBUG_TYPE_PUSH_GROUP && type != GL_DEBUG_TYPE_POP_GROUP && type != GL_DEBUG_TYPE_OTHER) {
        self->log()->Warning("%s", message);
    }
}
} // namespace Ren

Ren::Context::Context() = default;

Ren::Context::~Context() { ReleaseAll(); }

bool Ren::Context::Init(int w, int h, ILog *log, const char *) {
    InitGLExtentions();
    RegisterAsMainThread();

    w_ = w;
    h_ = h;
    log_ = log;

    api_ctx_.reset(new ApiContext);
    for (int i = 0; i < MaxFramesInFlight; i++) {
        api_ctx_->in_flight_fences.emplace_back(MakeFence());
    }

    log_->Info("===========================================");
    log_->Info("Device info:");

    // print device info
#if !defined(EMSCRIPTEN) && !defined(__ANDROID__)
    GLint gl_major_version;
    glGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);
    GLint gl_minor_version;
    glGetIntegerv(GL_MINOR_VERSION, &gl_minor_version);
    log_->Info("\tOpenGL version\t: %i.%i", int(gl_major_version), int(gl_minor_version));
#endif

    log_->Info("\tVendor\t\t: %s", glGetString(GL_VENDOR));
    log_->Info("\tRenderer\t: %s", glGetString(GL_RENDERER));
    log_->Info("\tGLSL version\t: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    log_->Info("Capabilities:");

    // determine if anisotropy supported
    if (IsExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
        GLfloat f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &f);
        capabilities.max_anisotropy = f;
        log_->Info("\tAnisotropy\t: %f", capabilities.max_anisotropy);
    }

    {
        GLint i = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &i);
        capabilities.max_vertex_input = i;
        log_->Info("\tMax vtx attribs\t: %i", capabilities.max_vertex_input);

        glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &i);
        i /= 4;
        capabilities.max_vertex_output = i;
        log_->Info("\tMax vtx output\t: %i", capabilities.max_vertex_output);
    }

    // determine compute work group sizes
    for (int i = 0; i < 3; i++) {
        GLint val;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &val);
        capabilities.max_compute_work_group_size[i] = val;
    }

    log_->Info("===========================================");

#if !defined(NDEBUG) && !defined(__APPLE__)
    if (IsExtensionSupported("GL_KHR_debug") || IsExtensionSupported("ARB_debug_output") ||
        IsExtensionSupported("AMD_debug_output")) {

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugCallback, this);
    }
#endif

#if !defined(__ANDROID__)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

    capabilities.spirv = IsExtensionSupported("GL_ARB_gl_spirv");
    capabilities.persistent_buf_mapping = IsExtensionSupported("GL_ARB_buffer_storage");

    const bool bindless_texture_arb = IsExtensionSupported("GL_ARB_bindless_texture");
    const bool bindless_texture_nv = IsExtensionSupported("GL_NV_bindless_texture");
    capabilities.bindless_texture = bindless_texture_arb || bindless_texture_nv;

    { // minimal texture buffer offset alignment
        GLint tex_buf_offset_alignment;
        glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &tex_buf_offset_alignment);
        capabilities.tex_buf_offset_alignment = tex_buf_offset_alignment;
    }

    { // minimal uniform buffer offset alignment
        GLint unif_buf_offset_alignment;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &unif_buf_offset_alignment);
        capabilities.unif_buf_offset_alignment = unif_buf_offset_alignment;
    }

    InitDefaultBuffers();

    texture_atlas_ = TextureAtlasArray{api_ctx_.get(),     TextureAtlasWidth,       TextureAtlasHeight,
                                       TextureAtlasLayers, eTexFormat::RawRGBA8888, eTexFilter::BilinearNoMipmap};

    return true;
}

void Ren::Context::Resize(int w, int h) {
    w_ = w;
    h_ = h;
    glViewport(0, 0, w_, h_);
}

bool Ren::Context::IsExtensionSupported(const char *ext) {
    GLint ext_count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);

    for (GLint i = 0; i < ext_count; i++) {
        const char *extension = (const char *)glGetStringi(GL_EXTENSIONS, i);
        if (strcmp(extension, ext) == 0) {
            return true;
        }
    }

    return false;
}

void Ren::ResetGLState() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    Ren::GLUnbindTextureUnits(0, 24);
    Ren::GLUnbindSamplers(0, 24);
    Ren::GLUnbindBufferUnits(0, 24);
}

void Ren::CheckError(const char *op, ILog *log) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        log->Error("after %s glError (0x%x)", op, error);
    }
}

void Ren::Context::BegSingleTimeCommands(void *cmd_buf) {}
Ren::SyncFence Ren::Context::EndSingleTimeCommands(void *cmd_buf) { return MakeFence(); }
void *Ren::Context::current_cmd_buf() { return nullptr; }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
