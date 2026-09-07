#include "motion_bridge/release_version.hpp"

#include <array>
#include <charconv>
#include <string>

namespace motion_bridge {

std::optional<ReleaseVersion> parse_release_version(std::string_view text) {
    if (text.starts_with('v') || text.starts_with('V')) text.remove_prefix(1);

    std::array<unsigned int, 3> parts{};
    for (std::size_t index = 0; index < parts.size(); ++index) {
        const auto separator = text.find('.');
        const auto token = text.substr(0, separator);
        if (token.empty()) return std::nullopt;

        const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), parts[index]);
        if (error != std::errc{} || end != token.data() + token.size()) return std::nullopt;

        if (index + 1 == parts.size()) {
            if (separator != std::string_view::npos) return std::nullopt;
        } else {
            if (separator == std::string_view::npos) return std::nullopt;
            text.remove_prefix(separator + 1);
        }
    }
    return ReleaseVersion{parts[0], parts[1], parts[2]};
}

} // namespace motion_bridge
