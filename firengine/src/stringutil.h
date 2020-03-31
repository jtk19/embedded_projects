#ifndef SAFEGETLINE_H
#define SAFEGETLINE_H


#include <istream>
#include <string>
#include <sstream>
using namespace std;


template <typename T>
string toString(T value)
{
	ostringstream os;
	os << value;
	return os.str();
}


/// This performs like getline() but deals with \r and \n to allow portability between windows and linux
istream& safeGetline(istream& is, string& t);


string toHexDigits(unsigned val, unsigned numDigits);



#endif

