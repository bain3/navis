#include "utils.h"
#include <ctime>

void utils::print_message(const std::string &type, const int &color, const std::string &message) {
    std::time_t result = std::time(nullptr);
    std::string t(ctime(&result));
    std::cout << "[" << t.substr(0, t.length() - 1)
              << "][\033[" << color << "m" << type << "\033[0m] " << message << std::endl;
}
