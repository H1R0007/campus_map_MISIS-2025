#include <vector>
#include <string>
#include <unordered_map>


# Структура параметров точек
struct Nod {
	int nod_id;
	std::string nod_name;
	float nod_x, nod_y;
	int nod_floor;
	std::vector<std::pair<int, float>> neighbors;
};


std::unordered_map<int, Nod> nods_database;
