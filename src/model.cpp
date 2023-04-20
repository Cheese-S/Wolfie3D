#include "model.hpp"

#include <stdint.h>

#include <array>
#include <cstdint>
#include <memory>

#include "device.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/string_cast.hpp"
#include "glm/gtx/transform.hpp"
#include "memory.hpp"
#include "tiny_gltf.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"

namespace W3D::gltf {

Model::~Model() = default;

const VkBuffer Model::indexBufferHandle() { return indexBuffer_->handle(); }

const VkBuffer Model::vertexBufferHandle() { return vertexBuffer_->handle(); }

Model::Model(tinygltf::Model& raw, Device* pDevice, DeviceMemory::Allocator* pAllocator)
    : vertexCount_(0), indexCount_(0) {
    const tinygltf::Scene& scene = raw.scenes[raw.defaultScene != -1 ? raw.defaultScene : 0];
    Loader loader = createLoader(scene, raw);
    for (auto nodeIdx : scene.nodes) {
        roots_.push_back(loadNode(nodeIdx, nullptr, raw, loader));
    }
    createVertexAndIndexBuffer(loader, pDevice, pAllocator);
}

void Model::createVertexAndIndexBuffer(const Loader& loader, Device* pDevice,
                                       DeviceMemory::Allocator* pAllocator) {
    size_t vertexBufferSize = vertexCount_ * sizeof(Vertex);
    auto pVertexStagingBuffer = pAllocator->allocateStagingBuffer(vertexBufferSize);
    memcpy(pVertexStagingBuffer->mappedData(), loader.vertexBuffer.get(), vertexBufferSize);
    vertexBuffer_ = pAllocator->allocateVertexBuffer(vertexBufferSize);

    size_t indexBufferSize = indexCount_ * sizeof(uint32_t);
    auto pIndexStagingBuffer = pAllocator->allocateStagingBuffer(indexBufferSize);
    memcpy(pIndexStagingBuffer->mappedData(), loader.indexBuffer.get(), indexBufferSize);
    indexBuffer_ = pAllocator->allocateIndexBuffer(indexBufferSize);

    auto commandBuffer = pDevice->beginOneTimeCommands();

    vk::BufferCopy vertexCopyRegion;
    vertexCopyRegion.size = vertexBufferSize;
    commandBuffer.copyBuffer(pVertexStagingBuffer->handle(), vertexBuffer_->handle(),
                             vertexCopyRegion);

    vk::BufferCopy indexCopyRegion;
    indexCopyRegion.size = indexBufferSize;
    commandBuffer.copyBuffer(pIndexStagingBuffer->handle(), indexBuffer_->handle(),
                             indexCopyRegion);

    pDevice->endOneTimeCommands(commandBuffer);
}

Model::Loader Model::createLoader(const tinygltf::Scene& scene, const tinygltf::Model& model) {
    Loader loader;
    for (auto& nodeIdx : scene.nodes) {
        countNodeSize(model.nodes[nodeIdx], model);
    }
    loader.vertexBuffer = std::make_unique<Vertex[]>(vertexCount_);
    loader.indexBuffer = std::make_unique<uint32_t[]>(indexCount_);
    return loader;
}

void Model::countNodeSize(const tinygltf::Node& node, const tinygltf::Model& model) {
    if (node.mesh != -1) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (auto& primitive : mesh.primitives) {
            //* Note: POSITION is required for all vertex
            vertexCount_ += model.accessors[primitive.attributes.find("POSITION")->second].count;
            if (primitive.indices != -1) {
                indexCount_ += model.accessors[primitive.indices].count;
            }
        }
    }

    for (auto childIdx : node.children) {
        countNodeSize(model.nodes[childIdx], model);
    }
}

std::unique_ptr<Node> Model::loadNode(uint32_t idx, Node* pParent, const tinygltf::Model& model,
                                      Loader& loader) {
    const tinygltf::Node& raw = model.nodes[idx];
    auto node = std::make_unique<Node>();
    node->parent = pParent;
    node->idx = idx;
    node->m = calcLocalTransform(raw);

    if (raw.mesh > -1) {
        node->pMesh = loadMesh(raw.mesh, model, loader);
    }

    for (auto childIdx : raw.children) {
        node->children.push_back(loadNode(childIdx, node.get(), model, loader));
    }

    return node;
}

