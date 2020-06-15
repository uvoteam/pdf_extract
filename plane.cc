#include <algorithm>
#include <vector>

#include <boost/geometry.hpp>
#include <boost/geometry/index/adaptors/query.hpp>

#include "plane.h"

using namespace std;
using namespace boost::geometry::index;

Plane::Plane(const vector<text_chunk_t> &arg) : tree(arg)
{
}

bool Plane::contains(const text_chunk_t &obj) const
{
    return tree.count(obj);
}

const Plane::rtree_t& Plane::get_objects() const
{
    return tree;
}

void Plane::add(const text_chunk_t &obj)
{
    tree.insert(obj);
}

//Check if there's any other object between obj1 and obj2
bool Plane::is_any(const text_chunk_t &obj1, const text_chunk_t &obj2) const
{
    float x0 = min(obj1.coordinates.x0(), obj2.coordinates.x0());
    float y0 = min(obj1.coordinates.y0(), obj2.coordinates.y0());
    float x1 = max(obj1.coordinates.x1(), obj2.coordinates.x1());
    float y1 = max(obj1.coordinates.y1(), obj2.coordinates.y1());

    for(const text_chunk_t &obj : tree | adaptors::queried(overlaps(box_t(point_t(x0, y0), point_t(x1, y1)))))
    {
        if (!(obj == obj1) && !(obj == obj2)) return true;
    }
    return false;
}

void Plane::remove(const text_chunk_t &obj)
{
    tree.remove(obj);
}
