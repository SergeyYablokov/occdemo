#include "ProgramVK.h"

//#include "GL.h"
#include "Log.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
bool IsMainThread();
} // namespace Ren

Ren::Program::Program(const char *name, ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref,
                      eProgLoadStatus *status, ILog *log) {
    name_ = String{name};
    Init(std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref), std::move(tes_ref), status, log);
}

Ren::Program::Program(const char *name, ShaderRef cs_ref, eProgLoadStatus *status, ILog *log) {
    name_ = String{name};
    Init(std::move(cs_ref), status, log);
}

Ren::Program::~Program() {
    /*if (id_) {
        assert(IsMainThread());
        auto prog = GLuint(id_);
        glDeleteProgram(prog);
    }*/
}

Ren::Program &Ren::Program::operator=(Program &&rhs) noexcept {
    /*if (id_) {
        assert(IsMainThread());
        auto prog = GLuint(id_);
        glDeleteProgram(prog);
    }*/

    // id_ = exchange(rhs.id_, 0);
    shaders_ = std::move(rhs.shaders_);
    attributes_ = std::move(rhs.attributes_);
    uniforms_ = std::move(rhs.uniforms_);
    uniform_blocks_ = std::move(rhs.uniform_blocks_);
    name_ = std::move(rhs.name_);

    RefCounter::operator=(std::move(rhs));

    return *this;
}

void Ren::Program::Init(ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref,
                        eProgLoadStatus *status, ILog *log) {
    // assert(id_ == 0);
    assert(IsMainThread());

    if (!vs_ref || !fs_ref) {
        (*status) = eProgLoadStatus::SetToDefault;
        return;
    }

    // store shaders
    shaders_[int(eShaderType::Vert)] = std::move(vs_ref);
    shaders_[int(eShaderType::Frag)] = std::move(fs_ref);
    shaders_[int(eShaderType::Tesc)] = std::move(tcs_ref);
    shaders_[int(eShaderType::Tese)] = std::move(tes_ref);

    InitBindings(log);

    (*status) = eProgLoadStatus::CreatedFromData;
}

void Ren::Program::Init(ShaderRef cs_ref, eProgLoadStatus *status, ILog *log) {
    // assert(id_ == 0);
    assert(IsMainThread());

    if (!cs_ref) {
        (*status) = eProgLoadStatus::SetToDefault;
        return;
    }

    /*GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, (GLuint)cs_ref->id());
        glLinkProgram(program);
        GLint link_status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &link_status);
        if (link_status != GL_TRUE) {
            GLint buf_len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_len);
            if (buf_len) {
                std::unique_ptr<char[]> buf(new char[buf_len]);
                if (buf) {
                    glGetProgramInfoLog(program, buf_len, nullptr, buf.get());
                    log->Error("Could not link program: %s", buf.get());
                }
            }
            glDeleteProgram(program);
            program = 0;
        } else {
#ifdef ENABLE_OBJ_LABELS
            glObjectLabel(GL_PROGRAM, program, -1, name_.c_str());
#endif
        }
    } else {
        log->Error("glCreateProgram failed");
    }*/

    // id_ = uint32_t(program);
    // store shader
    shaders_[int(eShaderType::Comp)] = std::move(cs_ref);

    InitBindings(log);

    (*status) = eProgLoadStatus::CreatedFromData;
}

void Ren::Program::InitBindings(ILog *log) {
    attributes_.clear();
    uniforms_.clear();
    uniform_blocks_.clear();

    for (const ShaderRef &sh_ref : shaders_) {
        if (!sh_ref) {
            continue;
        }

        const Shader &sh = (*sh_ref);
        for (const Descr &b : sh.blck_bindings) {
            auto it = std::find(std::begin(uniform_blocks_), std::end(uniform_blocks_), b);
            if (it == std::end(uniform_blocks_)) {
                uniform_blocks_.emplace_back(b);
            }
        }

        for (const Descr &u : sh.unif_bindings) {
            auto it = std::find(std::begin(uniforms_), std::end(uniforms_), u);
            if (it == std::end(uniforms_)) {
                uniforms_.emplace_back(u);
            }
        }
    }

    if (shaders_[int(eShaderType::Vert)]) {
        for (const Descr &a : shaders_[int(eShaderType::Vert)]->attr_bindings) {
            attributes_.emplace_back(a);
        }
    }

    log->Info("PROGRAM %s", name_.c_str());

    // Print all attributes
    log->Info("\tATTRIBUTES");
    for (int i = 0; i < int(attributes_.size()); i++) {
        if (attributes_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", attributes_[i].name.c_str(), attributes_[i].loc);
    }

    // Print all uniforms
    log->Info("\tUNIFORMS");
    for (int i = 0; i < int(uniforms_.size()); i++) {
        if (uniforms_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", uniforms_[i].name.c_str(), uniforms_[i].loc);
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
