#pragma once
#include <stdint.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "common.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "vulkan/vulkan_structs.hpp"

namespace tinygltf {
class Model;
class Node;
struct Scene;
struct Primitive;
}  // namespace tinygltf

namespace W3D {

class Device;

namespace DeviceMemory {
class Buffer;
class Allocator;
}  // namespace DeviceMemory

namespace gltf {

// glTF constants.
enum class ComponentType {
    BYTE = 5120,
    UBYTE = 5121,
    SHORT = 5122,
    USHORT = 5123,
    INT = 5124,
    UINT = 5125,
    FLOAT = 5126
};

struct Node;
struct Mesh;
struct Primitive;
struct Vertex;

class Model {
   public:
    Model(tinygltf::Model& raw, Device* pDevice, DeviceMemory::Allocator* pAllocator);
    ~Model();
    const VkBuffer vertexBufferHandle();
    const VkBuffer indexBufferHandle();

   private:
    struct Loader {
        std::unique_ptr<Vertex[]> vertexBuffer;
        std::unique_ptr<uint32_t[]> indexBuffer;
        size_t indexPos = 0;
        size_t vertexPos = 0;
    };

    struct AttrReader {
        const unsigned char* data = nullptr;
        size_t count;
        int componentStride;
        ComponentType type;
    };

    void createVertexAndIndexBuffer(const Loader& loader, Device* pDevice,
                                    DeviceMemory::Allocator* pAllocator);

    Loader createLoader(const tinygltf::Scene& scene, const tinygltf::Model& model);

    void getNodeProps(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount,
                      size_t& indexCount);

    std::unique_ptr<Node> loadNode(uint32_t idx, Node* pParent, const tinygltf::Model& model,
                                   Loader& loader);

    std::unique_ptr<Mesh> loadMesh(uint32_t idx, const tinygltf::Model& model, Loader& loader);

    AttrReader createAttrReader(const char* key, const tinygltf::Primitive& primitive,
                                const tinygltf::Model& model);

    AttrReader createIndexReader(const tinygltf::Primitive& primitive,
                                 const tinygltf::Model& model);

    inline glm::vec4 makePosVec(const AttrReader& reader, size_t v) {
        auto* buf = &(reinterpret_cast<const float*>(reader.data))[v * reader.componentStride];
        return glm::vec4(glm::make_vec3(buf), 1.0f);
    }

    inline glm::vec3 makeNormVec(const AttrReader& reader, size_t v) {
        if (!reader.data) {
            return glm::vec3(0.0f);
        }
        auto* buf = &(reinterpret_cast<const float*>(reader.data))[v * reader.componentStride];
        return glm::make_vec3(buf);
    }

    inline glm::vec4 makeColorVec(const AttrReader& reader, size_t v) {
        if (!reader.data) {
            return glm::vec4(1.0f);
        }
        auto* buf = &(reinterpret_cast<const float*>(reader.data))[v * reader.componentStride];
        return glm::make_vec4(buf);
    }

    template <typename T>
    void loadIndices(Model::Loader& loader, const T* buffer, size_t count, int start) {
        for (size_t idx = 0; idx < count; idx++) {
            loader.indexBuffer[loader.indexPos] = buffer[idx] + start;
            loader.indexPos++;
        }
    }

    glm::mat4 calcLocalTransform(const tinygltf::Node& node);

    std::vector<std::unique_ptr<Node>> roots_;
    std::unique_ptr<DeviceMemory::Buffer> vertexBuffer_;
    std::unique_ptr<DeviceMemory::Buffer> indexBuffer_;
};

struct Node {
    glm::mat4 m;
    uint32_t idx;
    Node* parent;
    std::unique_ptr<Mesh> pMesh;
    std::vector<std::unique_ptr<Node>> children;
};

struct Mesh {
    std::vector<std::unique_ptr<Primitive>> primitives;
};

struct Primitive {
    Primitive(uint32_t firstIdx, uint32_t idxCount, uint32_t vertexCount);
    uint32_t firstIdx;
    uint32_t idxCount;
    uint32_t vertexCount;
};

struct Vertex {
    glm::vec4 pos;
    glm::vec4 color;
    glm::vec3 normal;

    static std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions();
    static vk::VertexInputBindingDescription bindingDescription();
};
}  // namespace gltf
}  // namespace W3D