#include <utility>
#include <string>
#include <vector>

#include "coordinates.h"
#include "common.h"
#include "fonts.h"

using namespace std;

namespace
{
    matrix_t translate_matrix(const matrix_t &m, float x, float y)
    {
        return matrix_t{m[0], m[1], m[2], m[3], x * m[0] + y * m[2] + m[4], x * m[1] + y * m[3] + m[5]};
    }

    matrix_t get_matrix(vector<pair<pdf_object_t, string>> &st)
    {
        float f = stof(pop(st).second);
        float e = stof(pop(st).second);
        float d = stof(pop(st).second);
        float c = stof(pop(st).second);
        float b = stof(pop(st).second);
        float a = stof(pop(st).second);
        return matrix_t{a, b, c, d, e, f};
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
}

matrix_t Coordinates::get_CTM() const
{
    return CTM;
}

void Coordinates::Td(float x_a, float y_a)
{
    Tm = matrix_t{Tm[0], Tm[1], Tm[2], Tm[3], x_a * Tm[0] + y_a * Tm[2] + Tm[4], x_a * Tm[1] + y_a * Tm[3] + Tm[5]};
    x = 0;
    y = 0;
}

void Coordinates::set_default()
{
    Tm = IDENTITY_MATRIX;
    x = 0;
    y = 0;
}

pair<float, float> apply_matrix_pt(const matrix_t &m, float x, float y)
{
    return make_pair(m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]);
}

text_chunk_t Coordinates::adjust_coordinates(string &&s, size_t len, float width, float Tj, const Fonts &fonts)
{
    if (Tj != 0) x -= Tj * Tfs * Th * 0.001;
    float ty = fonts.get_descent() * Tfs + fonts.get_rise() * Tfs;
    float adv = width * Tfs * Th;
    const matrix_t m = Tm * CTM;
    float prev_f = m[5];
    const matrix_t T_start = translate_matrix(m, x, y);
    float f = T_start[5];
    if (len > 1) x += Tc * Th * (len - 1);
    for (char c : s)
    {
        if (c == ' ') x += Tw * Th;
    }
    const matrix_t T_end = translate_matrix(m, x, y);
    x += adv;
    if (prev_f != f) return text_chunk_t(); //do not render vertical fonts
    const pair<float, float> start_coordinates = apply_matrix_pt(T_start, 0, ty);
    const pair<float, float> end_coordinates = apply_matrix_pt(T_end, adv, ty + Tfs);
    float x0 = min(start_coordinates.first, end_coordinates.first);
    float x1 = max(start_coordinates.first, end_coordinates.first);
    float y0 = min(start_coordinates.second, end_coordinates.second);
    float y1 = max(start_coordinates.second, end_coordinates.second);
//    cout << s << " (" << x0 << ", " << y0 << ")(" << x1 << ", " << y1 << ") " << width << endl;
    return text_chunk_t(std::move(s), coordinates_t(x0, y0, x1, y1));
}

void Coordinates::do_cm(vector<pair<pdf_object_t, string>> &st)
{
    CTM = get_matrix(st) * CTM;
}

void Coordinates::do_q(vector<pair<pdf_object_t, string>> &st)
{
    CTMs.push(CTM);
}

void Coordinates::do_Q(vector<pair<pdf_object_t, string>> &st)
{
    if (!CTMs.empty()) CTM = pop(CTMs);
}

void Coordinates::set_Tz(vector<pair<pdf_object_t, string>> &st)
{
    //Th in percentages
    Th = stof(pop(st).second) / 100;
}

void Coordinates::set_TL(vector<pair<pdf_object_t, string>> &st)
{
    TL = stof(pop(st).second);
}

void Coordinates::set_Tc(vector<pair<pdf_object_t, string>> &st)
{
    Tc = stof(pop(st).second);
}

void Coordinates::set_Tw(vector<pair<pdf_object_t, string>> &st)
{
    Tw = stof(pop(st).second);
}

void Coordinates::set_Td(vector<pair<pdf_object_t, string>> &st)
{
        float y = stof(pop(st).second);
        float x = stof(pop(st).second);
        Td(x, y);
}

void Coordinates::set_TD(vector<pair<pdf_object_t, string>> &st)
{
    float y = stof(pop(st).second);
    float x = stof(pop(st).second);
    Td(x, y);
    TL = -y;
}

void Coordinates::set_Tm(vector<pair<pdf_object_t, string>> &st)
{
    Tm = get_matrix(st);
    x = 0;
    y = 0;
}

void Coordinates::set_T_star(vector<pair<pdf_object_t, string>> &st)
{
    Td(0, -TL);
}

void Coordinates::set_Tf(vector<pair<pdf_object_t, string>> &st)
{
    Tfs = stof(pop(st).second);
}

void Coordinates::set_quote(vector<pair<pdf_object_t, string>> &st)
{
    set_T_star(st);
}

void Coordinates::set_double_quote(vector<pair<pdf_object_t, string>> &st)
{
    Tc = stof(pop(st).second);
    Tw = stof(pop(st).second);
    set_quote(st);
}
