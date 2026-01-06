#include "context.hpp"
#include "util.hpp"

#if defined(VKR_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(VKR_LINUX)
    #define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>

static float vertices[] = {
    -1.0f, -1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    0.0f, 1.0f, 0.0f
};

static uint32_t indices[] = {
    0, 1, 2
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

    bd.pData = indices;
    bd.size = sizeof(indices);
    bd.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    vkr::BufferHandle ibo = context->CreateBuffer(bd);
    
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

    float dt = 0.0f;
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Update delta time
        dt += 1.0f / 60.0f;

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

        // Build and push MVP matrix
        glm::mat4 mvp = glm::perspectiveLH(glm::radians(75.0f), 800.0f / 600.0f, 0.01f, 1000.0f) *
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 5.0f)) * 
            glm::rotate(glm::mat4(1.0f), dt, glm::vec3(0.0f, 1.0f, 0.0f));

        context->SetPushConstants(&mvp, sizeof(mvp), 0);

        std::vector<vkr::BufferHandle> vbos = { vbo };
        context->SetVertexBuffers(vbos);
        context->SetIndexBuffer(ibo, VK_INDEX_TYPE_UINT32);
        context->DrawIndexed(0, 3);

        context->EndRendering();
        context->EndFrame();
    }

    glfwTerminate();
    return 0;
}