glm::mat4 Model::calcLocalTransform(const tinygltf::Node& node) {
    if (node.matrix.size()) {
        return glm::make_mat4x4(node.matrix.data());
    }

    glm::mat4 m = glm::mat4(1.0f);
    if (node.scale.size()) {
        m = glm::scale(m, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
    }

    if (node.rotation.size()) {
        glm::quat q = glm::make_quat(node.rotation.data());
        m = glm::mat4(q) * m;
        // m *= glm::mat4_cast(glm::make_quat(node.rotation.data()));
    }

    if (node.translation.size()) {
        m = glm::translate(
            m, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
    }
    return m;
}

std::unique_ptr<Mesh> Model::loadMesh(uint32_t idx, const tinygltf::Model& model, Loader& loader) {
    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
    const tinygltf::Mesh& raw = model.meshes[idx];
    for (const auto& primitive : raw.primitives) {
        uint32_t vertexStart = loader.vertexPos;
        uint32_t indexStart = loader.indexPos;

        AttrReader posReader = createAttrReader("POSITION", primitive, model);
        AttrReader normalReader = createAttrReader("NORMAL", primitive, model);
        AttrReader colorReader = createAttrReader("COLOR_0", primitive, model);

        uint32_t vertexCount = posReader.count;
        for (size_t v = 0; v < vertexCount; v++) {
            Vertex& vert = loader.vertexBuffer[loader.vertexPos];
            vert.pos = makePosVec(posReader, v);
            vert.normal = makeNormVec(normalReader, v);
            vert.color = makeColorVec(colorReader, v);
            loader.vertexPos++;
        }

        AttrReader idxReader = createIndexReader(primitive, model);
        uint32_t idxCount = idxReader.data ? idxReader.count : 0;
        if (idxReader.data) {
            switch (idxReader.type) {
                case ComponentType::UINT: {
                    const uint32_t* buffer = reinterpret_cast<const uint32_t*>(idxReader.data);
                    loadIndices(loader, buffer, idxCount, vertexStart);
                    break;
                }
                case ComponentType::USHORT: {
                    const uint16_t* buffer = reinterpret_cast<const uint16_t*>(idxReader.data);
                    loadIndices(loader, buffer, idxCount, vertexStart);
                    break;
                }
                case ComponentType::UBYTE: {
                    const uint8_t* buffer = static_cast<const uint8_t*>(idxReader.data);
                    loadIndices(loader, buffer, idxCount, vertexStart);
                    break;
                }
            }
        }

        mesh->primitives.push_back(std::make_unique<Primitive>(indexStart, idxCount, vertexCount));
    }
    return mesh;
}

Model::AttrReader Model::createAttrReader(const char* key, const tinygltf::Primitive& primitive,
                                          const tinygltf::Model& model) {
    auto it = primitive.attributes.find(key);
    AttrReader reader;
    if (it != primitive.attributes.end()) {
        const auto& accessor = model.accessors[it->second];
        const auto& view = model.bufferViews[accessor.bufferView];
        reader.data = &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
        reader.count = accessor.count;
        reader.componentStride = tinygltf::GetNumComponentsInType(accessor.type);
        reader.type = static_cast<ComponentType>(accessor.componentType);
    }
    return reader;
}

Model::AttrReader Model::createIndexReader(const tinygltf::Primitive& primitive,
                                           const tinygltf::Model& model) {
    AttrReader reader;
    if (primitive.indices != -1) {
        const auto& accessor = model.accessors[primitive.indices];
        const auto& view = model.bufferViews[accessor.bufferView];
        reader.data = &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
        reader.componentStride = 1;
        reader.type = static_cast<ComponentType>(accessor.componentType);
        reader.count = accessor.count;
    }
    return reader;
}

Primitive::Primitive(uint32_t firstIdx, uint32_t idxCount, uint32_t vertexCount)
    : firstIdx(firstIdx), idxCount(idxCount), vertexCount(vertexCount) {}

vk::VertexInputBindingDescription Vertex::bindingDescription() {
    vk::VertexInputBindingDescription bindingDescription(0, sizeof(Vertex),
                                                         vk::VertexInputRate::eVertex);
    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 3> Vertex::attributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 3> attrDescriptions;
    attrDescriptions[0] = {0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, pos)};
    attrDescriptions[1] = {1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, color)};
    attrDescriptions[2] = {2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)};
    return attrDescriptions;
}
}  // namespace W3D::gltf