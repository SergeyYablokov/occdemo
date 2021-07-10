#include "VKCtx.h"

#include "Log.h"
#include "SmallVector.h"

namespace Ren {
#if defined(__linux__)
Display *g_dpy = nullptr;
Window g_win = {};
#elif defined(__APPLE__)
void *g_metal_layer = nullptr;
#endif
} // namespace Ren

bool Ren::InitVkInstance(VkInstance &instance, const char *enabled_layers[], const int enabled_layers_count,
                         ILog *log) {
    { // Find validation layer
        uint32_t layers_count = 0;
        vkEnumerateInstanceLayerProperties(&layers_count, nullptr);

        if (!layers_count) {
            log->Error("Failed to find any layer in your system");
            return false;
        }

        SmallVector<VkLayerProperties, 16> layers_available(layers_count);
        vkEnumerateInstanceLayerProperties(&layers_count, &layers_available[0]);

#ifndef NDEBUG
        bool found_validation = false;
        for (uint32_t i = 0; i < layers_count; i++) {
            if (strcmp(layers_available[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                found_validation = true;
            }
        }

        if (!found_validation) {
            log->Error("Could not find validation layer");
            return false;
        }
#endif
    }

    const char *desired_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
#endif
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    const uint32_t number_required_extensions = 2;
    const uint32_t number_optional_extensions = 2;

    { // Find required extensions
        uint32_t ext_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);

        SmallVector<VkExtensionProperties, 16> extensions_available(ext_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, &extensions_available[0]);

        uint32_t found_required_extensions = 0;
        for (uint32_t i = 0; i < ext_count; i++) {
            for (uint32_t j = 0; j < number_required_extensions; j++) {
                if (strcmp(extensions_available[i].extensionName, desired_extensions[j]) == 0) {
                    found_required_extensions++;
                }
            }
        }
        if (found_required_extensions != number_required_extensions) {
            log->Error("Not all required extensions were found!");
            log->Error("\tRequested:");
            for (int i = 0; i < number_required_extensions; ++i) {
                log->Error("\t\t%s", desired_extensions[i]);
            }
            log->Error("\tFound:");
            for (uint32_t i = 0; i < ext_count; i++) {
                for (uint32_t j = 0; j < number_required_extensions; j++) {
                    if (strcmp(extensions_available[i].extensionName, desired_extensions[j]) == 0) {
                        log->Error("\t\t%s", desired_extensions[i]);
                    }
                }
            }
            return false;
        }
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Test1";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = enabled_layers_count;
    instance_info.ppEnabledLayerNames = enabled_layers;
    instance_info.enabledExtensionCount = number_required_extensions + number_optional_extensions;
    instance_info.ppEnabledExtensionNames = desired_extensions;

    VkResult res = vkCreateInstance(&instance_info, nullptr, &instance);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create vulkan instance");
        return false;
    }

    LoadVulkanExtensions(instance, log);

    return true;
}

bool Ren::InitVkSurface(VkSurfaceKHR &surface, VkInstance instance, ILog *log) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = GetModuleHandle(NULL);
    surface_create_info.hwnd = GetActiveWindow();

    VkResult res = vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS) {
        log->Error("Could not create surface");
        return false;
    }
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    VkXlibSurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_create_info.dpy = g_dpy;
    surface_create_info.window = g_win;

    VkResult res = vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS) {
        log->Error("Could not create surface");
        return false;
    }
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    VkIOSSurfaceCreateInfoMVK surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
    surface_create_info.pView = ctx.view_from_world;

    VkResult res = vkCreateIOSSurfaceMVK(ctx.instance, &surface_create_info, nullptr, &ctx.surface);
    if (res != VK_SUCCESS) {
        log->Error("Could not create surface");
        return false;
    }
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    VkMacOSSurfaceCreateInfoMVK surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    surface_create_info.pView = g_metal_layer;

    VkResult res = vkCreateMacOSSurfaceMVK(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS) {
        log->Error("Could not create surface");
        return false;
    }
#endif

    return true;
}

