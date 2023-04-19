#include <stdlib.h>

#include <exception>
#include <iostream>

#include "application.hpp"

int main() {
    W3D::Application app;
    try {
        app.start();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
};