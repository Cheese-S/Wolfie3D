#include "utils.hpp"

#include <sstream>

namespace W3D {
std::string to_snake_case(const std::string &text) {
    std::stringstream result;
    for (const auto c : text) {
        if (std::isalpha(c)) {
            if (std::isspace(c)) {
                result << "_";
            } else {
                if (std::isupper(c)) {
                    result << "_";
                }

                result << std::tolower(c);
            }
        } else {
            result << c;
        }
    }

    return result.str();
}
}  // namespace W3D