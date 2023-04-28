#include "gltf_loader.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <queue>

#include "common/common.hpp"
#include "common/error.hpp"
#include "common/file_utils.hpp"
#include "common/utils.hpp"
#include "core/device.hpp"
#include "core/memory.hpp"
#include "scene_graph/components/camera.hpp"
#include "scene_graph/components/image.hpp"
#include "scene_graph/components/mesh.hpp"
#include "scene_graph/components/pbr_material.hpp"
#include "scene_graph/components/perspective_camera.hpp"
#include "scene_graph/components/sampler.hpp"
#include "scene_graph/components/submesh.hpp"
#include "scene_graph/components/texture.hpp"
#include "scene_graph/components/transform.hpp"
#include "scene_graph/node.hpp"
#include "scene_graph/scene.hpp"

namespace W3D {

template <class T, class Y>
struct TypeCast {
    Y operator()(T value) const noexcept {
        return static_cast<Y>(value);
    }
};

inline vk::Filter to_vk_min_filter(int min_filter) {
    switch (min_filter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            return vk::Filter::eNearest;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            return vk::Filter::eLinear;
        default:
            return vk::Filter::eLinear;
    }
}

inline vk::Filter to_vk_mag_filter(int mag_filter) {
    switch (mag_filter) {
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
            return vk::Filter::eLinear;
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            return vk::Filter::eNearest;
        default:
            return vk::Filter::eLinear;
    }
}

inline vk::SamplerMipmapMode to_vk_mipmap_mode(int mipmap_mode) {
    switch (mipmap_mode) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            return vk::SamplerMipmapMode::eNearest;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            return vk::SamplerMipmapMode::eLinear;
        default:
            return vk::SamplerMipmapMode::eLinear;
    }
}

inline vk::SamplerAddressMode to_vk_wrap_mode(int wrap_mode) {
    switch (wrap_mode) {
        case TINYGLTF_TEXTURE_WRAP_REPEAT:
            return vk::SamplerAddressMode::eRepeat;
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            return vk::SamplerAddressMode::eClampToEdge;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            return vk::SamplerAddressMode::eMirroredRepeat;
        default:
            return vk::SamplerAddressMode::eRepeat;
    }
}

inline vk::Format get_attribute_format(const tinygltf::Model *model, uint32_t accessor_id) {
    assert(accessor_id < model->accessors.size());
    auto &accessor = model->accessors[accessor_id];

    vk::Format format;

    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
            static const std::map<int, vk::Format> mapped_format = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR8Sint},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR8G8Sint},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR8G8B8Sint},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR8G8B8A8Sint}};

            format = mapped_format.at(accessor.type);

            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            static const std::map<int, vk::Format> mapped_format = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR8Uint},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR8G8Uint},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR8G8B8Uint},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR8G8B8A8Uint}};

            static const std::map<int, vk::Format> mapped_format_normalize = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR8Unorm},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR8G8Unorm},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR8G8B8Unorm},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR8G8B8A8Unorm}};

            if (accessor.normalized) {
                format = mapped_format_normalize.at(accessor.type);
            } else {
                format = mapped_format.at(accessor.type);
            }

            break;
        }
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            static const std::map<int, vk::Format> mapped_format = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR16Sint},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR16G16Sint},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR16G16B16Sint},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR16G16B16A16Sint}};

            format = mapped_format.at(accessor.type);

            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            static const std::map<int, vk::Format> mapped_format = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR16Uint},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR16G16Uint},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR16G16B16Uint},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR16G16B16A16Uint}};

            static const std::map<int, vk::Format> mapped_format_normalize = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR16Unorm},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR16G16Unorm},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR16G16B16Unorm},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR16G16B16A16Unorm}};
            if (accessor.normalized) {
                format = mapped_format_normalize.at(accessor.type);
            } else {
                format = mapped_format.at(accessor.type);
            }

            break;
        }
        case TINYGLTF_COMPONENT_TYPE_INT: {
            static const std::map<int, vk::Format> mapped_format = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR32Sint},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR32G32Sint},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR32G32B32Sint},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR32G32B32A32Sint}};

            format = mapped_format.at(accessor.type);

            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            static const std::map<int, vk::Format> mapped_format = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR32Uint},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR32G32Uint},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR32G32B32Uint},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR32G32B32A32Uint}};
            format = mapped_format.at(accessor.type);

            break;
        }
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            static const std::map<int, vk::Format> mapped_format = {
                {TINYGLTF_TYPE_SCALAR, vk::Format::eR32Sfloat},
                {TINYGLTF_TYPE_VEC2, vk::Format::eR32G32Sfloat},
                {TINYGLTF_TYPE_VEC3, vk::Format::eR32G32B32Sfloat},
                {TINYGLTF_TYPE_VEC4, vk::Format::eR32G32B32A32Sfloat}};

            format = mapped_format.at(accessor.type);

            break;
        }
        default: {
            format = vk::Format::eUndefined;
            break;
        }
    }

    return format;
}

