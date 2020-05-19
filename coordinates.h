#ifndef COORDINATES_H
#define COORDINATES_H

#include <vector>
#include <string>
#include <stack>
#include <utility>

#include "common.h"
#include "fonts.h"




struct coordinates_t
{
    coordinates_t() noexcept : x0(0), y0(0), x1(0), y1(0)
    {
    }
    coordinates_t(double x0_arg, double y0_arg, double x1_arg, double y1_arg) noexcept :
                  x0(x0_arg), y0(y0_arg), x1(x1_arg), y1(y1_arg)
    {
    }
    bool operator==(const coordinates_t &obj) const
    {
        if (obj.x0 != x0 || obj.y0 != y0 || obj.x1 != x1 || obj.y1 != y1) return false;
        return true;
    }
    double x0;
    double y0;
    double x1;
    double y1;
};

struct text_t
{
    text_t(std::string &&text_arg, const coordinates_t &coordinates_arg) noexcept :
           coordinates(coordinates_arg), text(std::move(text_arg))
    {
    }

    text_t(const coordinates_t &coordinates_arg) noexcept : coordinates(coordinates_arg)
    {
    }
    bool operator==(const text_t &obj) const
    {
        if (!(coordinates == obj.coordinates)) return false;
        return text == obj.text;
    }
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
        if (!(obj.coordinates == coordinates)) return false;
        if (!(obj.texts == texts)) return false;
        if (obj.is_group != is_group) return false;
        return true;
    }
    coordinates_t coordinates;
    std::vector<text_t> texts;
    size_t string_len;
    bool is_group;
};

namespace std
{
    template<> struct hash<text_chunk_t>
    {
        size_t operator()(const text_chunk_t &obj) const
        {
            if (!obj.texts.empty()) return hash<string>()(obj.texts[0].text);
            return 0;
        }
    };
}

class Coordinates
{
public:
    Coordinates(const matrix_t &CTM);
    void set_default();
    matrix_t get_CTM() const;
    text_chunk_t adjust_coordinates(std::string &&s, size_t len, double width, double Tj, const Fonts &fonts);
    void set_coordinates(const std::string &token, std::stack<std::pair<pdf_object_t, std::string>> &st);
    void ctm_work(const std::string &token, std::stack<std::pair<pdf_object_t, std::string>> &st);
private:
    std::pair<double, double> get_coordinates(const matrix_t &m1, const matrix_t &m2) const;
    void T_quote();
    void T_star();
    void Td(double x, double y);
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
    double Tfs;
    double Th;
    double Tc;
    double Tw;
    double TL;
    double x, y;
    std::stack<matrix_t> CTMs;
};

#endif //COORDINATES_H
