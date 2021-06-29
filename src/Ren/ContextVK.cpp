#include "Context.h"

#include "VKCtx.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#include <Windows.h>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
VKAPI_ATTR VkBool32 VKAPI_ATTR DebugReportCallback(const VkDebugReportFlagsEXT flags,
                                                   const VkDebugReportObjectTypeEXT objectType, const uint64_t object,
                                                   const size_t location, const int32_t messageCode,
                                                   const char *pLayerPrefix, const char *pMessage, void *pUserData) {
    auto *ctx = reinterpret_cast<const Context *>(pUserData);

    ctx->log()->Error("%s: %s\n", pLayerPrefix, pMessage);
    return VK_FALSE;
}

const std::pair<uint32_t, const char *> KnownVendors[] = {
    {0x1002, "AMD"}, {0x10DE, "NVIDIA"}, {0x8086, "INTEL"}, {0x13B5, "ARM"}};
} // namespace Ren

Ren::Context::Context() = default;

Ren::Context::~Context() {
    ReleaseAll();

    if (ctx_) {
        vkDeviceWaitIdle(ctx_->device);

        for (int i = 0; i < MaxFramesInFlight; ++i) {
            vkDestroyFence(ctx_->device, ctx_->in_flight_fences[i], nullptr);
            vkDestroySemaphore(ctx_->device, ctx_->render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(ctx_->device, ctx_->image_avail_semaphores[i], nullptr);
        }

        vkFreeCommandBuffers(ctx_->device, ctx_->command_pool, 1, &ctx_->setup_cmd_buf);
        vkFreeCommandBuffers(ctx_->device, ctx_->command_pool, MaxFramesInFlight, &ctx_->draw_cmd_buf[0]);

        for (int i = 0; i < StageBufferCount; ++i) {
            default_stage_bufs_.fences[i].ClientWaitSync();
            default_stage_bufs_.fences[i] = {};
            default_stage_bufs_.bufs[i] = {};
        }

        vkDestroyCommandPool(ctx_->device, ctx_->command_pool, nullptr);
        vkDestroyCommandPool(ctx_->device, ctx_->temp_command_pool, nullptr);

        for (size_t i = 0; i < ctx_->present_image_views.size(); ++i) {
            vkDestroyImageView(ctx_->device, ctx_->present_image_views[i], nullptr);
            // vkDestroyImage(ctx_->device, ctx_->present_images[i], nullptr);
        }

        vkDestroySwapchainKHR(ctx_->device, ctx_->swapchain, nullptr);

        vkDestroyDevice(ctx_->device, nullptr);
        vkDestroySurfaceKHR(ctx_->instance, ctx_->surface, nullptr);
#ifndef NDEBUG
        vkDestroyDebugReportCallbackEXT(ctx_->instance, ctx_->debug_callback, nullptr);
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
        XCloseDisplay(g_dpy); // has to be done before instance destruction
                              // (https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1894)
#endif
        vkDestroyInstance(ctx_->instance, nullptr);
    }
}

bool Ren::Context::Init(const int w, const int h, ILog *log, const char *preferred_device) {
    if (!LoadVulkan(log)) {
        return false;
    }

    w_ = w;
    h_ = h;
    log_ = log;

    ctx_.reset(new VkContext);

#ifndef NDEBUG
    const char *enabled_layers[] = {"VK_LAYER_KHRONOS_validation"};
    const int enabled_layers_count = 1;
#endif

    if (!InitVkInstance(ctx_->instance, enabled_layers, enabled_layers_count, log)) {
        return false;
    }

#ifndef NDEBUG
    { // Sebug debug report callback
        VkDebugReportCallbackCreateInfoEXT callback_create_info = {};
        callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        callback_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                     VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        callback_create_info.pfnCallback = DebugReportCallback;
        callback_create_info.pUserData = this;

        const VkResult res =
            vkCreateDebugReportCallbackEXT(ctx_->instance, &callback_create_info, nullptr, &ctx_->debug_callback);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create debug report callback");
            return false;
        }
    }
#endif

    // Create platform-specific surface
    if (!InitVkSurface(ctx_->surface, ctx_->instance, log)) {
        return false;
    }

    if (!ChooseVkPhysicalDevice(ctx_->physical_device, ctx_->device_properties, ctx_->mem_properties,
                                ctx_->present_family_index, ctx_->graphics_family_index, preferred_device,
                                ctx_->instance, ctx_->surface, log)) {
        return false;
    }

    if (!InitVkDevice(ctx_->device, ctx_->physical_device, ctx_->present_family_index, ctx_->graphics_family_index,
                      enabled_layers, enabled_layers_count, log)) {
        return false;
    }

    if (!InitSwapChain(ctx_->swapchain, ctx_->surface_format, ctx_->res, ctx_->present_mode, w, h, ctx_->device,
                       ctx_->physical_device, ctx_->present_family_index, ctx_->graphics_family_index, ctx_->surface,
                       log)) {
        return false;
    }

    if (!InitCommandBuffers(ctx_->command_pool, ctx_->temp_command_pool, ctx_->setup_cmd_buf, ctx_->draw_cmd_buf,
                            ctx_->image_avail_semaphores, ctx_->render_finished_semaphores, ctx_->in_flight_fences,
                            ctx_->present_queue, ctx_->graphics_queue, ctx_->device, ctx_->present_family_index, log)) {
        return false;
    }

    if (!InitImageViews(ctx_->present_images, ctx_->present_image_views, ctx_->device, ctx_->swapchain,
                        ctx_->surface_format, ctx_->setup_cmd_buf, ctx_->present_queue, log)) {
        return false;
    }

    RegisterAsMainThread();

    log_->Info("===========================================");
    log_->Info("Device info:");

    log_->Info("\tVulkan version\t: %i.%i", VK_API_VERSION_MAJOR(ctx_->device_properties.apiVersion),
               VK_API_VERSION_MINOR(ctx_->device_properties.apiVersion));

    auto it =
        std::find_if(std::begin(KnownVendors), std::end(KnownVendors), [this](std::pair<uint32_t, const char *> v) {
            return ctx_->device_properties.vendorID == v.first;
        });
    if (it != std::end(KnownVendors)) {
        log_->Info("\tVendor\t\t: %s", it->second);
    }
    log_->Info("\tName\t\t: %s", ctx_->device_properties.deviceName);

    log_->Info("===========================================");

    InitDefaultBuffers();

    texture_atlas_ = TextureAtlasArray{ctx_.get(),         TextureAtlasWidth,       TextureAtlasHeight,
                                       TextureAtlasLayers, eTexFormat::RawRGBA8888, eTexFilter::BilinearNoMipmap};

    return true;
}

