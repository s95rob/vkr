#define VMA_IMPLEMENTATION
#include "context.hpp"

#include <cassert>
#include <vector>

#define VK_ASSERT_RESULT(exp, r) { VkResult vkResult = exp; assert(vkResult == r); }
#define VK_ASSERT(exp) VK_ASSERT_RESULT(exp, VK_SUCCESS)

namespace vkr {

    Context::Context(const PresentationParameters& params) 
        : m_presentParams(params) {
        // Create single VkInstance
        if (s_vkInstance == nullptr) {
            VkApplicationInfo ai = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = VK_API_VERSION_1_3
            };

            // Initialize instance layers
            std::vector<const char*> instanceLayerNames = {
                "VK_LAYER_KHRONOS_validation"
            };

            // Initialize instance extensions (OS surfaces, etc.)
            std::vector<const char*> instanceExtensionNames = {
                VK_KHR_SURFACE_EXTENSION_NAME,

                #if defined(VK_USE_PLATFORM_WIN32_KHR)
                    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
                #elif defined(VK_USE_PLATFORM_XLIB_KHR)
                    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
                #endif
            };


            VkInstanceCreateInfo ici = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo = &ai,
                .enabledLayerCount = static_cast<uint32_t>(instanceLayerNames.size()),
                .ppEnabledLayerNames = instanceLayerNames.data(),
                .enabledExtensionCount = static_cast<uint32_t>(instanceExtensionNames.size()),
                .ppEnabledExtensionNames = instanceExtensionNames.data()
            };

