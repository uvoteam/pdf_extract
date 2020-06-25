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
    coordinates_t() : x0(0), y0(0), x1(0), y1(0)
    {
    }

    coordinates_t(float x0_arg, float y0_arg, float x1_arg, float y1_arg) :
        x0(x0_arg), y0(y0_arg), x1(x1_arg), y1(y1_arg)
    {
    }

    bool operator==(const coordinates_t &obj) const
    {
        if (x0 != obj.x0 || y0 != obj.y0 || x1 != obj.x1 || y1 != obj.y1) return false;
        return true;
    }

    float x0;
    float y0;
    float x1;
    float y1;
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
    text_chunk_t() : is_empty(true)
    {
    }

    text_chunk_t(std::string &&text_arg, coordinates_t &&coordinates_arg) :
                 coordinates(std::move(coordinates_arg)),
                 texts{text_t(std::move(text_arg), coordinates)},
                 string_len(utf8_length(texts[0].text)),
                 is_empty(false)
    {
    }

    bool operator==(const text_chunk_t &obj) const
    {
        return (obj.coordinates == coordinates);
    }

    text_chunk_t& operator=(text_chunk_t &&arg)
    {
        coordinates = std::move(arg.coordinates);
        texts = std::move(arg.texts);
        string_len = arg.string_len;
        is_empty = arg.is_empty;
        arg.is_empty = true;
        return *this;
    }

    text_chunk_t& operator=(const text_chunk_t &arg) = default;

    text_chunk_t(text_chunk_t &&arg) :
                 coordinates(std::move(arg.coordinates)),
                 texts(std::move(arg.texts)),
                 string_len(arg.string_len),
                 is_empty(arg.is_empty)
    {
        arg.is_empty = true;
    }

    text_chunk_t(const text_chunk_t &arg) = default;

    bool operator<(const text_chunk_t &arg) const
    {
        if (coordinates.x0 != arg.coordinates.x0) return coordinates.x0 < arg.coordinates.x0;
        return coordinates.y0 < arg.coordinates.y0;
    }

    coordinates_t coordinates;
    std::vector<text_t> texts;
    size_t string_len;
    bool is_empty;
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
