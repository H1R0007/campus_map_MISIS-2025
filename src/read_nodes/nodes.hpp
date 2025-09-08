#include <vector>
#include <string>
#include <unordered_map>


// Структура параметров точек
class Node {
private:
int node_id;
	std::string node_name;
	float node_x, node_y;
	int node_floor;
	std::vector<std::pair<int, float>> neighbors;

	
	// Конструктор по умолчанию
	Node() = default;

	// Конструктор копирования
	Node(const Node&) = default;

	// Конструктор перемещения 
	Node(Node&&) = default;

	// Оператор присваивания копированием
	Node& operator=(const Node&) = default;

	// Оператор присваивания перемещением
	Node& operator=(Node&&) = default;

	// Деструктор 
	~Node() = default;
};


std::unordered_map<int, Nod> nodes_database;
