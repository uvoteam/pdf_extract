#ifndef PLANE_H
#define PLANE_H

#include <vector>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <map>

#include "coordinates.h"

// Plane
//   A set-like data structure for objects placed on a plane.
//   Can efficiently find objects in a certain rectangular area.
//   It maintains two parallel lists of objects, each of
//   which is sorted by its x or y coordinate.

class Plane
{
public:
    Plane(double x0_arg, double y0_arg, double x1_arg, double y1_arg, unsigned int gridsize_arg) noexcept;
    void add(const text_chunk_t &obj);
    void remove(const text_chunk_t &obj);
    std::unordered_set<text_chunk_t> find(double x0_arg, double y0_arg, double x1_arg, double y1_arg);
    std::unordered_set<text_chunk_t> get_objects() const;
    bool contains(const text_chunk_t &obj) const;
private:
    std::vector<std::pair<unsigned int, unsigned int>> get_range(double x0_arg,
                                                                 double y0_arg,
                                                                 double x1_arg,
                                                                 double y1_arg) const;

    std::map<std::pair<unsigned int, unsigned int>, std::vector<text_chunk_t>> grid;
    std::unordered_set<text_chunk_t> objs;
    unsigned int gridsize;
    double x0;
    double y0;
    double x1;
    double y1;
};

#endif //PLANE_H
