#pragma once

#if defined(VKR_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(VKR_LINUX)
    #define VK_USE_PLATFORM_XLIB_KHR
#endif
#include <vulkan/vulkan.h>

#include <vector>
#include <span>

namespace vkr {

    struct ShaderDesc {
        void* pData;
        size_t size;
    };

    struct GraphicsPipelineDesc {
        VkShaderModule vertexShader;
        VkShaderModule fragmentShader;
    };

    struct GraphicsPipeline {
        VkPipeline pipeline;
        VkPipelineLayout layout;
        VkDescriptorSetLayout pushDescriptorSetLayout;
    };

    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct PresentationParameters {
        #if defined(VK_USE_PLATFORM_WIN32_KHR)
            HWND hWnd;
        #elif defined(VK_USE_PLATFORM_XLIB_KHR)
            _XDisplay* dpy;
            XID window;
        #endif
    };

    class Context {
    public:
        Context(const PresentationParameters& params);
        ~Context();

        void BeginFrame();
        void EndFrame();

        // TODO: testing only
        void Do(const GraphicsPipeline& pipeline);

        VkShaderModule CreateShader(const ShaderDesc& desc);
        GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc);
        
    private:
        // Find a suitable queue family index based on flags
        uint32_t FindQueueFamilyIndex(VkPhysicalDevice pd, VkQueueFlags flags);

        // (Re)create the swapchain
        void ValidateSwapchain();

        // Return a transient command buffer used for immediate execution
        VkCommandBuffer BeginImmediateCommands();

        // End commands and submit them for immediate execution
        void EndImmediateCommands(VkCommandBuffer cmds);

        void TransitionImageLayout(VkCommandBuffer cmds, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    private:
        // Core
        inline static uint32_t s_contextCount = 0;
        inline static VkInstance s_vkInstance = nullptr;
        VkPhysicalDevice m_physicalDevice = nullptr;
        VkDevice m_device = nullptr;
        VkQueue m_graphicsQueue = nullptr;
        VkQueue m_transferQueue = nullptr;
        VkFence m_frameInFlightFences[MAX_FRAMES_IN_FLIGHT] = {};
        uint32_t m_frameIndex = 0;

        // Presentation
        PresentationParameters m_presentParams;
        VkSurfaceKHR m_surface = nullptr;
        VkSwapchainKHR m_swapchain = nullptr;
        VkFence m_imageAcquiredFence = nullptr;
        std::vector<VkSemaphore> m_imageAcquiredSignals;
        std::vector<VkImage> m_swapchainImages;
        std::vector<VkImageView> m_swapchainImageViews;
        uint32_t m_swapchainImageIndex = 0;
        VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
        std::vector<VkSemaphore> m_presentReadySignals;

        // Ext
        VkCommandPool m_transientCommandPool = nullptr;

        // Render commands
        VkCommandPool m_graphicsCommandPool = nullptr;
        VkCommandBuffer m_graphicsCommandBuffers[MAX_FRAMES_IN_FLIGHT] = {};
    };

}