inline std::vector<uint8_t> get_attribute_data(const tinygltf::Model *model, uint32_t accessor_id) {
    assert(accessor_id < model->accessors.size());
    auto &accessor = model->accessors[accessor_id];
    assert(accessor.bufferView < model->bufferViews.size());
    auto &buffer_view = model->bufferViews[accessor.bufferView];
    assert(buffer_view.buffer < model->buffers.size());
    auto &buffer = model->buffers[buffer_view.buffer];

    size_t stride = accessor.ByteStride(buffer_view);
    size_t start_byte = accessor.byteOffset + buffer_view.byteOffset;
    size_t end_byte = start_byte + accessor.count * stride;

    return {buffer.data.begin() + start_byte, buffer.data.begin() + end_byte};
}

inline std::vector<uint8_t> convert_underlying_data_stride(const std::vector<uint8_t> &src_data,
                                                           uint32_t src_stride,
                                                           uint32_t dst_stride) {
    auto elem_count = src_data.size() / src_stride;
    std::vector<uint8_t> result(elem_count * dst_stride);

    for (uint32_t src_idx = 0, dst_idx = 0; src_idx < src_data.size() && dst_idx < result.size();
         src_idx += src_stride, dst_idx += dst_stride) {
        std::copy(src_data.begin() + src_idx, src_data.begin() + src_idx + src_stride,
                  result.begin() + dst_idx);
    }

    return result;
}

inline void upload_image_to_device(vk::raii::CommandBuffer &command_buffer,
                                   DeviceMemory::Buffer &staging_buffer, SceneGraph::Image &image) {
    image.clear_data();
    vk::ImageMemoryBarrier begin_barrier;
    vk::ImageSubresourceRange subresource_range = {
        vk::ImageAspectFlagBits::eColor, 0, static_cast<uint32_t>(image.get_mipmaps().size()), 0,
        1};
    begin_barrier.oldLayout = vk::ImageLayout::eUndefined;
    begin_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    begin_barrier.image = image.get_vk_image().handle();
    begin_barrier.srcAccessMask = {};
    begin_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    begin_barrier.subresourceRange = subresource_range;
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost,
                                   vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                                   {begin_barrier});

    auto &mipmaps = image.get_mipmaps();

    std::vector<vk::BufferImageCopy> buffer_copy_regions(mipmaps.size());

    for (size_t i = 0; i < mipmaps.size(); i++) {
        auto &mipmap = mipmaps[i];
        auto &copy_region = buffer_copy_regions[i];

        copy_region.bufferOffset = mipmap.offset;
        copy_region.imageSubresource = {subresource_range.aspectMask, mipmap.level,
                                        subresource_range.baseArrayLayer,
                                        subresource_range.layerCount};
        copy_region.imageExtent = mipmap.extent;
    }

    command_buffer.copyBufferToImage(staging_buffer.handle(), image.get_vk_image().handle(),
                                     vk::ImageLayout::eTransferDstOptimal, buffer_copy_regions);

    vk::ImageMemoryBarrier end_barrier;
    end_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    end_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    end_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    end_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    end_barrier.subresourceRange = subresource_range;
    end_barrier.image = image.get_vk_image().handle();
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {},
                                   {end_barrier});
}

GLTFLoader::GLTFLoader(Device const &device) : device_(device) {
}

