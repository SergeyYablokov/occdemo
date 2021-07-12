#pragma once

#include "Common.h"
#include "MemoryAllocator.h"
#include "SmallVector.h"
#include "VK.h"

namespace Ren {
struct ApiContext {
    VkInstance instance = {};
#ifndef NDEBUG
    VkDebugReportCallbackEXT debug_callback = {};
#endif
    VkSurfaceKHR surface = {};
    VkPhysicalDevice physical_device = {};
    VkPhysicalDeviceProperties device_properties = {};
    VkPhysicalDeviceMemoryProperties mem_properties = {};
    uint32_t present_family_index = 0, graphics_family_index = 0;

    VkDevice device = {};
    VkExtent2D res = {};
    VkSurfaceFormatKHR surface_format = {};
    VkPresentModeKHR present_mode = {};
    SmallVector<VkImage, MaxFramesInFlight> present_images;
    SmallVector<VkImageView, MaxFramesInFlight> present_image_views;
    VkSwapchainKHR swapchain = {};

    uint32_t active_present_image = 0;

    VkQueue present_queue = {}, graphics_queue = {};

    VkCommandPool command_pool = {}, temp_command_pool = {};
    VkCommandBuffer setup_cmd_buf, draw_cmd_buf[MaxFramesInFlight];

    VkSemaphore image_avail_semaphores[MaxFramesInFlight] = {};
    VkSemaphore render_finished_semaphores[MaxFramesInFlight] = {};
    VkFence in_flight_fences[MaxFramesInFlight] = {};

    int backend_frame = 0;

    // resources scheduled for deferred destruction
    SmallVector<VkImage, 128> images_to_destroy[MaxFramesInFlight];
    SmallVector<VkImageView, 128> image_views_to_destroy[MaxFramesInFlight];
    SmallVector<MemAllocation, 128> allocs_to_free[MaxFramesInFlight];
    SmallVector<VkBuffer, 128> bufs_to_destroy[MaxFramesInFlight];
    SmallVector<VkBufferView, 128> buf_views_to_destroy[MaxFramesInFlight];
    SmallVector<VkDeviceMemory, 128> mem_to_free[MaxFramesInFlight];
};

class ILog;

bool InitVkInstance(VkInstance &instance, const char *enabled_layers[], int enabled_layers_count, ILog *log);
bool InitVkSurface(VkSurfaceKHR &surface, VkInstance instance, ILog *log);
bool ChooseVkPhysicalDevice(VkPhysicalDevice &physical_device, VkPhysicalDeviceProperties &device_properties,
                            VkPhysicalDeviceMemoryProperties &mem_properties, uint32_t &present_family_index,
                            uint32_t &graphics_family_index, const char *preferred_device, VkInstance instance,
                            VkSurfaceKHR surface, ILog *log);
bool InitVkDevice(VkDevice &device, VkPhysicalDevice physical_device, uint32_t present_family_index,
                  uint32_t graphics_family_index, const char *enabled_layers[], int enabled_layers_count, ILog *log);
bool InitSwapChain(VkSwapchainKHR &swapchain, VkSurfaceFormatKHR &surface_format, VkExtent2D &extent,
                   VkPresentModeKHR &present_mode, int w, int h, VkDevice device, VkPhysicalDevice physical_device,
                   uint32_t present_family_index, uint32_t graphics_family_index, VkSurfaceKHR surface, ILog *log);
bool InitCommandBuffers(VkCommandPool &command_pool, VkCommandPool &temp_command_pool, VkCommandBuffer &setup_cmd_buf,
                        VkCommandBuffer draw_cmd_buf[MaxFramesInFlight],
                        VkSemaphore image_avail_semaphores[MaxFramesInFlight],
                        VkSemaphore render_finished_semaphores[MaxFramesInFlight],
                        VkFence in_flight_fences[MaxFramesInFlight], VkQueue &present_queue, VkQueue &graphics_queue,
                        VkDevice device, uint32_t present_family_index, ILog *log);
bool InitImageViews(SmallVectorImpl<VkImage> &present_images, SmallVectorImpl<VkImageView> &present_image_views,
                    VkDevice device, VkSwapchainKHR swapchain, VkSurfaceFormatKHR surface_format,
                    VkCommandBuffer setup_cmd_buf, VkQueue present_queue, ILog *log);

VkCommandBuffer BegSingleTimeCommands(VkDevice device, VkCommandPool temp_command_pool);
void EndSingleTimeCommands(VkDevice device, VkQueue cmd_queue, VkCommandBuffer command_buf,
                           VkCommandPool temp_command_pool);
void EndSingleTimeCommands(VkDevice device, VkQueue cmd_queue, VkCommandBuffer command_buf,
                           VkCommandPool temp_command_pool, VkFence fence_to_insert);
void FreeSingleTimeCommandBuffer(VkDevice device, VkCommandPool temp_command_pool, VkCommandBuffer command_buf);

void DestroyDeferredResources(ApiContext *api_ctx, int i);
} // namespace Ren