bool Ren::ChooseVkPhysicalDevice(VkPhysicalDevice &physical_device, VkPhysicalDeviceProperties &out_device_properties,
                                 VkPhysicalDeviceMemoryProperties &out_mem_properties,
                                 uint32_t &out_present_family_index, uint32_t &out_graphics_family_index,
                                 const char *preferred_device, VkInstance instance, VkSurfaceKHR surface, ILog *log) {
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

    SmallVector<VkPhysicalDevice, 4> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(instance, &physical_device_count, &physical_devices[0]);

    int best_device = -1, best_score = 0;

    for (uint32_t i = 0; i < physical_device_count; i++) {
        VkPhysicalDeviceProperties device_properties = {};
        vkGetPhysicalDeviceProperties(physical_devices[i], &device_properties);

        { // check for swapchain support
            uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(physical_devices[i], nullptr, &extension_count, nullptr);

            SmallVector<VkExtensionProperties, 16> available_extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(physical_devices[i], nullptr, &extension_count,
                                                 &available_extensions[0]);

            bool swapchain_supported = false, anisotropy_supported = false;

            for (uint32_t j = 0; j < extension_count; j++) {
                const VkExtensionProperties &ext = available_extensions[j];

                if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                    swapchain_supported = true;
                    break;
                }
            }

            VkPhysicalDeviceFeatures supported_features;
            vkGetPhysicalDeviceFeatures(physical_devices[i], &supported_features);

            anisotropy_supported = (supported_features.samplerAnisotropy == VK_TRUE);

            if (!swapchain_supported || !anisotropy_supported) {
                continue;
            }
        }

        uint32_t queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, nullptr);

        SmallVector<VkQueueFamilyProperties, 8> queue_family_properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, &queue_family_properties[0]);

        uint32_t present_family_index = 0xffffffff, graphics_family_index = 0xffffffff;

        for (uint32_t j = 0; j < queue_family_count; j++) {
            VkBool32 supports_present;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[i], j, surface, &supports_present);

            if (supports_present && queue_family_properties[j].queueCount > 0 &&
                queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                present_family_index = j;
                graphics_family_index = j;
                break;
            } else if (supports_present && queue_family_properties[j].queueCount > 0 &&
                       present_family_index == 0xffffffff) {
                present_family_index = j;
            } else if (queue_family_properties[j].queueCount > 0 &&
                       (queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                       graphics_family_index == 0xffffffff) {
                graphics_family_index = j;
            }
        }

        if (present_family_index != 0xffffffff && graphics_family_index != 0xffffffff) {
            int score = 0;

            if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 1000;
            }

            score += int(device_properties.limits.maxImageDimension2D);

            if (present_family_index == graphics_family_index) {
                // prefer same present and graphics queue family for performance
                score += 500;
            }

            if (preferred_device && strstr(device_properties.deviceName, preferred_device)) {
                // preffered device found
                score += 100000;
            }

            if (score > best_score) {
                best_device = int(i);
                best_score = score;

                physical_device = physical_devices[i];
                out_device_properties = device_properties;
                out_present_family_index = present_family_index;
                out_graphics_family_index = graphics_family_index;
            }
        }
    }

    if (!physical_device) {
        log->Error("No physical device detected that can render and present!");
        return false;
    }

    vkGetPhysicalDeviceMemoryProperties(physical_device, &out_mem_properties);

    return true;
}

bool Ren::InitVkDevice(VkDevice &device, VkPhysicalDevice physical_device, uint32_t present_family_index,
                       uint32_t graphics_family_index, const char *enabled_layers[], int enabled_layers_count,
                       ILog *log) {
    VkDeviceQueueCreateInfo queue_create_infos[2] = {{}, {}};
    const float queue_priorities[] = {1.0f};

    { // present queue
        queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[0].queueFamilyIndex = present_family_index;
        queue_create_infos[0].queueCount = 1;

        queue_create_infos[0].pQueuePriorities = queue_priorities;
    }
    int infos_count = 1;

    if (graphics_family_index != present_family_index) {
        // graphics queue
        queue_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[1].queueFamilyIndex = graphics_family_index;
        queue_create_infos[1].queueCount = 1;

        queue_create_infos[1].pQueuePriorities = queue_priorities;

        ++infos_count;
    }

    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = infos_count;
    device_info.pQueueCreateInfos = queue_create_infos;
    device_info.enabledLayerCount = enabled_layers_count;
    device_info.ppEnabledLayerNames = enabled_layers;

    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_extensions;

    VkPhysicalDeviceFeatures features = {};
    features.shaderClipDistance = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    device_info.pEnabledFeatures = &features;

    const VkResult res = vkCreateDevice(physical_device, &device_info, nullptr, &device);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create logical device!");
        return false;
    }

    return true;
}

