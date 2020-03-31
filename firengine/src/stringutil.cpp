
#include "stringutil.h"


istream& safeGetline(istream& is, string& t)
{
    t.clear();

    // The characters in the stream are read one-by-one using a std::streambuf.
    // That is faster than reading them one-by-one using the std::istream.
    // Code that uses streambuf this way must be guarded by a sentry object.
    // The sentry object performs various tasks,
    // such as thread synchronization and updating the stream state.

    istream::sentry se(is, true);
    streambuf* sb = is.rdbuf();

    for(;;) {
        int c = sb->sbumpc();
        switch (c) {
        case '\n':
            return is;
        case '\r':
            if(sb->sgetc() == '\n')
                sb->sbumpc();
            return is;
        case -1:		// EOF
            // Also handle the case when the last line has no line ending
            if(t.empty())
                is.setstate(ios::eofbit);
            return is;
        default:
            t += (char)c;
        }
    }
}

string toHexDigits(unsigned val)
{
	char str[16];

	if (val == 0)
		return string("0");
	else
	{
		str[15] = '\0';			// trailing zero
		char* pStr = &str[15];
		while (val != 0)
		{
			--pStr;

			unsigned hexDigit = (val & 0xf);
			if (hexDigit < 10)
				(*pStr) = '0' + hexDigit;
			else
				(*pStr) = 'a' + (hexDigit - 10);

			val >>= 4;		// next digit
		}
		return string(pStr);
	}
}

string toHexDigits(unsigned val, unsigned numDigits)
{
	string str = toHexDigits(val);
	while (str.size() < numDigits)
		str = string("0") + str;
	return str;
}

