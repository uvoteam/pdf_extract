#ifndef PLANE_H
#define PLANE_H

#include <vector>

#include <boost/geometry/index/rtree.hpp>

#include "coordinates.h"

class Plane
{
public:
    struct indexable_t
    {
        using result_type = const box_t;
        const box_t& operator()(text_chunk_t const &v) const
        {
            return v.coordinates.coordinates;
        }
    };
    struct cmp_t
    {
        bool operator()(const text_chunk_t &v1, const text_chunk_t &v2) const
        {
            return v1 == v2;
        }
    };
    using rtree_t = boost::geometry::index::rtree<text_chunk_t, boost::geometry::index::quadratic<16>, indexable_t, cmp_t>;
public:
    Plane(const std::vector<text_chunk_t> &arg);
    void add(const text_chunk_t &obj);
    void remove(const text_chunk_t &obj);
    bool is_any(const text_chunk_t &obj1, const text_chunk_t &obj2) const;
    const rtree_t& get_objects() const;
private:
    rtree_t tree;
};

#endif //PLANE_H