bool Ren::InitSwapChain(VkSwapchainKHR &swapchain, VkSurfaceFormatKHR &surface_format, VkExtent2D &extent,
                        VkPresentModeKHR &present_mode, int w, int h, VkDevice device, VkPhysicalDevice physical_device,
                        uint32_t present_family_index, uint32_t graphics_family_index, VkSurfaceKHR surface,
                        ILog *log) {
    { // choose surface format
        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);

        SmallVector<VkSurfaceFormatKHR, 8> surface_formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, &surface_formats[0]);

        if (format_count == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED) {
            surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;
            surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        } else {
            surface_format = surface_formats[0];

            for (uint32_t i = 0; i < format_count; i++) {
                const VkSurfaceFormatKHR &fmt = surface_formats[i];

                if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    surface_format = fmt;
                    break;
                }
            }
        }
    }

    { // choose present mode
        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);

        SmallVector<VkPresentModeKHR, 8> present_modes(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, &present_modes[0]);

        // fifo mode is guaranteed to be supported
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for (uint32_t i = 0; i < present_mode_count; i++) {
            if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            } else if (present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
                // gives less latency than fifo mode, use it
                present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            }
        }
    }

    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

    { // choose resolution
        VkExtent2D surface_resolution = surface_capabilities.currentExtent;
        if (surface_resolution.width == 0xffffffff) {
            // can use any value, use native window resolution
            extent = VkExtent2D{uint32_t(w), uint32_t(h)};
        } else {
            VkExtent2D actual_extent = VkExtent2D{uint32_t(w), uint32_t(h)};

            actual_extent.width = std::min(std::max(actual_extent.width, surface_capabilities.minImageExtent.width),
                                           surface_capabilities.maxImageExtent.width);
            actual_extent.height = std::min(std::max(actual_extent.height, surface_capabilities.minImageExtent.height),
                                            surface_capabilities.maxImageExtent.height);

            extent = actual_extent;
        }
    }

    uint32_t desired_image_count = 1;
    if (present_mode == VK_PRESENT_MODE_FIFO_KHR || present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
        desired_image_count = 2;
    } else if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        desired_image_count = 3;
    }

    if (desired_image_count < surface_capabilities.minImageCount) {
        desired_image_count = surface_capabilities.minImageCount;
    } else if (surface_capabilities.maxImageCount != 0 && desired_image_count > surface_capabilities.maxImageCount) {
        desired_image_count = surface_capabilities.maxImageCount;
    }

    VkSurfaceTransformFlagBitsKHR pre_transform = surface_capabilities.currentTransform;
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }

    VkSwapchainCreateInfoKHR swap_chain_create_info = {};
    swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.surface = surface;
    swap_chain_create_info.minImageCount = desired_image_count;
    swap_chain_create_info.imageFormat = surface_format.format;
    swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
    swap_chain_create_info.imageExtent = extent;
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t queue_fam_indices[] = {present_family_index, graphics_family_index};

    if (queue_fam_indices[0] != queue_fam_indices[1]) {
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swap_chain_create_info.queueFamilyIndexCount = 2;
        swap_chain_create_info.pQueueFamilyIndices = queue_fam_indices;
    } else {
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swap_chain_create_info.preTransform = pre_transform;
    swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.presentMode = present_mode;
    swap_chain_create_info.clipped = VK_TRUE;
    swap_chain_create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult res = vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swapchain);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create swapchain!");
        return false;
    }

    return true;
}

bool Ren::InitCommandBuffers(VkCommandPool &command_pool, VkCommandPool &temp_command_pool,
                             VkCommandBuffer &setup_cmd_buf, VkCommandBuffer draw_cmd_buf[MaxFramesInFlight],
                             VkSemaphore image_avail_semaphores[MaxFramesInFlight],
                             VkSemaphore render_finished_semaphores[MaxFramesInFlight],
                             VkFence in_flight_fences[MaxFramesInFlight], VkQueue &present_queue,
                             VkQueue &graphics_queue, VkDevice device, uint32_t present_family_index, ILog *log) {
    vkGetDeviceQueue(device, present_family_index, 0, &present_queue);
    graphics_queue = present_queue;

    VkCommandPoolCreateInfo cmd_pool_create_info = {};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_create_info.queueFamilyIndex = present_family_index;

    VkResult res = vkCreateCommandPool(device, &cmd_pool_create_info, nullptr, &command_pool);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create command pool!");
        return false;
    }

    { // create pool for temporary commands
        VkCommandPoolCreateInfo tmp_cmd_pool_create_info = {};
        tmp_cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        tmp_cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        tmp_cmd_pool_create_info.queueFamilyIndex = present_family_index;

        res = vkCreateCommandPool(device, &tmp_cmd_pool_create_info, nullptr, &temp_command_pool);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create command pool!");
            return false;
        }
    }

    VkCommandBufferAllocateInfo cmd_buf_alloc_info = {};
    cmd_buf_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buf_alloc_info.commandPool = command_pool;
    cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buf_alloc_info.commandBufferCount = 1;

    res = vkAllocateCommandBuffers(device, &cmd_buf_alloc_info, &setup_cmd_buf);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create command buffer!");
        return false;
    }

    for (int i = 0; i < MaxFramesInFlight; i++) {
        res = vkAllocateCommandBuffers(device, &cmd_buf_alloc_info, &draw_cmd_buf[i]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create command buffer!");
            return false;
        }
    }

    { // create fences
        VkSemaphoreCreateInfo sem_info = {};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MaxFramesInFlight; i++) {
            res = vkCreateSemaphore(device, &sem_info, nullptr, &image_avail_semaphores[i]);
            if (res != VK_SUCCESS) {
                log->Error("Failed to create semaphore!");
                return false;
            }
            res = vkCreateSemaphore(device, &sem_info, nullptr, &render_finished_semaphores[i]);
            if (res != VK_SUCCESS) {
                log->Error("Failed to create semaphore!");
                return false;
            }
            res = vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]);
            if (res != VK_SUCCESS) {
                log->Error("Failed to create fence!");
                return false;
            }
        }
    }

    return true;
}

