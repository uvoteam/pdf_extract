#include <stack>
#include <utility>
#include <string>

#include "coordinates.h"
#include "common.h"


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

    matrix_t operator*(const matrix_t &m1, const matrix_t &m2)
    {
        matrix_t result(m1.size(), vector<double>(m2.at(0).size(), 0));
        for (size_t i = 0; i < m1.size(); i++)
        {
            for (size_t j = 0; j < m2.at(0).size(); j++)
            {
                result[i][j] = 0;
                for (size_t k = 0; k < m1.at(0).size(); k++) result[i][j] += m1[i][k] * m2[k][j];
            }
        }
        return result;
    }

    matrix_t get_matrix(stack<pair<pdf_object_t, string>> &st)
    {
        double e = stod(pop(st).second);
        double f = stod(pop(st).second);
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
    TL(TL_DEFAULT)
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

void Coordinates::Td(double x, double y)
{
    coordinates.start_x = x;
    coordinates.start_y = coordinates.end_y =  y;
    Tm = matrix_t{{1, 0, 0},
                  {0, 1, 0},
                  {coordinates.start_x, coordinates.start_y, 1}} * Tm;
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
    CTM = get_matrix(st) * CTM;
}

pair<double, double> Coordinates::get_coordinates(double x, double y) const
{
    matrix_t result =  matrix_t{{x, y, 1}} * matrix_t{{Tfs * Th, 0, 0}, {0, Tfs, 0}, {0, TRISE_DEFAULT, 0}} * Tm * CTM;
    return make_pair(result[0][0], result[0][1]);
}

pair<double, double> Coordinates::get_start_coordinates() const
{
    return get_coordinates(coordinates.start_x, coordinates.start_y);
}

coordinates_t Coordinates::adjust_coordinates(unsigned int width, size_t len, double Tj)
{
    const pair<double, double> start_coordinates = get_coordinates(coordinates.start_x, coordinates.start_y);
    coordinates.end_x += (((width - Tj)/1000) * Tfs + Tc + Tw) * Th * len;
    Tm = matrix_t{{1, 0, 0}, {0, 1, 0}, {coordinates.end_x, coordinates.end_y, 1}} * Tm;
    const pair<double, double> end_coordinates = get_coordinates(coordinates.end_x, coordinates.end_y);
    coordinates.start_x = coordinates.end_x;
    coordinates.start_y = coordinates.end_y;
    return coordinates_t(start_coordinates.first, start_coordinates.second, end_coordinates.first, end_coordinates.second);
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
