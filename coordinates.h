#ifndef COORDINATES_H
#define COORDINATES_H

#include <vector>
#include <string>
#include <stack>
#include <utility>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>

#include "common.h"
#include "fonts.h"

using point_t = boost::geometry::model::point<float, 2, boost::geometry::cs::cartesian>;
using box_t = boost::geometry::model::box<point_t>;

struct coordinates_t
{
    coordinates_t() : coordinates(point_t(0, 0), point_t(0, 0))
    {
    }

    coordinates_t(float x0_arg, float y0_arg, float x1_arg, float y1_arg) :
                  coordinates(point_t(x0_arg, y0_arg), point_t(x1_arg, y1_arg))
    {
    }
    bool operator==(const coordinates_t &obj) const
    {
        if (x0() != obj.x0() || y0() != obj.y0() || x1() != obj.x1() || y1() != obj.y1()) return false;
        return true;
    }
    void set_x0(const float x0_arg)
    {
        coordinates.min_corner().set<0>(x0_arg);
    }

    void set_y0(const float y0_arg)
    {
        coordinates.min_corner().set<1>(y0_arg);
    }

    void set_x1(const float x1_arg)
    {
        coordinates.max_corner().set<0>(x1_arg);
    }

    void set_y1(const float y1_arg)
    {
        coordinates.max_corner().set<1>(y1_arg);
    }

    float x0() const
    {
        return coordinates.min_corner().get<0>();
    }

    float x1() const
    {
        return coordinates.max_corner().get<0>();
    }

    float y0() const
    {
        return coordinates.min_corner().get<1>();
    }

    float y1() const
    {
        return coordinates.max_corner().get<1>();
    }
    box_t coordinates;
};

struct text_t
{
    text_t(std::string &&text_arg, const coordinates_t &coordinates_arg) noexcept :
           coordinates(coordinates_arg), text(std::move(text_arg))
    {
    }

    explicit text_t(const coordinates_t &coordinates_arg) : coordinates(coordinates_arg)
    {
    }
    bool operator==(const text_t &obj) const
    {
        if (!(coordinates == obj.coordinates)) return false;
        return text == obj.text;
    }
    text_t& operator=(text_t &&arg) = default;
    text_t& operator=(const text_t &arg) = default;
    text_t(text_t &&arg) = default;
    text_t(const text_t &arg) = default;

    coordinates_t coordinates;
    std::string text;
};

struct text_chunk_t
{
    text_chunk_t() : is_group(false)
    {
    }

    text_chunk_t(std::string &&text_arg, coordinates_t &&coordinates_arg) :
                 coordinates(std::move(coordinates_arg)),
                 texts{text_t(std::move(text_arg), coordinates)},
                 string_len(utf8_length(texts[0].text)),
                 is_group(false)
    {
    }
    bool operator==(const text_chunk_t &obj) const
    {
        return (obj.coordinates == coordinates);
    }
    text_chunk_t& operator=(text_chunk_t &&arg) = default;
    text_chunk_t& operator=(const text_chunk_t &arg) = default;
    text_chunk_t(text_chunk_t &&arg) = default;
    text_chunk_t(const text_chunk_t &arg) = default;
    bool operator<(const text_chunk_t &arg) const
    {
        if (coordinates.x0() != arg.coordinates.x0()) return coordinates.x0() < arg.coordinates.x0();
        return coordinates.y0() < arg.coordinates.y0();
    }
    std::pair<float, float> get_pair() const
    {
        return std::make_pair(coordinates.x0(), coordinates.y0());
    }
    coordinates_t coordinates;
    std::vector<text_t> texts;
    size_t string_len;
    bool is_group;
};

class Coordinates
{
public:
    Coordinates(const matrix_t &CTM);
    void set_default();
    matrix_t get_CTM() const;
    text_chunk_t adjust_coordinates(std::string &&s, size_t len, float width, float Tj, const Fonts &fonts);
    void set_coordinates(const std::string &token, std::stack<std::pair<pdf_object_t, std::string>> &st);
    void ctm_work(const std::string &token, std::stack<std::pair<pdf_object_t, std::string>> &st);
private:
    std::pair<float, float> get_coordinates(const matrix_t &m1, const matrix_t &m2) const;
    void T_quote();
    void T_star();
    void Td(float x, float y);
    enum
    {
        TH_DEFAULT = 1,
        TC_DEFAULT = 0,
        TW_DEFAULT = 0,
        TFS_DEFAULT = 1,
        TL_DEFAULT = 0,
        TRISE_DEFAULT = 0
    };
    matrix_t Tm;
    matrix_t CTM;
    float Tfs;
    float Th;
    float Tc;
    float Tw;
    float TL;
    float x, y;
    std::stack<matrix_t> CTMs;
};

#endif //COORDINATES_H
