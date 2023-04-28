#include "file_utils.hpp"

#include <fstream>
#include <unordered_map>

const std::string MODEL_FOLDER = "../data/models/";

namespace W3D::fu {

const std::unordered_map<FileType, std::string> relative_paths = {
    {FileType::Shaders, "shaders/"},
    {FileType::Models, "../data/models/"},
};

std::vector<uint8_t> read_shader_binary(const std::string &file_name) {
    return read_binary(compute_abs_path(FileType::Shaders, file_name));
};

std::vector<uint8_t> read_binary(const std::string &path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file");
    };

    size_t file_size = (size_t)file.tellg();
    std::vector<uint8_t> buffer(file_size);

    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), file_size);
    file.close();

    return buffer;
}

std::string get_file_extension(const std::string &file_name) {
    auto extensionPos = file_name.find_last_of(".");
    if (extensionPos != std::string::npos) {
        return file_name.substr(extensionPos, +1);
    }
    return "";
}

const std::string compute_abs_path(const FileType type, const std::string &file) {
    return relative_paths.at(type) + file;
}

}  // namespace W3D::fu