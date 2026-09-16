/*
Comp Eng 3DY4 (Computer Systems Integration Project)

Department of Electrical and Computer Engineering
McMaster University
Ontario, Canada
*/

#include "dy4.h"
#include "iofunc.h"

// Some basic functions for printing information from vectors
// or to read from/write to binary files in real format

void printRealVector(const std::vector<real> &x) {
	std::cout << "Printing real vector of size " << x.size() << "\n";
	for (int i = 0; i < (int)x.size(); i++) {
		std::cout << x[i] << " ";
	}
	std::cout << "\n";
}

void printComplexVector(const std::vector<std::complex<real>> &X) {
	std::cout << "Printing complex vector of size " << X.size() << "\n";
	for (int i = 0; i < (int)X.size(); i++) {
		std::cout << X[i] << " ";
	}
	std::cout << "\n";
}

// Assumes data in the raw binary file is in the format corresponding to 'real'
void readBinData(const std::string in_fname, std::vector<real> &bin_data)
{
	std::ifstream fdin(in_fname, std::ios::binary);
	if (!fdin) {
		std::cout << "File " << in_fname << " not found ... exiting\n";
		exit(1);
	} else {
		std::cout << "Reading raw binary from \"" << in_fname << "\"\n";
	}

	fdin.seekg(0, std::ios::end);
	const unsigned int num_samples = fdin.tellg() / sizeof(real);

	bin_data.resize(num_samples);
	fdin.seekg(0, std::ios::beg);
	fdin.read(reinterpret_cast<char *>(&bin_data[0]), num_samples * sizeof(real));
	fdin.close();
}

// Assumes data in the raw binary file is in the format corresponding to 'real'
void writeBinData(const std::string out_fname, const std::vector<real> &bin_data)
{
	std::cout << "Writing raw binary to \"" << out_fname << "\"\n";
	std::ofstream fdout(out_fname, std::ios::binary);
	for (int i = 0; i < (int)bin_data.size(); i++) {
		fdout.write(reinterpret_cast<const char *>(&bin_data[i]), sizeof(bin_data[i]));
	}
	fdout.close();
}

//////////////////////////////////////////////////////////////
// New code as part of benchmarking/testing and the project
//////////////////////////////////////////////////////////////

void generate_random_values(std::vector<real>& x, const real& lower_bound = -1., const real& upper_bound = 1.) {

	for (int i = 0; i < (int)x.size(); i++) {
		x[i] = lower_bound + (upper_bound - lower_bound) * ((real)(rand()) / RAND_MAX);
	}
}

void readStdinBlockData(unsigned int num_samples, std::vector<real> &block_data, std::vector<char> &raw_data)
{
	std::cin.read(reinterpret_cast<char*>(&raw_data[0]), num_samples * sizeof(char));

	for (int k = 0; k < (int)num_samples; k++)
	{
		block_data[k] = real(((unsigned char)raw_data[k]-128.0)/128);
	}
}

void UnInterleave_IQ(std::vector<real> &in_data, std::vector<real> &out_I, std::vector<real> &out_Q)
{
	for (int i = 0; i < (int)in_data.size()/2; i++)
	{
			out_I[i] = in_data[2*i];
			out_Q[i] = in_data[(2*i) + 1];
	}
}



