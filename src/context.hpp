#pragma once

#include "resource.hpp"

#if defined(VKR_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(VKR_LINUX)
    #define VK_USE_PLATFORM_XLIB_KHR
#endif
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include <vector>
#include <span>

namespace vkr {

    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct BufferDesc {
        void* pData;
        size_t size;
        VkBufferUsageFlags usage;
    };

    using BufferHandle = ResourceID;

    struct ShaderDesc {
        void* pData;
        size_t size;
    };

    struct VertexAttrib {
        uint32_t binding;
        uint32_t offset;
        uint32_t stride;
        VkFormat format;
    };

    struct GraphicsPipelineDesc {
        std::vector<VertexAttrib> vertexAttribs;
        VkShaderModule vertexShader;
        VkShaderModule fragmentShader;
    };

    using GraphicsPipelineHandle = ResourceID;

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

        void BeginRendering(const VkViewport& viewport);
        void EndRendering();

        void SetVertexBuffers(std::span<BufferHandle> buffers);
        void SetIndexBuffer(BufferHandle bufferHandle, VkIndexType indexType);

        void SetGraphicsPipeline(GraphicsPipelineHandle pipelineHandle);
        void SetPushConstants(void* pData, size_t size, size_t offset);
        void SetPrimitiveTopology(VkPrimitiveTopology topology);
        void SetCullMode(VkCullModeFlags cullMode);

        void Draw(uint32_t offset, uint32_t count);
        void DrawIndexed(uint32_t offset, uint32_t count);

        BufferHandle CreateBuffer(const BufferDesc& desc);
        VkShaderModule CreateShader(const ShaderDesc& desc);
        GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc);
        
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
        struct GraphicsPipelineAllocation {
            VkPipeline pipeline;
            VkPipelineLayout layout;
            VkDescriptorSetLayout pushDescriptorSetLayout;
        };

        struct BufferAllocation {
            VkBuffer buffer;
            VmaAllocation alloc;
            VmaAllocationInfo allocInfo;
        };

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
        GraphicsPipelineAllocation* m_pBoundGraphicsPipeline = nullptr;
        
        // Resources
        VmaAllocator m_allocator = nullptr;


        
        ResourceRegistry<GraphicsPipelineAllocation> m_graphicsPipelines;
        ResourceRegistry<BufferAllocation> m_buffers;
    };

}