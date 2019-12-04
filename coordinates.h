#ifndef COORDINATES_H
#define COORDINATES_H

#include <vector>
#include <string>
#include <stack>
#include <utility>

#include "common.h"
#include "pages_extractor.h"

using matrix_t = std::vector<std::vector<double>>;
class Coordinates
{
public:
    Coordinates(unsigned int rotate, const cropbox_t &cropbox);
    void set_CTM(std::stack<std::pair<pdf_object_t, std::string>> &st);
    void set_default();
    std::pair<unsigned int, unsigned int> get_coordinates() const;
    void adjust_coordinates(unsigned int width, size_t len, double Tj);
    void adjust_coordinates(const std::string &token, std::stack<std::pair<pdf_object_t, std::string>> &st);
private:
    enum
    {
        TH_DEFAULT = 100,
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
    double tx;
    double ty;
};

#endif //COORDINATES_H
