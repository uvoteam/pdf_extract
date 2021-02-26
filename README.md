pdf_extract is the thread-safe library to extract text from pdf files. Vertical text is not rendered. Multicolumn
document is rendered vertically(each column is drawed separated by \n).

Usage:

#include "pdf_extractor.h"

std::string pdf2txt(const std::string &buffer);

Input argument:
buffer - content of pdf file


Output:
extracted utf8 text

In case of error std::exception is thrown



Build:(cmake, libssl 1.0 are required)

1. mkdir build

2. cd ./build

3.cmake ..

4.make

5.make install


pdf_extract is distributed under GNU Public License version 2 or above.