bool Ren::InitImageViews(SmallVectorImpl<VkImage> &present_images, SmallVectorImpl<VkImageView> &present_image_views,
                         VkDevice device, VkSwapchainKHR swapchain, VkSurfaceFormatKHR surface_format,
                         VkCommandBuffer setup_cmd_buf, VkQueue present_queue, ILog *log) {
    uint32_t image_count;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);

    present_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, &present_images[0]);

    VkImageViewCreateInfo present_images_view_create_info = {};
    present_images_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    present_images_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    present_images_view_create_info.format = surface_format.format;
    present_images_view_create_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    present_images_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    present_images_view_create_info.subresourceRange.baseMipLevel = 0;
    present_images_view_create_info.subresourceRange.levelCount = 1;
    present_images_view_create_info.subresourceRange.baseArrayLayer = 0;
    present_images_view_create_info.subresourceRange.layerCount = 1;

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence submit_fence = {};
    vkCreateFence(device, &fence_create_info, nullptr, &submit_fence);

    present_image_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        present_images_view_create_info.image = present_images[i];

        vkBeginCommandBuffer(setup_cmd_buf, &begin_info);

        VkImageMemoryBarrier layout_transition_barrier = {};
        layout_transition_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        layout_transition_barrier.srcAccessMask = 0;
        layout_transition_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        layout_transition_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        layout_transition_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        layout_transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        layout_transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        layout_transition_barrier.image = present_images[i];

        VkImageSubresourceRange resource_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        layout_transition_barrier.subresourceRange = resource_range;

        vkCmdPipelineBarrier(setup_cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &layout_transition_barrier);

        vkEndCommandBuffer(setup_cmd_buf);

        VkPipelineStageFlags wait_stage_mask[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.pWaitDstStageMask = wait_stage_mask;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &setup_cmd_buf;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = nullptr;

        VkResult res = vkQueueSubmit(present_queue, 1, &submit_info, submit_fence);
        if (res != VK_SUCCESS) {
            log->Error("vkQueueSubmit failed");
            return false;
        }

        vkWaitForFences(device, 1, &submit_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &submit_fence);

        vkResetCommandBuffer(setup_cmd_buf, 0);

        res = vkCreateImageView(device, &present_images_view_create_info, nullptr, &present_image_views[i]);
        if (res != VK_SUCCESS) {
            log->Error("vkCreateImageView failed");
            return false;
        }
    }

    vkDestroyFence(device, submit_fence, nullptr);

    return true;
}

VkCommandBuffer Ren::BegSingleTimeCommands(VkDevice device, VkCommandPool temp_command_pool) {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = temp_command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buf = {};
    vkAllocateCommandBuffers(device, &alloc_info, &command_buf);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buf, &begin_info);
    return command_buf;
}

void Ren::EndSingleTimeCommands(VkDevice device, VkQueue cmd_queue, VkCommandBuffer command_buf,
                                VkCommandPool temp_command_pool) {
    vkEndCommandBuffer(command_buf);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buf;

    vkQueueSubmit(cmd_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(cmd_queue);

    vkFreeCommandBuffers(device, temp_command_pool, 1, &command_buf);
}

void Ren::EndSingleTimeCommands(VkDevice device, VkQueue cmd_queue, VkCommandBuffer command_buf,
                                VkCommandPool temp_command_pool, VkFence fence_to_insert) {
    vkEndCommandBuffer(command_buf);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buf;

    vkQueueSubmit(cmd_queue, 1, &submit_info, fence_to_insert);
}

void Ren::FreeSingleTimeCommandBuffer(VkDevice device, VkCommandPool temp_command_pool, VkCommandBuffer command_buf) {
    vkFreeCommandBuffers(device, temp_command_pool, 1, &command_buf);
}
