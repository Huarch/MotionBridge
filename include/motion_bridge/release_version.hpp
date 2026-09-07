#pragma once

#include <compare>
#include <optional>
#include <string_view>

namespace motion_bridge {

struct ReleaseVersion {
    unsigned int major{};
    unsigned int minor{};
    unsigned int patch{};

    auto operator<=>(const ReleaseVersion&) const = default;
};

// Accept stable GitHub-style tags such as v0.1.7 or 0.1.7. Pre-release and
// malformed tags intentionally do not compare as installable updates.
[[nodiscard]] std::optional<ReleaseVersion> parse_release_version(std::string_view text);

} // namespace motion_bridge