std::unique_ptr<SceneGraph::Scene> GLTFLoader::read_scene_from_file(const std::string &file_name,
                                                                    int scene_index) {
    std::string err;
    std::string warn;

    tinygltf::TinyGLTF gltf_loader;

    std::string gltf_file_path = fu::compute_abs_path(fu::FileType::Models, file_name);
    bool load_result;

    if (fu::get_file_extension(gltf_file_path) == "bin") {
        load_result =
            gltf_loader.LoadBinaryFromFile(&gltf_model_, &err, &warn, gltf_file_path.c_str());
    } else {
        load_result =
            gltf_loader.LoadASCIIFromFile(&gltf_model_, &err, &warn, gltf_file_path.c_str());
    }

    if (!load_result) {
        throw std::runtime_error("Unable to load gltf file.");
    }

    if (!err.empty()) {
        throw std::runtime_error(err);
    }

    if (!warn.empty()) {
        throw std::runtime_error(warn);
    }

    size_t pos = gltf_file_path.find_last_of('/');
    model_path_ = gltf_file_path.substr(0, pos);
    if (pos == std::string::npos) {
        model_path_.clear();
    }

    return std::make_unique<SceneGraph::Scene>(parse_scene(scene_index));
}

SceneGraph::Scene GLTFLoader::parse_scene(int scene_index) {
    auto scene = SceneGraph::Scene("gltf_scene");

    std::vector<std::unique_ptr<SceneGraph::Sampler>> sampler_components(
        gltf_model_.samplers.size());
    for (size_t sampler_index = 0; sampler_index < gltf_model_.samplers.size(); sampler_index++) {
        auto sampler = parse_sampler(gltf_model_.samplers[sampler_index]);
        sampler_components[sampler_index] = std::move(sampler);
    }
    scene.set_components(std::move(sampler_components));

    std::vector<std::unique_ptr<SceneGraph::Image>> image_components(gltf_model_.images.size());
    for (size_t image_index = 0; image_index < gltf_model_.images.size(); image_index++) {
        auto image = parse_image(gltf_model_.images[image_index]);
        image_components[image_index] = std::move(image);
    }

    size_t image_index = 0;
    size_t image_count = image_components.size();
    while (image_index < image_count) {
        std::vector<std::unique_ptr<DeviceMemory::Buffer>> staging_buffers;
        auto command_buffer = device_.beginOneTimeCommands();
        size_t batch_size = 0;

        while (image_index < image_count && batch_size < 64 * 1024 * 1024) {
            auto &pImage = image_components[image_index];
            std::unique_ptr<DeviceMemory::Buffer> pStaging_buffer =
                device_.get_allocator().allocateStagingBuffer(pImage->get_data().size());
            batch_size += pImage->get_data().size();
            pStaging_buffer->update(pImage->get_data());
            upload_image_to_device(command_buffer, *pStaging_buffer, *pImage);
            staging_buffers.push_back(std::move(pStaging_buffer));
            image_index++;
        }

        device_.endOneTimeCommands(command_buffer);
        device_.handle().waitIdle();
        staging_buffers.clear();  // Free all the staging buffers
    }
    scene.set_components(std::move(image_components));

    auto samplers = scene.get_components<SceneGraph::Sampler>();
    auto default_sampler = create_default_sampler();
    auto images = scene.get_components<SceneGraph::Image>();

    for (auto &gltf_texture : gltf_model_.textures) {
        auto texture = parse_texture(gltf_texture);

        assert(gltf_texture.source < images.size());
        texture->set_image(*images[gltf_texture.source]);

        if (gltf_texture.sampler >= 0 && gltf_texture.sampler < static_cast<int>(samplers.size())) {
            texture->set_sampler(*samplers[gltf_texture.sampler]);
        } else {
            if (gltf_texture.name.empty()) {
                gltf_texture.name = images[gltf_texture.source]->get_name();
            }

            texture->set_sampler(*default_sampler);
        }

        scene.add_component(std::move(texture));
    }

    scene.add_component(std::move(default_sampler));

    bool has_textures = scene.has_component<SceneGraph::Texture>();
    std::vector<SceneGraph::Texture *> textures;
    if (has_textures) {
        textures = scene.get_components<SceneGraph::Texture>();
    }

    for (auto &gltf_material : gltf_model_.materials) {
        auto material = parse_material(gltf_material);

        for (auto &gltf_value : gltf_material.values) {
            if (gltf_value.first.find("Texture") != std::string::npos) {
                std::string tex_name = to_snake_case(gltf_value.first);

                assert(gltf_value.second.TextureIndex() < textures.size());
                SceneGraph::Texture *tex = textures[gltf_value.second.TextureIndex()];

                // ? Convert texture to srgb

                material->textures_[tex_name] = tex;
            }
        }

        for (auto &gltf_value : gltf_material.additionalValues) {
            if (gltf_value.first.find("Texture") != std::string::npos) {
                std::string tex_name = to_snake_case(gltf_value.first);

                assert(gltf_value.second.TextureIndex() < textures.size());
                SceneGraph::Texture *tex = textures[gltf_value.second.TextureIndex()];

                // ? Convert texture to srgb

                material->textures_[tex_name] = tex;
            }
        }

        scene.add_component(std::move(material));
    }

    auto default_material = create_default_material();

    auto materials = scene.get_components<SceneGraph::PBRMaterial>();

    for (auto &gltf_mesh : gltf_model_.meshes) {
        auto mesh = parse_mesh(gltf_mesh);

        for (const auto &gltf_primitive : gltf_mesh.primitives) {
            auto submesh = parse_submesh_as_model(gltf_primitive);
            if (gltf_primitive.material < 0) {
                submesh->set_material(*default_material);
            } else {
                assert(gltf_primitive.material < materials.size());
                submesh->set_material(*materials[gltf_primitive.material]);
            }
            mesh->add_submesh(*submesh);
            scene.add_component(std::move(submesh));
        }

        scene.add_component(std::move(mesh));
    }

    for (auto &gltf_camera : gltf_model_.cameras) {
        auto camera = parse_camera(gltf_camera);
        scene.add_component(std::move(camera));
    }

    auto meshes = scene.get_components<SceneGraph::Mesh>();
    std::vector<std::unique_ptr<SceneGraph::Node>> nodes;

    for (size_t node_index = 0; node_index < gltf_model_.nodes.size(); node_index++) {
        auto gltf_node = gltf_model_.nodes[node_index];
        auto node = parse_node(gltf_node, node_index);

        if (gltf_node.mesh >= 0) {
            assert(gltf_node.mesh < meshes.size());
            auto mesh = meshes[gltf_node.mesh];
            node->set_component(*mesh);
            mesh->add_node(*node);
        }

        if (gltf_node.camera >= 0) {
            auto cameras = scene.get_components<SceneGraph::Camera>();
            assert(gltf_node.camera < cameras.size());
            auto camera = cameras[gltf_node.camera];

            node->set_component(*camera);
            camera->set_node(*node);
        }

        nodes.push_back(std::move(node));
    }

    std::queue<std::pair<SceneGraph::Node &, int>> traverse_nodes;

    tinygltf::Scene *gltf_scene{nullptr};

    if (scene_index >= 0 && scene_index < static_cast<int>(gltf_model_.scenes.size())) {
        gltf_scene = &gltf_model_.scenes[scene_index];
    } else if (gltf_model_.defaultScene >= 0 &&
               gltf_model_.defaultScene < static_cast<int>(gltf_model_.scenes.size())) {
        gltf_scene = &gltf_model_.scenes[gltf_model_.defaultScene];
    } else if (gltf_model_.scenes.size() > 0) {
        gltf_scene = &gltf_model_.scenes[0];
    }

    if (!gltf_scene) {
        throw std::runtime_error("Couldn't determine which scene to load!");
    }

    auto root_node = std::make_unique<SceneGraph::Node>(0, gltf_scene->name);

    for (auto node_index : gltf_scene->nodes) {
        traverse_nodes.push(std::make_pair(std::ref(*root_node), node_index));
    }

    while (!traverse_nodes.empty()) {
        auto node_it = traverse_nodes.front();
        traverse_nodes.pop();
        assert(node_it.second < nodes.size());
        auto &current_node = *nodes[node_it.second];
        auto &traverse_root_node = node_it.first;

        current_node.set_parent(traverse_root_node);
        traverse_root_node.add_child(current_node);

        for (auto child_node_index : gltf_model_.nodes[node_it.second].children) {
            traverse_nodes.push(std::make_pair(std::ref(current_node), child_node_index));
        }
    }

    scene.set_root_node(*root_node);
    nodes.push_back(std::move(root_node));
    scene.set_nodes(std::move(nodes));

    auto camera_node = std::make_unique<SceneGraph::Node>(-1, "default_camera");

    auto default_camera = create_default_camera();
    default_camera->set_node(*camera_node);
    camera_node->set_component(*default_camera);
    scene.add_component(std::move(default_camera));

    scene.get_root_node().add_child(*camera_node);
    scene.add_node(std::move(camera_node));

    // ? Add default lights

    return scene;
}

