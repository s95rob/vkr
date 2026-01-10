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

constexpr uint32_t WINDOW_WIDTH = 1024;
constexpr uint32_t WINDOW_HEIGHT = 768;

struct MeshPart {
    vkr::BufferHandle vbo, nbo, uvbo, ibo;
    vkr::BufferHandle mbo; // TODO: material buffer- testing only
    uint32_t indexOffset, indexCount;
    VkIndexType indexType;
    uint32_t colorTextureIndex;
};

struct Mesh {
    std::vector<MeshPart> parts;
};

struct Material {
    glm::vec4 baseColor;
};

int main(int argc, char** argv) {
    // Setup window
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "vkr", nullptr, nullptr);

    // Load GLTF
    tinygltf::TinyGLTF gltfLoader;
    tinygltf::Model gltfModel;

    bool gltfLoadResult = gltfLoader.LoadBinaryFromFile(&gltfModel, nullptr, nullptr, "../res/Avocado.glb");
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
    gpd.vertexAttribs.push_back(attrib);    // Vertex positions

    attrib.binding = 1;
    gpd.vertexAttribs.push_back(attrib);    // Vertex normals

    attrib.binding = 2;
    attrib.format = VK_FORMAT_R32G32_SFLOAT;
    attrib.stride = sizeof(float) * 2;
    gpd.vertexAttribs.push_back(attrib);    // Vertex tex coords

    gpd.vertexShader = vs;
    gpd.fragmentShader = fs;

    vkr::GraphicsPipelineHandle pipeline = context->CreateGraphicsPipeline(gpd);

    // Create default sampler
    vkr::SamplerDesc smpd = {
        .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minFilter = VK_FILTER_LINEAR,
        .magFilter = VK_FILTER_LINEAR
    };

    vkr::SamplerHandle sampler = context->CreateSampler(smpd); 

    // Build GLTF scene buffers
    std::vector<Mesh> sceneMeshes;
    std::vector<vkr::TextureHandle> sceneTextures;

    for (auto& gltfMesh : gltfModel.meshes) {
        Mesh mesh = {};

        for (auto& primitive : gltfMesh.primitives) {
            MeshPart meshPart = {};

            auto& vertexPositionsAccessor = gltfModel.accessors[primitive.attributes["POSITION"]];
            auto& vertexNormalsAccessor = gltfModel.accessors[primitive.attributes["NORMAL"]];
            auto& vertexTexCoordsAccessor = gltfModel.accessors[primitive.attributes["TEXCOORD_0"]];
            auto& vertexIndicesAccessor = gltfModel.accessors[primitive.indices];

            auto& vertexPositionsView = gltfModel.bufferViews[vertexPositionsAccessor.bufferView];
            auto& vertexNormalsView = gltfModel.bufferViews[vertexNormalsAccessor.bufferView];
            auto& vertexTexCoordsView = gltfModel.bufferViews[vertexTexCoordsAccessor.bufferView];
            auto& vertexIndicesView = gltfModel.bufferViews[vertexIndicesAccessor.bufferView];

            auto& vertexPositionsBuffer = gltfModel.buffers[vertexPositionsView.buffer];
            auto& vertexNormalsBuffer = gltfModel.buffers[vertexNormalsView.buffer];
            auto& vertexTexCoordsBuffer = gltfModel.buffers[vertexTexCoordsView.buffer];
            auto& vertexIndicesBuffer = gltfModel.buffers[vertexIndicesView.buffer];

            
            meshPart.indexCount = vertexIndicesAccessor.count;
            
            // Build vertex buffer
            vkr::BufferDesc bd = {};
            bd.pData = reinterpret_cast<void*>(vertexPositionsBuffer.data.data() + vertexPositionsView.byteOffset + vertexPositionsAccessor.byteOffset);
            bd.size = vertexPositionsView.byteLength;
            bd.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            meshPart.vbo = context->CreateBuffer(bd);
            
            bd.pData = reinterpret_cast<void*>(vertexNormalsBuffer.data.data() + vertexNormalsView.byteOffset + vertexNormalsAccessor.byteOffset);
            bd.size = vertexNormalsView.byteLength;
            bd.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            meshPart.nbo = context->CreateBuffer(bd);

            bd.pData = reinterpret_cast<void*>(vertexTexCoordsBuffer.data.data() + vertexTexCoordsView.byteOffset + vertexTexCoordsAccessor.byteOffset);
            bd.size = vertexTexCoordsView.byteLength;
            bd.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            meshPart.uvbo = context->CreateBuffer(bd);

            // Build index buffer
            bd.pData = reinterpret_cast<void*>(vertexIndicesBuffer.data.data() + vertexIndicesView.byteOffset + vertexIndicesAccessor.byteOffset);
            bd.size = vertexIndicesView.byteLength;
            bd.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            meshPart.ibo = context->CreateBuffer(bd);
            
            // Build material buffer
            auto& material = gltfModel.materials[primitive.material];

            auto& baseColorFactor = material.pbrMetallicRoughness.baseColorFactor;
            Material materialData = {
                .baseColor = { baseColorFactor[0], baseColorFactor[1], baseColorFactor[2], baseColorFactor[3] }
            };

            bd.pData = &materialData;
            bd.size = sizeof(materialData);
            bd.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            meshPart.mbo = context->CreateBuffer(bd);

            // Resolve index type
            switch (vertexIndicesAccessor.componentType) {
            case GL_UNSIGNED_SHORT: meshPart.indexType = VK_INDEX_TYPE_UINT16; break;
            case GL_UNSIGNED_INT: meshPart.indexType = VK_INDEX_TYPE_UINT32; break;
            }

            // Get texture index
            meshPart.colorTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;

            mesh.parts.push_back(meshPart);
        }

        sceneMeshes.push_back(mesh);
    }

    for (auto& gltfImage : gltfModel.images) {
        vkr::TextureDesc td = {
            .width = (uint32_t)gltfImage.width,
            .height = (uint32_t)gltfImage.height,
            .format = VK_FORMAT_R8G8B8A8_SRGB
        };
        td.pData = reinterpret_cast<void*>(gltfImage.image.data());

        sceneTextures.push_back(context->CreateTexture(td));
    }

    float dt = 0.0f;
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Update delta time
        dt += 1.0f / 60.0f;

        context->BeginFrame();
        
        VkViewport viewport = {
            .width = WINDOW_WIDTH,
            .height = WINDOW_HEIGHT,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        context->BeginRendering(viewport);

        context->SetGraphicsPipeline(pipeline);
        context->SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        context->SetCullMode(VK_CULL_MODE_FRONT_BIT);

        // Build and push MVP matrix
        glm::mat4 viewProjectionMatrix = glm::perspectiveLH(glm::radians(75.0f), 800.0f / 600.0f, 0.01f, 1000.0f) *
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.025f, 0.075f));

        glm::mat4 modelMatrix = glm::rotate(glm::mat4(1.0f), dt, glm::vec3(0.0f, 1.0f, 0.0f));

        context->SetPushConstants(&modelMatrix, sizeof(glm::mat4), 0);
        context->SetPushConstants(&viewProjectionMatrix, sizeof(glm::mat4), sizeof(glm::mat4));

        // Draw GLTF scene
        for (auto& mesh : sceneMeshes) {
            for (auto& meshPart : mesh.parts) {
                vkr::BufferHandle vbos[] = { meshPart.vbo, meshPart.nbo, meshPart.uvbo };
                context->SetVertexBuffers(vbos);
                context->SetIndexBuffer(meshPart.ibo, meshPart.indexType);
                context->SetUniformBuffer(meshPart.mbo, 0);
                context->SetTexture(sceneTextures[meshPart.colorTextureIndex], sampler, 1);
                context->DrawIndexed(meshPart.indexOffset, meshPart.indexCount);
            }
        }

        context->EndRendering();
        context->EndFrame();
    }

    glfwTerminate();
    return 0;
}