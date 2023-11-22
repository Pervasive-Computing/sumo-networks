#include <filesystem>
#include <iostream>
#include <pugixml.hpp>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <cstdint>


struct StreetLamp {
std::int64_t		   id;
	float lat;
	float lon;
};

auto pprint(const StreetLamp& lamp) -> void {
	std::printf("(StreetLamp) { .id = %ld, .lat = %lf, .lon = %lf }\n", lamp.id, lamp.lat, lamp.lon);
}



int main(int argc, char** argv) {
	pugi::xml_document	   doc;
	pugi::xml_parse_result result = doc.load_file(argv[1]);

	// doc.load_string(R"xml(
	//     <?xml version='1.0' encoding='UTF-8'?>
	//     <osm version='0.6' generator='JOSM'>
	//         <!-- your XML data here -->
	//     </osm>
	// )xml");

	if (! result) {
		std::cerr << "XML parsed with errors\n";
		return 1;
	}

	auto streetlamps = std::vector<StreetLamp> {};

	// TODO: use a XPath query instead of nested for loops
	// pugi::xpath_node_set tags = doc.select_nodes("//node/tag[@k='highway']");
	// pugi::xpath_node_set tags = doc.select_nodes("//node/@lat");

	// for (auto it = tags.begin(); it != tags.end(); ++it) {
	// 	auto node = *it;
	// 	for (const auto tag : node.children())

	// 	std::cout << "heelo " <<  node.node().attribute("k").value() << '\n';
	// }

	// Traverse the XML to find street lamp nodes
	for (const auto node : doc.child("osm").children("node")) {
		for (const auto tag : node.children("tag")) {
			if (std::string(tag.attribute("k").value()) == "highway" &&
				std::string(tag.attribute("v").value()) == "street_lamp") {

				const auto id = std::stol(node.attribute("id").value());
				const auto lat = std::stod(node.attribute("lat").value());
				const auto lon = std::stod(node.attribute("lon").value());

				streetlamps.emplace_back(id, lat, lon);

				// Found a street lamp, extract relevant information
				// std::cout << "Street Lamp: " << std::endl;
				// std::cout << "ID: " << node.attribute("id").value() << std::endl;
				// std::cout << "Latitude: " << node.attribute("lat").value() << std::endl;
				// std::cout << "Longitude: " << node.attribute("lon").value() << std::endl;
				// std::cout << std::endl;

				break; // No need to check other tags for this node
			}
		}
	}

	std::for_each(std::begin(streetlamps), std::end(streetlamps), pprint);

	return 0;
}
