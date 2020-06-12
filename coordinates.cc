#include <stack>
#include <utility>
#include <string>

#include "coordinates.h"
#include "common.h"
#include "fonts.h"

using namespace std;

namespace
{
    matrix_t translate_matrix(const matrix_t &m1, float x, float y)
    {
        float a = m1.at(0).at(0);
        float b = m1.at(0).at(1);
        float c = m1.at(1).at(0);
        float d = m1.at(1).at(1);
        float e = m1.at(2).at(0);
        float f = m1.at(2).at(1);
        return matrix_t{{a, b, 0},
                        {c, d, 0},
                        {x*a + y*c + e, x*b + y*d + f, 1}};
    }

    matrix_t get_matrix(stack<pair<pdf_object_t, string>> &st)
    {
        float f = stof(pop(st).second);
        float e = stof(pop(st).second);
        float d = stof(pop(st).second);
        float c = stof(pop(st).second);
        float b = stof(pop(st).second);
        float a = stof(pop(st).second);
        return matrix_t{{a, b, 0},
                        {c, d, 0},
                        {e, f, 1}};
    }
}

Coordinates::Coordinates(const matrix_t &CTM_arg):
    CTM(CTM_arg),
    Tm(IDENTITY_MATRIX),
    Tfs(TFS_DEFAULT),
    Th(TH_DEFAULT),
    Tc(TC_DEFAULT),
    Tw(TW_DEFAULT),
    TL(TL_DEFAULT),
    x(0),
    y(0)
{
    CTMs.push(CTM);
}

matrix_t Coordinates::get_CTM() const
{
    return CTM;
}

void Coordinates::T_quote()
{
    T_star();
}

void Coordinates::T_star()
{
    Td(0, -TL);
}

void Coordinates::Td(float x_arg, float y_arg)
{
    float a = Tm.at(0).at(0);
    float b = Tm.at(0).at(1);
    float c = Tm.at(1).at(0);
    float d = Tm.at(1).at(1);
    float e = Tm.at(2).at(0);
    float f = Tm.at(2).at(1);
    Tm = matrix_t{{a, b, 0},
                  {c, d, 0},
                  {x_arg*a + y_arg*c + e , x_arg*b + y_arg*d + f, 1}};
    x = 0;
    y = 0;
}

void Coordinates::set_default()
{
    Tm = IDENTITY_MATRIX;
    x = 0;
    y = 0;
}

pair<float, float> Coordinates::get_coordinates(const matrix_t &m1, const matrix_t &m2) const
{
    matrix_t r = m1 * m2;
    return make_pair(r[0][0], r[0][1]);
}

text_chunk_t Coordinates::adjust_coordinates(string &&s, size_t len, float width, float Tj, const Fonts &fonts)
{
    if (Tj != 0)
    {
        x -= Tj * Tfs * Th * 0.001;
        x += Tc * Th;
    }
    float ty = fonts.get_descent() * Tfs + fonts.get_rise() * Tfs;
    float adv = width * Tfs * Th;
    const matrix_t bll{{0, ty, 1}}, bur{{adv, ty + fonts.get_height() * Tfs, 1}};
    const matrix_t T_start = translate_matrix(Tm * CTM, x, y);
    if (len > 1) x += Tc * Th * (len - 1);
    for (char c : s)
    {
        if (c == ' ') x += Tw * Th;
    }
    const matrix_t T_end = translate_matrix(Tm * CTM, x, y);
    const pair<float, float> start_coordinates = get_coordinates(bll, T_start);
    const pair<float, float> end_coordinates = get_coordinates(bur, T_end);
    float x0 = min(start_coordinates.first, end_coordinates.first);
    float x1 = max(start_coordinates.first, end_coordinates.first);
    float y0 = min(start_coordinates.second, end_coordinates.second);
    float y1 = max(start_coordinates.second, end_coordinates.second);
    x += adv;
//    cout << s << " (" << x0 << ", " << y0 << ")(" << x1 << ", " << y1 << ") " << width << endl;
    return text_chunk_t(std::move(s), coordinates_t(x0, y0, x1, y1));
}

void Coordinates::ctm_work(const string &token, stack<pair<pdf_object_t, string>> &st)
{
    if (token == "cm") CTM = get_matrix(st) * CTM;
    else if (token == "q") CTMs.push(CTM);
    else if (token == "Q"  && !CTMs.empty()) CTM = pop(CTMs);
}

void Coordinates::set_coordinates(const string &token, stack<pair<pdf_object_t, string>> &st)
{
    if (token == "Tz")
    {
        //Th in percentages
        Th = stof(pop(st).second) / 100;
    }
    else if (token == "'")
    {
        T_quote();
    }
    else if (token == "\"")
    {
        Tc = stof(pop(st).second);
        Tw = stof(pop(st).second);
        T_quote();
    }
    else if (token == "TL")
    {
        TL = stof(pop(st).second);
    }
    else if (token == "T*")
    {
        T_star();
    }
    else if (token == "Tc")
    {
        Tc = stof(pop(st).second);
    }
    else if (token == "Tw")
    {
        Tw = stof(pop(st).second);
    }
    else if (token == "Td")
    {
        float y = stof(pop(st).second);
        float x = stof(pop(st).second);
        Td(x, y);
    }
    else if (token == "TD")
    {
        float y = stof(pop(st).second);
        float x = stof(pop(st).second);
        Td(x, y);
        TL = -y;
    }
    else if (token == "Tm")
    {
        Tm = get_matrix(st);
        x = 0;
        y = 0;
    }
    else if (token == "Tf")
    {
        Tfs = stof(pop(st).second);
    }
    else
    {
        throw pdf_error(FUNC_STRING + "unknown token:" + token);
    }
}