            VK_ASSERT(vkCreateInstance(&ici, nullptr, &s_vkInstance));
        }

        // Select a physical device
        // TODO: just grabbing the first one for now
        uint32_t pdc = 0;
        vkEnumeratePhysicalDevices(s_vkInstance, &pdc, nullptr);

        std::vector<VkPhysicalDevice> pds(pdc);
        vkEnumeratePhysicalDevices(s_vkInstance, &pdc, pds.data());
        m_physicalDevice = pds[0];

        // Create logical device

        // Find a graphics queue
        std::vector<float> graphicsQueuePriorities = { 1.0f };

        VkDeviceQueueCreateInfo gdqci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = FindQueueFamilyIndex(m_physicalDevice, VK_QUEUE_GRAPHICS_BIT),
            .queueCount = 1,
            .pQueuePriorities = graphicsQueuePriorities.data()
        };

        std::vector<VkDeviceQueueCreateInfo> dqcis = { gdqci };

        // Initialize device extensions
        std::vector<const char*> deviceExtensionNames = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
            VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        };

        // Device extension structs
        VkPhysicalDeviceDynamicRenderingFeatures pddrf = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .dynamicRendering = true
        };

        VkPhysicalDeviceSynchronization2Features pds2f = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .pNext = &pddrf,
            .synchronization2 = true
        };

        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT pdedsf = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
            .pNext = &pds2f,
            .extendedDynamicState = true
        };

        VkPhysicalDeviceDescriptorIndexingFeatures pddif = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
            .pNext = &pdedsf,
            .shaderSampledImageArrayNonUniformIndexing = true,
            .descriptorBindingPartiallyBound = true,
            .descriptorBindingVariableDescriptorCount = true,
            .runtimeDescriptorArray = true
        };

        // Initialize the device and create allocator
        VkDeviceCreateInfo dci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &pddif,
            .queueCreateInfoCount = static_cast<uint32_t>(dqcis.size()),
            .pQueueCreateInfos = dqcis.data(),
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensionNames.size()),
            .ppEnabledExtensionNames = deviceExtensionNames.data()
        };

        VK_ASSERT(vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_device));

        VmaAllocatorCreateInfo aci = {
            .vulkanApiVersion = VK_API_VERSION_1_3,
            .instance = s_vkInstance,
            .physicalDevice = m_physicalDevice,
            .device = m_device
        };

        VK_ASSERT(vmaCreateAllocator(&aci, &m_allocator));

        // Grab handles to the queues
        vkGetDeviceQueue(m_device, gdqci.queueFamilyIndex, 0, &m_graphicsQueue);

        // Create presentation resources
        ValidateSwapchain();    
        
        // Create command resources
        VkCommandPoolCreateInfo cpci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = gdqci.queueFamilyIndex
        };

        VK_ASSERT(vkCreateCommandPool(m_device, &cpci, nullptr, &m_graphicsCommandPool));

        cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

        VK_ASSERT(vkCreateCommandPool(m_device, &cpci, nullptr, &m_transientCommandPool));
        
        VkCommandBufferAllocateInfo cbai = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_graphicsCommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VK_ASSERT(vkAllocateCommandBuffers(m_device, &cbai, &m_graphicsCommandBuffers[i]));
        }

        // Create the frameInFlightFence already signalled so that the first frame can start
        VkFenceCreateInfo fci = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT   
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            VK_ASSERT(vkCreateFence(m_device, &fci, nullptr, &m_frameInFlightFences[i]));

        fci.flags = 0;
        VK_ASSERT(vkCreateFence(m_device, &fci, nullptr, &m_imageAcquiredFence));

        s_contextCount++;
    }

    Context::~Context() {
        vkDeviceWaitIdle(m_device);
        
        if (m_swapchain != nullptr) {
            //vkDestroySemaphore(m_device, m_imageAcquiredSignal, nullptr);

            for (auto& imageView : m_swapchainImageViews)
                vkDestroyImageView(m_device, imageView, nullptr);
            
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            vkDestroySurfaceKHR(s_vkInstance, m_surface, nullptr);
        }
        
        if (m_device != nullptr) {
            //vkDestroySemaphore(m_device, m_graphicsSubmitSignal, nullptr);
            //vkDestroyFence(m_device, m_frameInFlightFence, nullptr);
            vkDestroyDevice(m_device, nullptr);
        }
        
        // Final context destroyed, kill the instance
        if (s_vkInstance != nullptr && --s_contextCount == 0) {
            vkDestroyInstance(s_vkInstance, nullptr);
        }
    }

    void Context::BeginFrame() {
        // Wait until the last frame is finished before compiling more commands
        vkWaitForFences(m_device, 1, &m_frameInFlightFences[m_frameIndex], true, UINT64_MAX);
        vkResetFences(m_device, 1, &m_frameInFlightFences[m_frameIndex]);
        
        // Get the next swapchain image
        vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAcquiredSignals[m_frameIndex], nullptr, &m_swapchainImageIndex);

        // Start the graphics command buffer
        VkCommandBufferBeginInfo cbbi = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_ASSERT(vkBeginCommandBuffer(m_graphicsCommandBuffers[m_frameIndex], &cbbi));

        // Ready the swapchain image for rendering
        TransitionImageLayout(m_graphicsCommandBuffers[m_frameIndex], m_swapchainImages[m_swapchainImageIndex],
        m_swapchainFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    void Context::EndFrame() {
        // Ready the swapchain image to be presented
        TransitionImageLayout(m_graphicsCommandBuffers[m_frameIndex], m_swapchainImages[m_swapchainImageIndex],
            m_swapchainFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        // Finalize graphics commands
        VK_ASSERT(vkEndCommandBuffer(m_graphicsCommandBuffers[m_frameIndex]));

        // Submit commands to graphics queue
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo si = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_imageAcquiredSignals[m_frameIndex],
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &m_graphicsCommandBuffers[m_frameIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &m_presentReadySignals[m_swapchainImageIndex]
        };
        
        VK_ASSERT(vkQueueSubmit(m_graphicsQueue, 1, &si, m_frameInFlightFences[m_frameIndex]));

        VkResult presentResult;

        VkPresentInfoKHR pi = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_presentReadySignals[m_swapchainImageIndex],
            .swapchainCount = 1,
            .pSwapchains = &m_swapchain,
            .pImageIndices = &m_swapchainImageIndex,
            .pResults = &presentResult
        };

        VK_ASSERT(vkQueuePresentKHR(m_graphicsQueue, &pi));

        m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void Context::BeginRendering(const VkViewport& viewport) {
        VkRenderingAttachmentInfo rai = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = m_swapchainImageViews[m_swapchainImageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f }
        };

        int x = static_cast<int>(viewport.x);
        int y = static_cast<int>(viewport.y);
        uint32_t width = static_cast<uint32_t>(viewport.width);
        uint32_t height = static_cast<uint32_t>(viewport.height);

        VkRenderingInfo ri = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .layerCount = 1,
            .renderArea = {
                .offset = { x, y },
                .extent = { width, height }
            },
            .colorAttachmentCount = 1,
            .pColorAttachments = &rai
        };

        vkCmdBeginRendering(m_graphicsCommandBuffers[m_frameIndex], &ri);
        
        VkRect2D scissor = {
            .offset = { x, y },
            .extent = { width, height }
        };
        
        // Vulkan sizes the viewport -y up by default
        VkViewport vp = viewport;
        vp.y = vp.height;
        vp.height *= -1.0f; 
        vkCmdSetViewport(m_graphicsCommandBuffers[m_frameIndex], 0, 1, &vp);
        vkCmdSetScissor(m_graphicsCommandBuffers[m_frameIndex], 0, 1, &scissor);
    }

    void Context::EndRendering() {
        vkCmdEndRendering(m_graphicsCommandBuffers[m_frameIndex]);
    }

    void Context::SetVertexBuffers(std::span<BufferHandle> bufferHandles) {
        std::vector<VkBuffer> buffers;
        for (auto handle : bufferHandles) {
            buffers.push_back(m_buffers[handle].buffer);
        }

        // TODO: support offsets?
        VkDeviceSize offsets[16] = {};

        vkCmdBindVertexBuffers(m_graphicsCommandBuffers[m_frameIndex], 0,
            buffers.size(), buffers.data(), offsets);
    }

    void Context::SetIndexBuffer(BufferHandle bufferHandle, VkIndexType indexType) {
        BufferAllocation& buffer = m_buffers[bufferHandle];
        vkCmdBindIndexBuffer(m_graphicsCommandBuffers[m_frameIndex], buffer.buffer, 0, indexType);
    }
    
    void Context::SetGraphicsPipeline(GraphicsPipelineHandle pipelineHandle) {
        auto& pipeline = m_graphicsPipelines[pipelineHandle];
        vkCmdBindPipeline(m_graphicsCommandBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        m_pBoundGraphicsPipeline = &pipeline;
    }

    void Context::SetPushConstants(void* pData, size_t size, size_t offset) {
        vkCmdPushConstants(m_graphicsCommandBuffers[m_frameIndex], m_pBoundGraphicsPipeline->layout, 
            VK_SHADER_STAGE_ALL_GRAPHICS, offset, size, pData);
    }
    
    void Context::SetPrimitiveTopology(VkPrimitiveTopology topology) {
        vkCmdSetPrimitiveTopology(m_graphicsCommandBuffers[m_frameIndex], topology);
    }
    
    void Context::SetCullMode(VkCullModeFlags cullMode) {
        vkCmdSetCullMode(m_graphicsCommandBuffers[m_frameIndex], cullMode);
    }
    
    void Context::Draw(uint32_t offset, uint32_t count) {
        vkCmdDraw(m_graphicsCommandBuffers[m_frameIndex], count, 1, offset, 0);
    }

    void Context::DrawIndexed(uint32_t offset, uint32_t count) {
        vkCmdDrawIndexed(m_graphicsCommandBuffers[m_frameIndex], count, 1, offset, 0, 0);
    }
    
    BufferHandle Context::CreateBuffer(const BufferDesc& desc) {
        BufferHandle handle = m_buffers.Create();
        BufferAllocation& buffer = m_buffers[handle];

        // Create the device-local buffer
        VkBufferCreateInfo bci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = desc.size,
            .usage = desc.usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VmaAllocationCreateInfo aci = {
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        };

        VK_ASSERT(vmaCreateBuffer(m_allocator, &bci, &aci, &buffer.buffer, &buffer.alloc, &buffer.allocInfo));

        // Copy data from host to the buffer on device
        BufferAllocation stagingBuffer;

        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VK_ASSERT(vmaCreateBuffer(m_allocator, &bci, &aci, 
            &stagingBuffer.buffer, &stagingBuffer.alloc, 
            &stagingBuffer.allocInfo));

        memcpy(stagingBuffer.allocInfo.pMappedData, desc.pData, desc.size);

        VkCommandBuffer cmds = BeginImmediateCommands();

        VkBufferCopy bc = {
            .size = desc.size,
        };

        vkCmdCopyBuffer(cmds, stagingBuffer.buffer, buffer.buffer, 1, &bc);

        EndImmediateCommands(cmds);

        vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.alloc);

        return handle;
    }

    VkShaderModule Context::CreateShader(const ShaderDesc& desc) {
        VkShaderModule shader = nullptr;

        VkShaderModuleCreateInfo smci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = desc.size,
            .pCode = static_cast<const uint32_t*>(desc.pData)
        };

        VK_ASSERT(vkCreateShaderModule(m_device, &smci, nullptr, &shader));

        return shader;
    }

    GraphicsPipelineHandle Context::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) {
        GraphicsPipelineHandle handle = m_graphicsPipelines.Create();
        GraphicsPipelineAllocation& pipeline = m_graphicsPipelines[handle];

        // Setup pipeline layout
        VkDescriptorSetLayoutBinding uniformBinding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        };

        VkDescriptorSetLayoutBinding samplerBinding = {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        };

        VkDescriptorSetLayoutBinding descriptorSetBindings[] = {
            uniformBinding, samplerBinding
        };

        VkDescriptorSetLayoutCreateInfo dslci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT,
            .bindingCount = 2,
            .pBindings = descriptorSetBindings
        };

        VK_ASSERT(vkCreateDescriptorSetLayout(m_device,  &dslci, nullptr, &pipeline.pushDescriptorSetLayout));

        // TODO: poll for PCR size?
        VkPushConstantRange pcr = {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .size = 128
        };

        VkPipelineLayoutCreateInfo plci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &pipeline.pushDescriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pcr
        };

        VK_ASSERT(vkCreatePipelineLayout(m_device, &plci, nullptr, &pipeline.layout));

        // Setup dynamic pipeline states
        static const std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_CULL_MODE,
            VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
        };

        VkPipelineDynamicStateCreateInfo pdsci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        // Setup vertex input layout
        // TODO: for testing only- change with input layout descriptor
        std::vector<VkVertexInputBindingDescription> vibds;
        std::vector<VkVertexInputAttributeDescription> viads;

        uint32_t vertexLocation = 0;
        for (auto& attrib : desc.vertexAttribs) {
            VkVertexInputBindingDescription vibd = {
                .binding = attrib.binding,
                .stride = attrib.stride,
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            };

            VkVertexInputAttributeDescription viad = {
                .binding = attrib.binding,
                .location = vertexLocation++,
                .format = attrib.format,
                .offset = attrib.offset
            };

            vibds.push_back(vibd);
            viads.push_back(viad);
        };

        VkPipelineVertexInputStateCreateInfo pvisci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = static_cast<uint32_t>(vibds.size()),
            .pVertexBindingDescriptions = vibds.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(viads.size()),
            .pVertexAttributeDescriptions = viads.data()
        };

        // Setup (dynamic) input assembly
        VkPipelineInputAssemblyStateCreateInfo piasci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = false
        };

        // Setup (dynamic) viewport state
        VkPipelineViewportStateCreateInfo pvsci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1
        };

        // Setup (dynamic) rasterizer state
        VkPipelineRasterizationStateCreateInfo prsci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .lineWidth = 1.0f,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
        };

        // Setup multisampling state
        VkPipelineMultisampleStateCreateInfo pmssci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
        };

        // TODO: Setup depth stencil state 

        // Setup color blending state
        // TODO: for testing only- add attachment descriptor
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates;
        for (uint32_t i = 0; i < 1; i++) {
            VkPipelineColorBlendAttachmentState blendAttachment = {
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT | 
                    VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = true,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD
            };

            blendAttachmentStates.push_back(blendAttachment);
        }

        VkPipelineColorBlendStateCreateInfo pcbsci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size()),
            .pAttachments = blendAttachmentStates.data()
        };

        // Setup shader stages
        VkPipelineShaderStageCreateInfo pssciVertex = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = desc.vertexShader,
            .pName = "main"
        };

        VkPipelineShaderStageCreateInfo pssciFrag = pssciVertex;
        pssciFrag.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pssciFrag.module = desc.fragmentShader;

        VkPipelineShaderStageCreateInfo psscis[] = {
            pssciVertex,
            pssciFrag
        };

        // Setup dynamic rendering pipeline struct
        VkFormat colorFormat = m_swapchainFormat;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        VkPipelineRenderingCreateInfo prci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorFormat,
            //.depthAttachmentFormat = depthFormat
        };

        // Create the pipeline
        VkGraphicsPipelineCreateInfo gpci = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &prci,
            .stageCount = 2,
            .pStages = psscis,
            .pVertexInputState = &pvisci,
            .pInputAssemblyState = &piasci,
            .pViewportState = &pvsci,
            .pRasterizationState = &prsci,
            .pMultisampleState = &pmssci,
            // TODO: depth stencil state here
            .pColorBlendState = &pcbsci,
            .pDynamicState = &pdsci,
            .layout = pipeline.layout
        };

        // TODO: use pipeline cache?
        VK_ASSERT(vkCreateGraphicsPipelines(m_device, nullptr, 1, &gpci, nullptr, &pipeline.pipeline));

        return handle;
    }

    uint32_t Context::FindQueueFamilyIndex(VkPhysicalDevice pd, VkQueueFlags flags) {
        uint32_t qfpc = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfpc, nullptr);
        
        std::vector<VkQueueFamilyProperties> qfps(qfpc);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfpc, qfps.data());
        for (uint32_t i = 0; i < qfpc; i++) {
            auto& qfp = qfps[i];
            if (qfp.queueFlags & flags)
                return i;
        }

        // Failed to find a suitable queue family
        return UINT32_MAX;
    }

    void Context::ValidateSwapchain() {
        // Create the platform-specific surface from the window handle
        VkSurfaceKHR surface = nullptr;
        
        {
            #if defined(VK_USE_PLATFORM_XLIB_KHR)
                VkXlibSurfaceCreateInfoKHR sci = {
                    .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                    .dpy = m_presentParams.dpy,
                    .window = m_presentParams.window
                };
            
                VK_ASSERT(vkCreateXlibSurfaceKHR(s_vkInstance, &sci, nullptr, &surface));
            #endif
        }

        // Get surface capabilities
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, surface, &caps);
        
        // Get a suitable surface format
        uint32_t sfc = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, surface, &sfc, nullptr);
        
        std::vector<VkSurfaceFormatKHR> sfs(sfc);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, surface, &sfc, sfs.data());

        VkSurfaceFormatKHR surfaceFormat;
        
        // TODO: just prioritizing most common surface image formats (BGRA, ABGR or RGBA) for now
        for (auto& sf : sfs) {
            if (sf.format == VK_FORMAT_B8G8R8A8_SRGB ||
                sf.format == VK_FORMAT_A8B8G8R8_SRGB_PACK32 ||
                sf.format == VK_FORMAT_R8G8B8A8_SRGB) {
                surfaceFormat = sf;
                break;
            }
        }
        
        // Create the swapchain
        VkSwapchainKHR swapchain = nullptr;

        VkSwapchainCreateInfoKHR scci = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = caps.minImageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = caps.maxImageExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .preTransform = caps.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = true,
            .oldSwapchain = (m_swapchain == nullptr)
            ? nullptr
            : m_swapchain
        };

        m_swapchainFormat = scci.imageFormat;
        
        VK_ASSERT(vkCreateSwapchainKHR(m_device, &scci, nullptr, &swapchain));

        // Destroy the old swapchain resources if they already exist
        if (m_swapchain != nullptr) {
            vkDeviceWaitIdle(m_device);

            for (auto& signal : m_imageAcquiredSignals) {
                vkDestroySemaphore(m_device, signal, nullptr);
            }

            for (auto& imageView : m_swapchainImageViews) {
                vkDestroyImageView(m_device, imageView, nullptr);
            }

            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            vkDestroySurfaceKHR(s_vkInstance, m_surface, nullptr);

            m_swapchainImages.clear();
            m_swapchainImageViews.clear();
            m_imageAcquiredSignals.clear();
        }

        // Assign new swapchain resource handles
        m_surface = surface;
        m_swapchain = swapchain;

        // Gather new swapchain images
        uint32_t scic = 0;
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &scic, nullptr);
        
        m_swapchainImages.resize(scic);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &scic, m_swapchainImages.data());
        
        // Create new swapchain image views
        m_swapchainImageViews.resize(scic);
        for (uint32_t i = 0; i < m_swapchainImageViews.size(); i++) {
            VkImageViewCreateInfo ivci = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = m_swapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = surfaceFormat.format,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
                },
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1
                }
            };

            VK_ASSERT(vkCreateImageView(m_device, &ivci, nullptr, &m_swapchainImageViews[i]));
        }

        // Create swapchain signals
        VkSemaphoreCreateInfo sci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        m_imageAcquiredSignals.resize(MAX_FRAMES_IN_FLIGHT);
        for (auto& signal : m_imageAcquiredSignals)
            VK_ASSERT(vkCreateSemaphore(m_device, &sci, nullptr, &signal))

        m_presentReadySignals.resize(scic);
        for (auto& signal : m_presentReadySignals)
            VK_ASSERT(vkCreateSemaphore(m_device, &sci, nullptr, &signal));
    }

    VkCommandBuffer Context::BeginImmediateCommands() {
        VkCommandBuffer cmds;
        VkCommandBufferAllocateInfo cbai = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_transientCommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VK_ASSERT(vkAllocateCommandBuffers(m_device, &cbai, &cmds));

        VkCommandBufferBeginInfo cbbi = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        vkBeginCommandBuffer(cmds, &cbbi);

        return cmds;
    }

    void Context::EndImmediateCommands(VkCommandBuffer cmds) {
        vkEndCommandBuffer(cmds);

        VkSubmitInfo si = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmds
        };

        vkQueueSubmit(m_graphicsQueue, 1, &si, nullptr);

        // TODO: bad sync, use timeline semaphore later
        vkQueueWaitIdle(m_graphicsQueue);

        vkFreeCommandBuffers(m_device, m_transientCommandPool, 1, &cmds);
    }

    void Context::TransitionImageLayout(VkCommandBuffer cmds, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkImageMemoryBarrier2 imb = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1
            }
        };

        // Determine stages and access masks based on layouts
        switch (oldLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imb.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            imb.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imb.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            imb.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        default:
            imb.srcAccessMask = 0;
            imb.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            break;
        }

        switch (newLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imb.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            imb.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            imb.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            imb.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imb.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            imb.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        default:
            imb.dstAccessMask = 0;
            imb.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            break;
        }

        VkDependencyInfo di = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imb
        };

        vkCmdPipelineBarrier2(cmds, &di);
    }

}