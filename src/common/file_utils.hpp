#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace W3D::fu
{

enum class FileType
{
	eShader,
	eModelAsset,
	eImage,
};

// Filesystem related utils. Decouple project structure from the rest of the application.
// All file utils expect relative path.
// Eg. read_shader_binary expects the filename to be a relative path from the shader directory.
std::vector<uint8_t> read_shader_binary(const std::string &filename);
std::vector<uint8_t> read_binary(const std::string &filename);
std::string          get_file_extension(const std::string &filename);
const std::string    compute_abs_path(const FileType type, const std::string &file);

}        // namespace W3D::fu