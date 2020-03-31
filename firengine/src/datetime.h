#ifndef DATETIME_H
#define DATETIME_H


#include <string>
using namespace std;


class DateTime
{
public:
	static unsigned createDateValue(unsigned day, unsigned month, unsigned year);
	static string convertDateValueToString(unsigned dateValue);
	static unsigned getDateValue();
public:
	static unsigned createTimeValue(unsigned hour, unsigned min, unsigned sec);
	static unsigned getTimeValue();
	static string convertTimeValueToString(unsigned timeValue);
};


#endif
