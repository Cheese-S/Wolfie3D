#pragma once

#include <string>
#include <vector>

namespace W3D::fu {

enum class FileType {
    Shaders,
    Models,
};

std::vector<uint8_t> read_shader_binary(const std::string& filename);
std::vector<uint8_t> read_binary(const std::string& filename);
std::string get_file_extension(const std::string& filename);
const std::string compute_abs_path(const FileType type, const std::string& file);

}  // namespace W3D::fu