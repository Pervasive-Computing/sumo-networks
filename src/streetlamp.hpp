#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>
#include <tl/expected.hpp>

struct StreetLamp {
	std::int64_t id;
	float lat;
	float lon;
};

[[nodiscard]]
auto pformat(const StreetLamp& lamp) -> std::string;
auto pprint(const StreetLamp& lamp) -> void;

enum class extract_streetlamps_from_osm_error {
    file_not_found,
    xml_parse_error,
};

using extract_streetlamps_from_osm_result = tl::expected<std::vector<StreetLamp>, extract_streetlamps_from_osm_error>;

[[nodiscard]]
auto extract_streetlamps_from_osm(const std::filesystem::path& osm) -> extract_streetlamps_from_osm_result;
