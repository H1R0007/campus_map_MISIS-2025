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


	// Конструктор по умолчанию
	Point() = default;

	// Конструктор через параметры
	Point(int _point_id, float _point_length) : point_id(_point_id), point_length(_point_length) {}

	// Оператор сравнения для очереди по приоритету
	bool operator>(const Point& other) const { return point_length > other.point_length; }

	// Деструктор
	~Point() = default;
};