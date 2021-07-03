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

    if (api_ctx_) {
        vkDeviceWaitIdle(api_ctx_->device);

        for (int i = 0; i < MaxFramesInFlight; ++i) {
            vkDestroyFence(api_ctx_->device, api_ctx_->in_flight_fences[i], nullptr);
            vkDestroySemaphore(api_ctx_->device, api_ctx_->render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(api_ctx_->device, api_ctx_->image_avail_semaphores[i], nullptr);
        }

        vkFreeCommandBuffers(api_ctx_->device, api_ctx_->command_pool, 1, &api_ctx_->setup_cmd_buf);
        vkFreeCommandBuffers(api_ctx_->device, api_ctx_->command_pool, MaxFramesInFlight, &api_ctx_->draw_cmd_buf[0]);

        for (int i = 0; i < StageBufferCount; ++i) {
            default_stage_bufs_.fences[i].ClientWaitSync();
            default_stage_bufs_.fences[i] = {};
            default_stage_bufs_.bufs[i] = {};
        }

        vkDestroyCommandPool(api_ctx_->device, api_ctx_->command_pool, nullptr);
        vkDestroyCommandPool(api_ctx_->device, api_ctx_->temp_command_pool, nullptr);

        for (size_t i = 0; i < api_ctx_->present_image_views.size(); ++i) {
            vkDestroyImageView(api_ctx_->device, api_ctx_->present_image_views[i], nullptr);
            // vkDestroyImage(api_ctx_->device, api_ctx_->present_images[i], nullptr);
        }

        vkDestroySwapchainKHR(api_ctx_->device, api_ctx_->swapchain, nullptr);

        vkDestroyDevice(api_ctx_->device, nullptr);
        vkDestroySurfaceKHR(api_ctx_->instance, api_ctx_->surface, nullptr);
#ifndef NDEBUG
        vkDestroyDebugReportCallbackEXT(api_ctx_->instance, api_ctx_->debug_callback, nullptr);
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
        XCloseDisplay(g_dpy); // has to be done before instance destruction
                              // (https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1894)
#endif
        vkDestroyInstance(api_ctx_->instance, nullptr);
    }
}

bool Ren::Context::Init(const int w, const int h, ILog *log, const char *preferred_device) {
    if (!LoadVulkan(log)) {
        return false;
    }

    w_ = w;
    h_ = h;
    log_ = log;

    api_ctx_.reset(new ApiContext);

#ifndef NDEBUG
    const char *enabled_layers[] = {"VK_LAYER_KHRONOS_validation"};
    const int enabled_layers_count = 1;
#endif

    if (!InitVkInstance(api_ctx_->instance, enabled_layers, enabled_layers_count, log)) {
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

        const VkResult res = vkCreateDebugReportCallbackEXT(api_ctx_->instance, &callback_create_info, nullptr,
                                                            &api_ctx_->debug_callback);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create debug report callback");
            return false;
        }
    }
#endif

    // Create platform-specific surface
    if (!InitVkSurface(api_ctx_->surface, api_ctx_->instance, log)) {
        return false;
    }

    if (!ChooseVkPhysicalDevice(api_ctx_->physical_device, api_ctx_->device_properties, api_ctx_->mem_properties,
                                api_ctx_->present_family_index, api_ctx_->graphics_family_index, preferred_device,
                                api_ctx_->instance, api_ctx_->surface, log)) {
        return false;
    }

    if (!InitVkDevice(api_ctx_->device, api_ctx_->physical_device, api_ctx_->present_family_index,
                      api_ctx_->graphics_family_index, enabled_layers, enabled_layers_count, log)) {
        return false;
    }

    if (!InitSwapChain(api_ctx_->swapchain, api_ctx_->surface_format, api_ctx_->res, api_ctx_->present_mode, w, h,
                       api_ctx_->device, api_ctx_->physical_device, api_ctx_->present_family_index,
                       api_ctx_->graphics_family_index, api_ctx_->surface, log)) {
        return false;
    }

    if (!InitCommandBuffers(api_ctx_->command_pool, api_ctx_->temp_command_pool, api_ctx_->setup_cmd_buf,
                            api_ctx_->draw_cmd_buf, api_ctx_->image_avail_semaphores,
                            api_ctx_->render_finished_semaphores, api_ctx_->in_flight_fences, api_ctx_->present_queue,
                            api_ctx_->graphics_queue, api_ctx_->device, api_ctx_->present_family_index, log)) {
        return false;
    }

    if (!InitImageViews(api_ctx_->present_images, api_ctx_->present_image_views, api_ctx_->device, api_ctx_->swapchain,
                        api_ctx_->surface_format, api_ctx_->setup_cmd_buf, api_ctx_->present_queue, log)) {
        return false;
    }

    RegisterAsMainThread();

    log_->Info("===========================================");
    log_->Info("Device info:");

    log_->Info("\tVulkan version\t: %i.%i", VK_API_VERSION_MAJOR(api_ctx_->device_properties.apiVersion),
               VK_API_VERSION_MINOR(api_ctx_->device_properties.apiVersion));

    auto it =
        std::find_if(std::begin(KnownVendors), std::end(KnownVendors), [this](std::pair<uint32_t, const char *> v) {
            return api_ctx_->device_properties.vendorID == v.first;
        });
    if (it != std::end(KnownVendors)) {
        log_->Info("\tVendor\t\t: %s", it->second);
    }
    log_->Info("\tName\t\t: %s", api_ctx_->device_properties.deviceName);

    log_->Info("===========================================");

    InitDefaultBuffers();

    texture_atlas_ = TextureAtlasArray{api_ctx_.get(),     TextureAtlasWidth,       TextureAtlasHeight,
                                       TextureAtlasLayers, eTexFormat::RawRGBA8888, eTexFilter::BilinearNoMipmap};

    return true;
}

void Ren::Context::Resize(int w, int h) {
    w_ = w;
    h_ = h;

    vkDeviceWaitIdle(api_ctx_->device);

    for (size_t i = 0; i < api_ctx_->present_image_views.size(); ++i) {
        vkDestroyImageView(api_ctx_->device, api_ctx_->present_image_views[i], nullptr);
        // vkDestroyImage(api_ctx_->device, api_ctx_->present_images[i], nullptr);
    }

    vkDestroySwapchainKHR(api_ctx_->device, api_ctx_->swapchain, nullptr);

    if (!InitSwapChain(api_ctx_->swapchain, api_ctx_->surface_format, api_ctx_->res, api_ctx_->present_mode, w, h,
                       api_ctx_->device, api_ctx_->physical_device, api_ctx_->present_family_index,
                       api_ctx_->graphics_family_index, api_ctx_->surface, log_)) {
        log_->Error("Swapchain initialization failed");
    }

    if (!InitImageViews(api_ctx_->present_images, api_ctx_->present_image_views, api_ctx_->device, api_ctx_->swapchain,
                        api_ctx_->surface_format, api_ctx_->setup_cmd_buf, api_ctx_->present_queue, log_)) {
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
    const VkResult res = vkCreateFence(api_ctx_->device, &fence_info, nullptr, &new_fence);
    if (res != VK_SUCCESS) {
        log_->Error("Failed to create fence!");
        return {};
    }

    Ren::EndSingleTimeCommands(api_ctx_->device, api_ctx_->graphics_queue, reinterpret_cast<VkCommandBuffer>(cmd_buf),
                               api_ctx_->temp_command_pool, new_fence);

    return SyncFence{api_ctx_->device, new_fence};
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