std::unique_ptr<SceneGraph::Sampler> GLTFLoader::parse_sampler(
    const tinygltf::Sampler &gltf_sampler) const {
    auto name = gltf_sampler.name;

    vk::Filter min_filter = to_vk_min_filter(gltf_sampler.minFilter);
    vk::Filter mag_filter = to_vk_mag_filter(gltf_sampler.magFilter);

    vk::SamplerMipmapMode mipmap_mode = to_vk_mipmap_mode(gltf_sampler.minFilter);

    vk::SamplerAddressMode address_mode_u = to_vk_wrap_mode(gltf_sampler.wrapS);
    vk::SamplerAddressMode address_mode_v = to_vk_wrap_mode(gltf_sampler.wrapT);
    vk::SamplerAddressMode address_mode_w = to_vk_wrap_mode(gltf_sampler.wrapS);

    vk::SamplerCreateInfo sampler_info{};
    sampler_info.minFilter = min_filter;
    sampler_info.magFilter = mag_filter;
    sampler_info.mipmapMode = mipmap_mode;
    sampler_info.addressModeU = address_mode_u;
    sampler_info.addressModeV = address_mode_v;
    sampler_info.addressModeW = address_mode_w;
    sampler_info.borderColor = vk::BorderColor::eIntOpaqueWhite;
    sampler_info.maxLod = std::numeric_limits<float>::max();

    vk::raii::Sampler sampler = device_.handle().createSampler(sampler_info);

    return std::make_unique<SceneGraph::Sampler>(name, std::move(sampler));
}

