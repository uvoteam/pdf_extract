#include <vector>
#include <utility>
#include <string>
#include <algorithm>
#include <unordered_set>

#include "plane.h"

using namespace std;

namespace
{
    pair<unsigned int, unsigned int> drange(double v0, double v1, unsigned int d)
    {
        if (v0 >= v1) throw pdf_error(FUNC_STRING + "v0 must be < v1. v0=" + to_string(v0) + " v1=" + to_string(v1));
        return make_pair(static_cast<unsigned int>(v0) / d, static_cast<unsigned int>(v1 + d) / d);
    }
}

Plane::Plane(double x0_arg, double y0_arg, double x1_arg, double y1_arg, unsigned int gridsize_arg) noexcept :
             x0(x0_arg), y0(y0_arg), x1(x1_arg), y1(y1_arg), gridsize(gridsize_arg)
{
}

bool Plane::contains(const text_chunk_t &obj) const
{
    return objs.count(obj);
}

unordered_set<text_chunk_t> Plane::get_objects() const
{
    return objs;
}

vector<pair<unsigned int, unsigned int>> Plane::get_range(double x0_arg, double y0_arg, double x1_arg, double y1_arg) const
{
    if (x1_arg <= x0 || x1 <= x0_arg || y1_arg <= y0 || y1 <= y0_arg) return vector<pair<unsigned int, unsigned int>>();
    double x0_max = max(x0, x0_arg);
    double y0_max = max(y0, y0_arg);
    double x1_min = min(x1, x1_arg);
    double y1_min = min(y1, y1_arg);
    pair<unsigned int, unsigned int> y_range = drange(y0_max, y1_min, gridsize);
    pair<unsigned int, unsigned int> x_range = drange(x0_max, x1_min, gridsize);
    vector<pair<unsigned int, unsigned int>> result;
    for (unsigned int y = y_range.first; y < y_range.second; ++y)
    {
        for (unsigned int x = x_range.first; x < x_range.second; ++x) result.push_back(make_pair(x, y));
    }
    return result;
}

void Plane::add(const text_chunk_t &obj)
{
    const coordinates_t &coord = obj.coordinates;
    for (const pair<unsigned int, unsigned int> &k : get_range(coord.x0, coord.y0, coord.x1, coord.y1))
    {
        if (!grid.count(k)) grid.insert(make_pair(k, vector<text_chunk_t>()));
        grid[k].push_back(obj);
        objs.insert(obj);
    }
}

unordered_set<text_chunk_t> Plane::find(double x0_arg, double y0_arg, double x1_arg, double y1_arg)
{
    unordered_set<text_chunk_t> result;
    for (const pair<unsigned int, unsigned int> &k : get_range(x0_arg, y0_arg, x1_arg, y1_arg))
    {
        if (!grid.count(k)) continue;
        for (const text_chunk_t &obj : grid[k])
        {
            result.insert(obj);
            if (obj.coordinates.x1 <= x0_arg ||
                x1_arg <= obj.coordinates.x0 ||
                obj.coordinates.y1 <= y0_arg ||
                y1_arg <= obj.coordinates.y0) continue;
            result.insert(obj);
        }
    }
    return result;
}

void Plane::remove(const text_chunk_t &obj)
{
    const coordinates_t &coord = obj.coordinates;
    for (const pair<unsigned int, unsigned int> &k : get_range(coord.x0, coord.y0, coord.x1, coord.y1))
    {
        if (grid.count(k)) grid[k].erase(std::remove(grid[k].begin(), grid[k].end(), obj), grid[k].end());
        objs.erase(obj);
    }
}
