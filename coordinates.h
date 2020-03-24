#ifndef COORDINATES_H
#define COORDINATES_H

#include <vector>
#include <string>
#include <stack>
#include <utility>

#include "common.h"
#include "pages_extractor.h"

using matrix_t = std::vector<std::vector<double>>;
struct coordinates_t
{
    coordinates_t() noexcept : start_x(0), start_y(0), end_x(0), end_y(0)
    {
    }
    coordinates_t(double start_x_arg, double start_y_arg, double end_x_arg, double end_y_arg) noexcept :
                  start_x(start_x_arg), start_y(start_y_arg), end_x(end_x_arg), end_y(end_y_arg)
    {
    }
    double start_x;
    double start_y;
    double end_x;
    double end_y;
};

class Coordinates
{
public:
    Coordinates(unsigned int rotate, const cropbox_t &cropbox);
    void set_CTM(std::stack<std::pair<pdf_object_t, std::string>> &st);
    void set_default();
    void push_CTM();
    void pop_CTM();
    coordinates_t adjust_coordinates(unsigned int width, size_t len, double Tj);
    void set_coordinates(const std::string &token, std::stack<std::pair<pdf_object_t, std::string>> &st);
private:
    std::pair<double, double> get_coordinates(double x, double y) const;
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
    coordinates_t coordinates;
    std::stack<matrix_t> CTMs;
};

#endif //COORDINATES_H
