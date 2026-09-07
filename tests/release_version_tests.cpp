#include "motion_bridge/release_version.hpp"

#include <cstdlib>
#include <iostream>

using motion_bridge::ReleaseVersion;
using motion_bridge::parse_release_version;

int main() {
    if (parse_release_version("v0.1.7") != ReleaseVersion{0, 1, 7} ||
        parse_release_version("1.10.0") != ReleaseVersion{1, 10, 0} ||
        parse_release_version("v1.2") ||
        parse_release_version("v1.2.3-beta") ||
        parse_release_version("release-1.2.3")) {
        std::cerr << "Release version parsing failed\n";
        return EXIT_FAILURE;
    }
    if (!(ReleaseVersion{0, 1, 8} > ReleaseVersion{0, 1, 7}) ||
        !(ReleaseVersion{1, 0, 0} > ReleaseVersion{0, 99, 99}) ||
        !(ReleaseVersion{0, 1, 7} == ReleaseVersion{0, 1, 7})) {
        std::cerr << "Release version comparison failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Release version tests passed\n";
    return EXIT_SUCCESS;
}