void Ren::Context::Resize(int w, int h) {
    w_ = w;
    h_ = h;

    vkDeviceWaitIdle(ctx_->device);

    for (size_t i = 0; i < ctx_->present_image_views.size(); ++i) {
        vkDestroyImageView(ctx_->device, ctx_->present_image_views[i], nullptr);
        // vkDestroyImage(ctx_->device, ctx_->present_images[i], nullptr);
    }

    vkDestroySwapchainKHR(ctx_->device, ctx_->swapchain, nullptr);

    if (!InitSwapChain(ctx_->swapchain, ctx_->surface_format, ctx_->res, ctx_->present_mode, w, h, ctx_->device,
                       ctx_->physical_device, ctx_->present_family_index, ctx_->graphics_family_index, ctx_->surface,
                       log_)) {
        log_->Error("Swapchain initialization failed");
    }

    if (!InitImageViews(ctx_->present_images, ctx_->present_image_views, ctx_->device, ctx_->swapchain,
                        ctx_->surface_format, ctx_->setup_cmd_buf, ctx_->present_queue, log_)) {
        log_->Error("Image views initialization failed");
    }
}

void Ren::Context::BegSingleTimeCommands(void *_cmd_buf) {
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    vkResetCommandBuffer(cmd_buf, 0);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd_buf, &begin_info);
}

Ren::SyncFence Ren::Context::EndSingleTimeCommands(void *cmd_buf) {
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence new_fence;
    VkResult res = vkCreateFence(ctx_->device, &fence_info, nullptr, &new_fence);
    if (res != VK_SUCCESS) {
        log_->Error("Failed to create fence!");
        return {};
    }

    Ren::EndSingleTimeCommands(ctx_->device, ctx_->graphics_queue, reinterpret_cast<VkCommandBuffer>(cmd_buf),
                               ctx_->temp_command_pool, new_fence);

    return SyncFence{ctx_->device, new_fence};
}

#if 0
Ren::ProgramRef Ren::Context::LoadProgramGLSL(const char *name, const char *vs_source, const char *fs_source, eProgLoadStatus *load_status) {
    ProgramRef ref = programs_.FindByName(name);

    std::string vs_source_str, fs_source_str;

    if (vs_source) {
        vs_source_str = glsl_defines_ + vs_source;
        vs_source = vs_source_str.c_str();
    }

    if (fs_source) {
        fs_source_str = glsl_defines_ + fs_source;
        fs_source = fs_source_str.c_str();
    }

    if (!ref) {
        ref = programs_.Add(name, vs_source, fs_source, load_status);
    } else {
        if (ref->ready()) {
            if (load_status) *load_status = ProgFound;
        } else if (!ref->ready() && vs_source && fs_source) {
            ref->Init(vs_source, fs_source, load_status);
        }
    }

    return ref;
}

Ren::ProgramRef Ren::Context::LoadProgramGLSL(const char *name, const char *cs_source, eProgLoadStatus *load_status) {
    ProgramRef ref = programs_.FindByName(name);

    std::string cs_source_str;

    if (cs_source) {
        cs_source_str = glsl_defines_ + cs_source;
        cs_source = cs_source_str.c_str();
    }

    if (!ref) {
        ref = programs_.Add(name, cs_source, load_status);
    } else {
        if (ref->ready()) {
            if (load_status) *load_status = ProgFound;
        } else if (!ref->ready() && cs_source) {
            ref->Init(name, cs_source, load_status);
        }
    }

    return ref;
}
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif
