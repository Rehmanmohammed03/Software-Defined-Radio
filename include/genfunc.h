/*
Comp Eng 3DY4 (Computer Systems Integration Project)

Department of Electrical and Computer Engineering
McMaster University
Ontario, Canada
*/

#ifndef DY4_GENFUNC_H
#define DY4_GENFUNC_H

// Add headers as needed
#include <iostream>
#include <vector>
#include <complex>

// Declaration of function prototypes
void generateSin(std::vector<real> &, std::vector<real> &, real, \
	real, real, real, real);

void addSin(const std::vector<std::vector<real>> &, std::vector<real> &);

void generateRandomSamples(std::vector<real> &, unsigned int, \
	unsigned short int, unsigned char);



#endif // DY4_GENFUNC_H