std::unique_ptr<SceneGraph::Sampler> GLTFLoader::create_default_sampler() {
    tinygltf::Sampler gltf_sampler;
    gltf_sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
    gltf_sampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;

    gltf_sampler.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
    gltf_sampler.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;

    return parse_sampler(gltf_sampler);
}

std::unique_ptr<SceneGraph::Image> GLTFLoader::parse_image(tinygltf::Image &gltf_image) const {
    std::unique_ptr<SceneGraph::Image> pImage = nullptr;

    if (!gltf_image.image.empty()) {
        auto mipmap = SceneGraph::Mipmap{0,
                                         0,
                                         {static_cast<uint32_t>(gltf_image.width),
                                          static_cast<uint32_t>(gltf_image.height), 1u}};
        std::vector<SceneGraph::Mipmap> mipmaps = {mipmap};
        pImage = std::make_unique<SceneGraph::Image>(gltf_image.name, std::move(gltf_image.image),
                                                     std::move(mipmaps));
    } else {
        auto image_uri = model_path_ + "/" + gltf_image.uri;
        pImage = SceneGraph::Image::load(gltf_image.name, image_uri);
    }

    pImage->create_vk_image(device_);

    return pImage;
}

std::unique_ptr<SceneGraph::Texture> GLTFLoader::parse_texture(
    const tinygltf::Texture &gltf_texture) const {
    return std::make_unique<SceneGraph::Texture>(gltf_texture.name);
}

std::unique_ptr<SceneGraph::PBRMaterial> GLTFLoader::parse_material(
    const tinygltf::Material &gltf_material) const {
    auto material = std::make_unique<SceneGraph::PBRMaterial>(gltf_material.name);

    for (auto &gltf_value : gltf_material.values) {
        if (gltf_value.first == "baseColorFactor") {
            const auto &color_factor = gltf_value.second.ColorFactor();
            material->base_color_factor_ =
                glm::vec4(color_factor[0], color_factor[1], color_factor[2], color_factor[3]);
        } else if (gltf_value.first == "metallicFactor") {
            material->metallic_factor = static_cast<float>(gltf_value.second.Factor());
        } else if (gltf_value.first == "roughnessFactor") {
            material->roughness_factor = static_cast<float>(gltf_value.second.Factor());
        }
    }

    for (auto &gltf_value : gltf_material.additionalValues) {
        if (gltf_value.first == "emissiveFactor") {
            const auto &emissive_factor = gltf_value.second.number_array;
            material->emissive_ =
                glm::vec3(emissive_factor[0], emissive_factor[1], emissive_factor[2]);
        } else if (gltf_value.first == "alphaMode") {
            if (gltf_value.second.string_value == "BLEND") {
                material->alpha_mode_ = SceneGraph::AlphaMode::Blend;
            } else if (gltf_value.second.string_value == "OPAQUE") {
                material->alpha_mode_ = SceneGraph::AlphaMode::Opaque;
            } else if (gltf_value.second.string_value == "MASK") {
                material->alpha_mode_ = SceneGraph::AlphaMode::Mask;
            }
        } else if (gltf_value.first == "alphaCutoff") {
            material->alpha_cutoff_ = static_cast<float>(gltf_value.second.number_value);
        } else if (gltf_value.first == "doubleSided") {
            material->is_double_sided = gltf_value.second.bool_value;
        }
    }

    return material;
}

