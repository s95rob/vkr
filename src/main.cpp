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

#include <tiny_gltf.h>

#include <memory>

struct MeshPart {
    vkr::BufferHandle vbo, ibo;
    uint32_t indexOffset, indexCount;
    VkIndexType indexType;
};

struct Mesh {
    std::vector<MeshPart> parts;
};

int main(int argc, char** argv) {
    // Setup window
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(800, 600, "vkr", nullptr, nullptr);

    // Load GLTF
    tinygltf::TinyGLTF gltfLoader;
    tinygltf::Model gltfModel;

    bool gltfLoadResult = gltfLoader.LoadBinaryFromFile(&gltfModel, nullptr, nullptr, "../res/Box.glb");
    assert(gltfLoadResult != false);

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

    // Build GLTF scene buffers
    std::vector<Mesh> sceneMeshes;

    for (auto& gltfMesh : gltfModel.meshes) {
        Mesh mesh = {};

        for (auto& primitive : gltfMesh.primitives) {
            MeshPart meshPart = {};

            auto& vertexPositionsAccessor = gltfModel.accessors[primitive.attributes["POSITION"]];
            auto& vertexIndicesAccessor = gltfModel.accessors[primitive.indices];

            auto& vertexPositionsView = gltfModel.bufferViews[vertexPositionsAccessor.bufferView];
            auto& vertexIndicesView = gltfModel.bufferViews[vertexIndicesAccessor.bufferView];

            auto& vertexPositionsBuffer = gltfModel.buffers[vertexPositionsView.buffer];
            auto& vertexIndicesBuffer = gltfModel.buffers[vertexIndicesView.buffer];

            meshPart.indexCount = vertexIndicesAccessor.count;

            // Build vertex buffer
            vkr::BufferDesc bd = {};
            bd.pData = reinterpret_cast<void*>(vertexPositionsBuffer.data.data() + vertexPositionsView.byteOffset + vertexPositionsAccessor.byteOffset);
            bd.size = vertexPositionsView.byteLength;
            bd.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            meshPart.vbo = context->CreateBuffer(bd);

            // Build index buffer
            bd.pData = reinterpret_cast<void*>(vertexIndicesBuffer.data.data() + vertexIndicesView.byteOffset + vertexIndicesAccessor.byteOffset);
            bd.size = vertexIndicesView.byteLength;
            bd.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            meshPart.ibo = context->CreateBuffer(bd);

            // Resolve index type
            switch (vertexIndicesAccessor.componentType) {
            case GL_UNSIGNED_SHORT: meshPart.indexType = VK_INDEX_TYPE_UINT16; break;
            case GL_UNSIGNED_INT: meshPart.indexType = VK_INDEX_TYPE_UINT32; break;
            }

            mesh.parts.push_back(meshPart);
        }

        sceneMeshes.push_back(mesh);
    }

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
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f)) * 
            glm::rotate(glm::mat4(1.0f), dt, glm::vec3(0.0f, 1.0f, 0.0f));

        context->SetPushConstants(&mvp, sizeof(mvp), 0);

        // Draw GLTF scene
        for (auto& mesh : sceneMeshes) {
            for (auto& meshPart : mesh.parts) {
                vkr::BufferHandle vbos[] = { meshPart.vbo };
                context->SetVertexBuffers(vbos);
                context->SetIndexBuffer(meshPart.ibo, meshPart.indexType);
                context->DrawIndexed(meshPart.indexOffset, meshPart.indexCount);
            }
        }

        context->EndRendering();
        context->EndFrame();
    }

    glfwTerminate();
    return 0;
}