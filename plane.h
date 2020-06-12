#ifndef PLANE_H
#define PLANE_H

#include <vector>
#include <utility>
#include <set>
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
    Plane(float x0_arg, float y0_arg, float x1_arg, float y1_arg, unsigned int gridsize_arg) noexcept;
    void add(const text_chunk_t &obj);
    void remove(const text_chunk_t &obj);
    std::set<text_chunk_t> find(float x0_arg, float y0_arg, float x1_arg, float y1_arg);
    std::set<text_chunk_t> get_objects() const;
    bool contains(const text_chunk_t &obj) const;
private:
    std::vector<std::pair<unsigned int, unsigned int>> get_range(float x0_arg,
                                                                 float y0_arg,
                                                                 float x1_arg,
                                                                 float y1_arg) const;

    std::map<std::pair<unsigned int, unsigned int>, std::vector<text_chunk_t>> grid;
    std::set<text_chunk_t> objs;
    unsigned int gridsize;
    float x0;
    float y0;
    float x1;
    float y1;
};

#endif //PLANE_H
