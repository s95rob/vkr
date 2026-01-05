#include "context.hpp"
#include "util.hpp"

#if defined(VKR_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(VKR_LINUX)
    #define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <memory>

static float vertices[] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f
};


int main(int argc, char** argv) {
    // Setup window
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(800, 600, "vkr", nullptr, nullptr);

    vkr::PresentationParameters params = {};
    #if defined(VKR_LINUX)
        params.dpy = glfwGetX11Display();
        params.window = glfwGetX11Window(window);
    #endif

    // Setup context and pipeline
    std::shared_ptr<vkr::Context> context = std::make_shared<vkr::Context>(params);

    FileReader vsFile("test.vs.spv");
    FileReader fsFile("test.fs.spv");

    vkr::BufferDesc bd = {
        .pData = vertices,
        .size = sizeof(vertices),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    };

    vkr::BufferHandle vbo = context->CreateBuffer(bd);
    
    vkr::ShaderDesc sd;
    sd.pData = vsFile.Data();
    sd.size = vsFile.Size();
    VkShaderModule vs = context->CreateShader(sd);

    sd.pData = fsFile.Data();
    sd.size = fsFile.Size();
    VkShaderModule fs = context->CreateShader(sd);

    vkr::GraphicsPipelineDesc gpd = {};
    vkr::VertexAttrib attrib = {
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0,
        .stride = sizeof(float) * 3
    };
    gpd.vertexAttribs.push_back(attrib);

    gpd.vertexShader = vs;
    gpd.fragmentShader = fs;

    vkr::GraphicsPipelineHandle pipeline = context->CreateGraphicsPipeline(gpd);

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        context->BeginFrame();
        
        VkViewport viewport = {
            .width = 800,
            .height = 600,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        context->BeginRendering(viewport);

        context->SetGraphicsPipeline(pipeline);
        context->SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        context->SetCullMode(VK_CULL_MODE_NONE);

        std::vector<vkr::BufferHandle> vbos = { vbo };
        context->SetVertexBuffers(vbos);
        context->Draw(0, 3);

        context->EndRendering();
        context->EndFrame();
    }

    glfwTerminate();
    return 0;
}