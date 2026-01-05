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
    
    vkr::ShaderDesc sd;
    sd.pData = vsFile.Data();
    sd.size = vsFile.Size();
    VkShaderModule vs = context->CreateShader(sd);

    sd.pData = fsFile.Data();
    sd.size = fsFile.Size();
    VkShaderModule fs = context->CreateShader(sd);

    vkr::GraphicsPipelineDesc gpd = {};
    gpd.vertexShader = vs;
    gpd.fragmentShader = fs;

    vkr::GraphicsPipeline pipeline = context->CreateGraphicsPipeline(gpd);

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        context->BeginFrame();
        context->Do(pipeline);
        context->EndFrame();
    }

    glfwTerminate();
    return 0;
}