std::unique_ptr<SceneGraph::PBRMaterial> GLTFLoader::create_default_material() {
    tinygltf::Material gltf_material;
    return parse_material(gltf_material);
}

std::unique_ptr<SceneGraph::Mesh> GLTFLoader::parse_mesh(const tinygltf::Mesh &gltf_mesh) const {
    return std::make_unique<SceneGraph::Mesh>(gltf_mesh.name);
}

std::unique_ptr<SceneGraph::SubMesh> GLTFLoader::parse_submesh_as_model(
    const tinygltf::Primitive &gltf_primitive) const {
    auto submesh = std::make_unique<SceneGraph::SubMesh>();
    std::vector<SceneGraph::Vertex> vertex_data;

    const float *pos = nullptr;
    const float *norm = nullptr;
    const float *uv = nullptr;

    auto &accessor = gltf_model_.accessors[gltf_primitive.attributes.find("POSITION")->second];
    size_t vertex_count = accessor.count;
    submesh->vertices_count_ = vertex_count;
    auto &buffer_view = gltf_model_.bufferViews[accessor.bufferView];
    pos =
        reinterpret_cast<const float *>(&(gltf_model_.buffers[buffer_view.buffer]
                                              .data[accessor.byteOffset + buffer_view.byteOffset]));

    if (gltf_primitive.attributes.find("NORMAL") != gltf_primitive.attributes.end()) {
        auto &accessor = gltf_model_.accessors[gltf_primitive.attributes.find("NORMAL")->second];
        auto &buffer_view = gltf_model_.bufferViews[accessor.bufferView];
        norm = reinterpret_cast<const float *>(
            &(gltf_model_.buffers[buffer_view.buffer]
                  .data[accessor.byteOffset + buffer_view.byteOffset]));
    }

    if (gltf_primitive.attributes.find("TEXCOORD_0") != gltf_primitive.attributes.end()) {
        auto &accessor =
            gltf_model_.accessors[gltf_primitive.attributes.find("TEXCOORD_0")->second];
        auto &buffer_view = gltf_model_.bufferViews[accessor.bufferView];
        uv = reinterpret_cast<const float *>(
            &(gltf_model_.buffers[buffer_view.buffer]
                  .data[accessor.byteOffset + buffer_view.byteOffset]));
    }

    for (size_t v = 0; v < vertex_count; v++) {
        SceneGraph::Vertex vert{};
        vert.pos = glm::vec3(glm::make_vec3(&pos[v * 3]));
        vert.norm =
            glm::normalize(glm::vec3(norm ? glm::make_vec3(&norm[v * 3]) : glm::vec3(0.0f)));
        vert.uv = uv ? glm::make_vec2(&uv[v * 2]) : glm::vec3(0.0f);
        vertex_data.push_back(vert);
    }
    std::vector<std::unique_ptr<DeviceMemory::Buffer>> transient_buffers;
    auto command_buffer = device_.beginOneTimeCommands();

    auto vert_staging_buffer =
        device_.get_allocator().allocateStagingBuffer(vertex_count * sizeof(SceneGraph::Vertex));
    vert_staging_buffer->update(vertex_data.data(), vertex_count * sizeof(SceneGraph::Vertex));
    auto vert_buffer =
        device_.get_allocator().allocateVertexBuffer(vertex_count * sizeof(SceneGraph::Vertex));
    vk::BufferCopy vertex_copy{0, 0, vertex_count * sizeof(SceneGraph::Vertex)};
    command_buffer.copyBuffer(vert_staging_buffer->handle(), vert_buffer->handle(), {vertex_copy});

    submesh->pVertex_buffer_ = std::move(vert_buffer);
    transient_buffers.push_back(std::move(vert_staging_buffer));

    if (gltf_primitive.indices >= 0) {
        auto &accessor = gltf_model_.accessors[gltf_primitive.indices];
        submesh->vertex_indices_ = accessor.count;

        auto format = get_attribute_format(&gltf_model_, gltf_primitive.indices);
        auto index_data = get_attribute_data(&gltf_model_, gltf_primitive.indices);

        switch (format) {
            case vk::Format::eR32Uint:
                break;
            case vk::Format::eR16Uint: {
                index_data = convert_underlying_data_stride(index_data, 2, 4);
                break;
            }
            case vk::Format::eR8Uint: {
                index_data = convert_underlying_data_stride(index_data, 1, 4);
                break;
            }
        }

        auto index_staging_buffer =
            device_.get_allocator().allocateStagingBuffer(index_data.size());
        index_staging_buffer->update(index_data);
        auto index_buffer = device_.get_allocator().allocateIndexBuffer(index_data.size());
        vk::BufferCopy index_copy(0, 0, index_data.size());
        command_buffer.copyBuffer(index_staging_buffer->handle(), index_buffer->handle(),
                                  {index_copy});
        submesh->pIndex_buffer_ = std::move(index_buffer);
        transient_buffers.push_back(std::move(index_staging_buffer));
    }

    device_.endOneTimeCommands(command_buffer);

    transient_buffers.clear();

    device_.handle().waitIdle();

    return std::move(submesh);
}

