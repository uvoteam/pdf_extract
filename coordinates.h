#ifndef COORDINATES_H
#define COORDINATES_H

#include <vector>
#include <string>
#include <stack>
#include <utility>

#include "common.h"
#include "pages_extractor.h"
#include "fonts.h"

using matrix_t = std::vector<std::vector<double>>;


struct coordinates_t
{
    coordinates_t() noexcept : x0(0), y0(0), x1(0), y1(0)
    {
    }
    coordinates_t(double x0_arg, double y0_arg, double x1_arg, double y1_arg) noexcept :
                  x0(x0_arg), y0(y0_arg), x1(x1_arg), y1(y1_arg)
    {
    }
    double x0;
    double y0;
    double x1;
    double y1;
};

struct text_chunk_t
{
    text_chunk_t(std::string &&text_arg, const coordinates_t &coordinates_arg) noexcept :
                 coordinates(std::move(coordinates_arg)), text(std::move(text_arg))
    {
    }
    coordinates_t coordinates;
    std::string text;
};

struct text_line_t
{
    text_line_t(std::string &&text_arg, coordinates_t &&coordinates_arg) :
                coordinates(std::move(coordinates_arg)),
                chunks{text_chunk_t(std::move(text_arg), coordinates)},
                string_len(utf8_length(chunks[0].text))
    {
    }
    coordinates_t coordinates;
    std::vector<text_chunk_t> chunks;
    size_t string_len;
};

class Coordinates
{
public:
    Coordinates(unsigned int rotate, const mediabox_t &mediabox);
    void set_CTM(std::stack<std::pair<pdf_object_t, std::string>> &st);
    void set_default();
    void push_CTM();
    void pop_CTM();
    text_line_t adjust_coordinates(std::string &&s, size_t len, double width, double Tj, const Fonts &fonts);
    void set_coordinates(const std::string &token, std::stack<std::pair<pdf_object_t, std::string>> &st);
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
