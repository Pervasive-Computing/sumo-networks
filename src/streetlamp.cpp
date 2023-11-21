#include "streetlamp.hpp"
// #include "ansi-escape-codes.hpp"
// #include "pretty-printers.hpp"

#include <fmt/core.h>
#include <pugixml.hpp>

[[nodiscard]] auto pformat(const StreetLamp& lamp) -> std::string {
	return fmt::format("(StreetLamp) {{ .id = {}, .lat = {}, .lon = {} }}",
					   lamp.id, lamp.lat, lamp.lon);
}

auto pprint(const StreetLamp& lamp) -> void {
	fmt::println("{}", pformat(lamp));
}


[[nodiscard]] auto extract_streetlamps_from_osm(const std::filesystem::path& osm)
	-> extract_streetlamps_from_osm_result {
	if (! std::filesystem::exists(osm)) {
		return tl::make_unexpected(extract_streetlamps_from_osm_error::file_not_found);
	}
	pugi::xml_document	   doc;
	pugi::xml_parse_result result = doc.load_file(osm.c_str());

	if (! result) {
		return tl::make_unexpected(extract_streetlamps_from_osm_error::xml_parse_error);
	}

	auto streetlamps = std::vector<StreetLamp> {};

	// Traverse the XML to find street lamp nodes
	for (const auto node : doc.child("osm").children("node")) {
		for (const auto tag : node.children("tag")) {
			if (std::string(tag.attribute("k").value()) == "highway" &&
				std::string(tag.attribute("v").value()) == "street_lamp") {

				const auto id = std::stol(node.attribute("id").value());
				const auto lat = std::stod(node.attribute("lat").value());
				const auto lon = std::stod(node.attribute("lon").value());
				streetlamps.emplace_back(id, lat, lon);
				break; // No need to check other tags for this node
			}
		}
	}

	return streetlamps;
}
