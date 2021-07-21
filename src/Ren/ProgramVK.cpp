#include "ProgramVK.h"

//#include "GL.h"
#include "Log.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
const VkShaderStageFlagBits g_shader_stage_flag_bits[int(eShaderType::_Count)] = {
    VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM,          // None
    VK_SHADER_STAGE_VERTEX_BIT,                  // Vert
    VK_SHADER_STAGE_FRAGMENT_BIT,                // Frag
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,    // Tesc
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, // Tese
    VK_SHADER_STAGE_COMPUTE_BIT                  // Comp
};
static_assert(sizeof(g_shader_stage_flag_bits) / sizeof(g_shader_stage_flag_bits[0]) == int(eShaderType::_Count), "!");

bool IsMainThread();
} // namespace Ren

Ren::Program::Program(const char *name, ApiContext *api_ctx, ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref,
                      ShaderRef tes_ref, eProgLoadStatus *status, ILog *log) {
    name_ = String{name};
    api_ctx_ = api_ctx;
    Init(std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref), std::move(tes_ref), status, log);
}

Ren::Program::Program(const char *name, ApiContext *api_ctx, ShaderRef cs_ref, eProgLoadStatus *status, ILog *log) {
    name_ = String{name};
    api_ctx_ = api_ctx;
    Init(std::move(cs_ref), status, log);
}

Ren::Program::~Program() { Destroy(); }

Ren::Program &Ren::Program::operator=(Program &&rhs) noexcept {
    Destroy();

    shaders_ = std::move(rhs.shaders_);
    attributes_ = std::move(rhs.attributes_);
    uniforms_ = std::move(rhs.uniforms_);
    name_ = std::move(rhs.name_);

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    desc_set_layouts_ = rhs.desc_set_layouts_;
    rhs.desc_set_layouts_.fill(VK_NULL_HANDLE);

    RefCounter::operator=(std::move(rhs));

    return *this;
}

void Ren::Program::Destroy() {
    for (VkDescriptorSetLayout &l : desc_set_layouts_) {
        if (l) {
            vkDestroyDescriptorSetLayout(api_ctx_->device, l, nullptr);
        }
    }
    desc_set_layouts_.fill(VK_NULL_HANDLE);
}

void Ren::Program::Init(ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref,
                        eProgLoadStatus *status, ILog *log) {
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

    if (!InitDescSetLayouts(log)) {
        log->Error("Failed to initialize descriptor set layouts! (%s)", name_.c_str());
    }
    InitBindings(log);

    (*status) = eProgLoadStatus::CreatedFromData;
}

void Ren::Program::Init(ShaderRef cs_ref, eProgLoadStatus *status, ILog *log) {
    assert(IsMainThread());

    if (!cs_ref) {
        (*status) = eProgLoadStatus::SetToDefault;
        return;
    }

    // store shader
    shaders_[int(eShaderType::Comp)] = std::move(cs_ref);

    if (!InitDescSetLayouts(log)) {
        log->Error("Failed to initialize descriptor set layouts! (%s)", name_.c_str());
    }
    InitBindings(log);

    (*status) = eProgLoadStatus::CreatedFromData;
}

bool Ren::Program::InitDescSetLayouts(ILog *log) {
    SmallVector<VkDescriptorSetLayoutBinding, 16> layout_bindings[4];

    for (int i = 0; i < int(eShaderType::_Count); ++i) {
        const ShaderRef &sh_ref = shaders_[i];
        if (!sh_ref) {
            continue;
        }

        const Shader &sh = (*sh_ref);
        for (const Descr &u : sh.unif_bindings) {
            auto &bindings = layout_bindings[u.set];

            auto it = std::find_if(std::begin(bindings), std::end(bindings),
                                   [&u](const VkDescriptorSetLayoutBinding &b) { return u.loc == b.binding; });

            if (it == std::end(bindings)) {
                auto &new_binding = bindings.emplace_back();
                new_binding.binding = u.loc;
                new_binding.descriptorType = u.desc_type;
                new_binding.descriptorCount = 1;
                new_binding.stageFlags = g_shader_stage_flag_bits[i];
                new_binding.pImmutableSamplers = nullptr;
            } else {
                it->stageFlags |= g_shader_stage_flag_bits[i];
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (layout_bindings[i].empty()) {
            continue;
        }

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = uint32_t(layout_bindings[i].size());
        layout_info.pBindings = layout_bindings[i].cdata();

        const VkResult res =
            vkCreateDescriptorSetLayout(api_ctx_->device, &layout_info, nullptr, &desc_set_layouts_[i]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create descriptor set layout!");
            return false;
        }
    }

    return true;
}

void Ren::Program::InitBindings(ILog *log) {
    attributes_.clear();
    uniforms_.clear();
    pc_ranges_.clear();

    for (int i = 0; i < int(eShaderType::_Count); ++i) {
        const ShaderRef &sh_ref = shaders_[i];
        if (!sh_ref) {
            continue;
        }

        const Shader &sh = (*sh_ref);
        for (const Descr &u : sh.unif_bindings) {
            auto it = std::find(std::begin(uniforms_), std::end(uniforms_), u);
            if (it == std::end(uniforms_)) {
                uniforms_.emplace_back(u);
            }
        }

        for (const Range r : sh.pc_ranges) {
            auto it = std::find_if(std::begin(pc_ranges_), std::end(pc_ranges_), [&](const VkPushConstantRange &rng) {
                return r.offset == rng.offset && r.size == rng.size;
            });

            if (it == std::end(pc_ranges_)) {
                VkPushConstantRange &new_rng = pc_ranges_.emplace_back();
                new_rng.stageFlags = g_shader_stage_flag_bits[i];
                new_rng.offset = r.offset;
                new_rng.size = r.size;
            } else {
                it->stageFlags |= g_shader_stage_flag_bits[i];
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
