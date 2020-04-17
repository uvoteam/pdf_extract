#include <stack>
#include <utility>
#include <string>

#include "coordinates.h"
#include "common.h"
#include "fonts.h"

using namespace std;

namespace
{
    matrix_t init_CTM(unsigned int rotate, const cropbox_t &cropbox)
    {
        if (rotate == 90) return matrix_t{{0, -1, 0},
                                         {1, 0, 0},
                                         {-cropbox.at(1), cropbox.at(2), 1}};
        if (rotate == 180) return matrix_t{{-1, 0, 0},
                                           {0, -1, 0},
                                           {cropbox.at(2), cropbox.at(3), 1}}; 
        if (rotate == 270) return matrix_t{{0, 1, 0},
                                           {-1, 0, 0},
                                           {cropbox.at(3), -cropbox.at(0), 1}};
        return matrix_t{{1, 0, 0},
                        {0, 1, 0},
                        {-cropbox.at(0), -cropbox.at(1), 0}};
    }

    matrix_t translate_matrix(const matrix_t &m1, double x, double y)
    {
        double a = m1.at(0).at(0);
        double b = m1.at(0).at(1);
        double c = m1.at(1).at(0);
        double d = m1.at(1).at(1);
        double e = m1.at(2).at(0);
        double f = m1.at(2).at(1);
        return matrix_t{{a, b, 0},
                        {c, d, 0},
                        {x*a + y*c + e, x*b + y*d + f, 1}};
    }

    matrix_t operator*(const matrix_t &m1, const matrix_t &m2)
    {
        matrix_t result(m1.size(), vector<double>(m2.at(0).size(), 0));
        for (size_t i = 0; i < m1.size(); i++)
        {
            for (size_t j = 0; j < m2.at(0).size(); j++)
            {
                result[i][j] = 0;
                for (size_t k = 0; k < m1.at(0).size(); k++) result[i][j] += m1[i][k] * m2.at(k)[j];
            }
        }
        return result;
    }

    matrix_t get_matrix(stack<pair<pdf_object_t, string>> &st)
    {
        double f = stod(pop(st).second);
        double e = stod(pop(st).second);
        double d = stod(pop(st).second);
        double c = stod(pop(st).second);
        double b = stod(pop(st).second);
        double a = stod(pop(st).second);
        return matrix_t{{a, b, 0},
                        {c, d, 0},
                        {e, f, 1}};
    }
}

Coordinates::Coordinates(unsigned int rotate, const cropbox_t &cropbox):
    CTM(init_CTM(rotate, cropbox)),
    Tm(matrix_t{{1, 0, 0},
                {0, 1, 0},
                {0, 0, 1}}),
    Tfs(TFS_DEFAULT),
    Th(TH_DEFAULT),
    Tc(TC_DEFAULT),
    Tw(TW_DEFAULT),
    TL(TL_DEFAULT),
    x(0),
    y(0)
{
}
void Coordinates::T_quote()
{
    T_star();
}

void Coordinates::T_star()
{
    Td(0, -TL);
}

void Coordinates::Td(double x_arg, double y_arg)
{
    double a = Tm.at(0).at(0);
    double b = Tm.at(0).at(1);
    double c = Tm.at(1).at(0);
    double d = Tm.at(1).at(1);
    double e = Tm.at(2).at(0);
    double f = Tm.at(2).at(1);
    Tm = matrix_t{{a, b, 0},
                  {c, d, 0},
                  {x_arg*a + y_arg*c + e , x_arg*b + y_arg*d + f, 1}};
    x = 0;
    y = 0;
}

void Coordinates::set_default()
{
            Tm = matrix_t{{1, 0, 0},
                          {0, 1, 0},
                          {0, 0, 1}};
            Th = TH_DEFAULT;
            Tc = TC_DEFAULT;
            Tw = TW_DEFAULT;
            Tfs = TFS_DEFAULT;
            TL = TL_DEFAULT;
}

void Coordinates::set_CTM(stack<pair<pdf_object_t, string>> &st)
{
    CTM = get_matrix(st);
}

pair<double, double> Coordinates::get_coordinates(const matrix_t &m1, const matrix_t &m2) const
{
    matrix_t r = m1 * m2;
    return make_pair(r[0][0], r[0][1]);
}

text_line_t Coordinates::adjust_coordinates(string &&s, size_t len, double width, double Tj, const Fonts &fonts)
{
    if (Tj != 0)
    {
        x -= Tj * Tfs * Th * 0.001;
        x += Tc * Th;
    }
    double ty = fonts.get_descent() * Tfs + fonts.get_rise() * Tfs;
    double adv = width * Tfs * Th;
    const matrix_t bll{{0, ty, 1}}, bur{{adv, ty + fonts.get_height() * Tfs, 1}};
    const matrix_t T_start = translate_matrix(Tm * CTM, x, y);
    if (len > 1) x += Tc * Th * (len - 1);
    const matrix_t T_end = translate_matrix(Tm * CTM, x, y);
    const pair<double, double> start_coordinates = get_coordinates(bll, T_start);
    const pair<double, double> end_coordinates = get_coordinates(bur, T_end);
    double x0 = min(start_coordinates.first, end_coordinates.first);
    double x1 = max(start_coordinates.first, end_coordinates.first);
    double y0 = min(start_coordinates.second, end_coordinates.second);
    double y1 = max(start_coordinates.second, end_coordinates.second);
    x += adv;
//    cout << '(' << x0 << ", " << y0 << ")(" << x1 << ", " << y1 << ") " << endl;
    for (char c : s)
    {
        if (c == ' ') x += Tw * Th;
    }
    return text_line_t(std::move(s), coordinates_t(x0, y0, x1, y1));
}

void Coordinates::set_coordinates(const string &token, stack<pair<pdf_object_t, string>> &st)
{
    if (token == "Tz")
    {
        //Th in percentages
        Th = stod(pop(st).second) / 100;
    }
    else if (token == "'")
    {
        T_quote();
    }
    else if (token == "\"")
    {
        Tc = stod(pop(st).second);
        Tw = stod(pop(st).second);
        T_quote();
    }
    else if (token == "TL")
    {
        TL = stod(pop(st).second);
    }
    else if (token == "T*")
    {
        T_star();
    }
    else if (token == "Tc")
    {
        Tc = stod(pop(st).second);
    }
    else if (token == "Tw")
    {
        Tw = stod(pop(st).second);
    }
    else if (token == "Td")
    {
        double y = stod(pop(st).second);
        double x = stod(pop(st).second);
        Td(x, y);
    }
    else if (token == "TD")
    {
        double y = stod(pop(st).second);
        double x = stod(pop(st).second);
        Td(x, y);
        TL = -y;
    }
    else if (token == "Tm")
    {
        double f = stod(pop(st).second);
        double e = stod(pop(st).second);
        double d = stod(pop(st).second);
        double c = stod(pop(st).second);
        double b = stod(pop(st).second);
        double a = stod(pop(st).second);
        Tm = matrix_t{{a, b, 0},
                      {c, d, 0},
                      {e, f, 1}};
        x = 0;
        y = 0;
    }
    else if (token == "Tf")
    {
        Tfs = stod(pop(st).second);
    }
    else
    {
        throw pdf_error(FUNC_STRING + "unknown token:" + token);
    }
}

void Coordinates::push_CTM()
{
    CTMs.push(CTM);
}

void Coordinates::pop_CTM()
{
    CTM = pop(CTMs);
}
