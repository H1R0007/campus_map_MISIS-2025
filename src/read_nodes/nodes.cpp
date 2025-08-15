#include "path_finder/nodes.hpp"
using json = nlohmann::json;


static void load_nods(const std::string& jsonStr) {
	auto dat = json::parse(jsonStr);
	for (auto& nod_dat : dat["nods"]) {
		Nod n;
		n.nod_id = nod_dat["id"];
		n.nod_name = nod_dat["name"];
		n.nod_x = nod_dat["x"];
		n.nod_y = nod_dat["y"];
		n.nod_floor = nod_dat["floor"];


		for (auto& neighbor : nod_dat["neighbors"]) {
			n.neighbors.push_back({ neighbor["id"], neighbor["distance"] });
		}

		nods_database[n.id] = n;
	}
}