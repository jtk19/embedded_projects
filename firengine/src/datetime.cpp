
#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "datetime.h"
#include <time.h>



unsigned DateTime::createDateValue(unsigned day, unsigned month, unsigned year)
{
	unsigned val = ((year * 16) + month) * 32 + day;
	return val;
}


string DateTime::convertDateValueToString(unsigned dateValue)
{
	unsigned mday = dateValue & 31;
	unsigned month = (dateValue / 32) & 15;
	unsigned year = dateValue / (16 * 32);

	string dateStr;

	switch (month)
	{
	case 0:		dateStr += ("Start "); break;
	case 1:		dateStr += ("Jan "); break;
	case 2:		dateStr += ("Feb "); break;
	case 3:		dateStr += ("Mar "); break;
	case 4:		dateStr += ("Apr "); break;
	case 5:		dateStr += ("May "); break;
	case 6:		dateStr += ("Jun "); break;
	case 7:		dateStr += ("Jul "); break;
	case 8:		dateStr += ("Aug "); break;
	case 9:		dateStr += ("Sep "); break;
	case 10:	dateStr += ("Oct "); break;
	case 11:	dateStr += ("Nov "); break;
	case 12:	dateStr += ("Dec "); break;
	default:	dateStr += ("End "); break;
	}

	dateStr += ('0' + (mday / 10));
	dateStr += ('0' + (mday % 10));
	dateStr += (' ');

	dateStr += ('0' + (year / 1000)); year %= 1000;
	dateStr += ('0' + (year / 100)); year %= 100;
	dateStr += ('0' + (year / 10)); year %= 10;
	dateStr += ('0' + year);

	return dateStr;
}

unsigned DateTime::getDateValue()
{
	time_t tp;
	time(&tp);
	struct tm* pTimeinfo;
	pTimeinfo = localtime(&tp);
	if (!pTimeinfo)
		return 0;
	return createDateValue(pTimeinfo->tm_mday, pTimeinfo->tm_mon, pTimeinfo->tm_year + 1900);
}



unsigned DateTime::createTimeValue(unsigned hour, unsigned min, unsigned sec)
{
	return ((hour * 64) + min) * 64 + sec;
}

unsigned DateTime::getTimeValue()
{
	unsigned hour = 0;
	unsigned min = 0;
	unsigned sec = 0;

	time_t tp;
	tp = time(NULL);
	struct tm *ptmLocalTime = localtime(&tp);
	if (ptmLocalTime)
	{
		hour = ptmLocalTime->tm_hour;
		min = ptmLocalTime->tm_min;
		sec = ptmLocalTime->tm_sec;
	}

	return createTimeValue(hour, min, sec);
}

string DateTime::convertTimeValueToString(unsigned timeValue)
{
	string timeStr;

	unsigned sec = timeValue % 64;
	unsigned min = (timeValue / 64) % 64;
	unsigned hour = timeValue / (64 * 64);

	timeStr += ('0' + (hour / 10));
	timeStr += ('0' + (hour % 10));
	timeStr += (':');
	timeStr += ('0' + (min / 10));
	timeStr += ('0' + (min % 10));
	timeStr += (':');
	timeStr += ('0' + (sec / 10));
	timeStr += ('0' + (sec % 10));

	return string(timeStr);
}
