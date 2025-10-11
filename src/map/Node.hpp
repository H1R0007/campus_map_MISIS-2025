#pragma once
#include <string>
#include <vector>

// === Node ===
// Represents a point in the navigation graph: room, entrance, stair etc.
// Holds position, building/floor info and list of neighbors.
struct Node {
    // --- Identity ---
    std::string id;            // unique node id (e.g. "A_1_ROOM_101")

    // --- Position ---
    int x = 0;
    int y = 0;
    int floor = 0;             // floor index (0 = campus level)

    // --- Context ---
    std::string building;      // building ID (e.g. "Building_A", "CAMPUS")
    bool isPortal = false;     // true if node is a portal (entrance, stair, lift...)

    // --- Graph edges ---
    std::vector<std::string> neighbors;   // connected node ids
};