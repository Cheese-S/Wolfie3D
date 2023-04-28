#pragma once

#include "scene_graph/component.hpp"

namespace W3D::SceneGraph {
class Image;
class Sampler;

class Texture : public Component {
   public:
    Texture(const std::string &name);
    Texture(Texture &&other) = default;

    virtual ~Texture() = default;
    virtual std::type_index get_type() override;

    void set_image(Image &image);
    void set_sampler(Sampler &sampler);

    Image *get_image();
    Sampler *get_sampler();

   private:
    Image *pImage_{nullptr};
    Sampler *pSampler_{nullptr};
};
};  // namespace W3D::SceneGraph