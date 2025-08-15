#include <vector>
#include <string>
#include <queue>
#include <cmath>
#include <limits>
#include <algorythm>
#include <unordered_map>
#include <read_nodes/nodes.hpp>


struct Point {
	int point_id;
	float point_length;
	bool operator>(const Point& other) const { return point_length > other.point_length; }
};