std::unique_ptr<SceneGraph::Camera> GLTFLoader::parse_camera(
    const tinygltf::Camera &gltf_camera) const {
    std::unique_ptr<SceneGraph::Camera> camera;

    if (gltf_camera.type == "perspective") {
        auto perspective_camera = std::make_unique<SceneGraph::PerspectiveCamera>(gltf_camera.name);

        perspective_camera->set_aspect_ratio(
            static_cast<float>(gltf_camera.perspective.aspectRatio));
        perspective_camera->set_field_of_view(static_cast<float>(gltf_camera.perspective.yfov));
        perspective_camera->set_near_plane(static_cast<float>(gltf_camera.perspective.znear));
        perspective_camera->set_far_plane(static_cast<float>(gltf_camera.perspective.zfar));

        camera = std::move(perspective_camera);
    } else {
        throw std::runtime_error("Camera type not supported");
    }

    return camera;
}

std::unique_ptr<SceneGraph::Camera> GLTFLoader::create_default_camera() {
    tinygltf::Camera gltf_camera;
    gltf_camera.name = "default_camera";
    gltf_camera.type = "perspective";

    gltf_camera.perspective.aspectRatio = 1.77f;
    gltf_camera.perspective.yfov = 1.0f;
    gltf_camera.perspective.znear = 0.1f;
    gltf_camera.perspective.zfar = 1000.0f;

    return parse_camera(gltf_camera);
}

std::unique_ptr<SceneGraph::Node> GLTFLoader::parse_node(const tinygltf::Node &gltf_node,
                                                         size_t index) const {
    auto node = std::make_unique<SceneGraph::Node>(index, gltf_node.name);

    auto &transform = node->get_component<SceneGraph::Transform>();

    if (!gltf_node.translation.empty()) {
        glm::vec3 translation;
        std::transform(gltf_node.translation.begin(), gltf_node.translation.end(),
                       glm::value_ptr(translation), TypeCast<double, float>{});
        transform.set_tranlsation(translation);
    }

    if (!gltf_node.rotation.empty()) {
        glm::quat rotation;
        std::transform(gltf_node.rotation.begin(), gltf_node.rotation.end(),
                       glm::value_ptr(rotation), TypeCast<double, float>{});
        transform.set_rotation(rotation);
    }

    if (!gltf_node.scale.empty()) {
        glm::vec3 scale;
        std::transform(gltf_node.scale.begin(), gltf_node.scale.end(), glm::value_ptr(scale),
                       TypeCast<double, float>{});
        transform.set_scale(scale);
    }

    if (!gltf_node.matrix.empty()) {
        glm::mat4 matrix;
        std::transform(gltf_node.matrix.begin(), gltf_node.matrix.end(), glm::value_ptr(matrix),
                       TypeCast<double, float>{});
        transform.set_world_M(matrix);
    }

    return node;
}
}  // namespace W3D