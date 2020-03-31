#include <sstream>
#include <cstdlib>
#include <ctype.h>
#include "stringmatchstream.h"


StringMatchStream::StringMatchStream(const string& str) :
	m_Begin			(str.begin()),
	m_End			(str.end())
{
}

StringMatchStream::StringMatchStream(string::const_iterator& begin, string::const_iterator& end) :
	m_Begin			(begin),
	m_End			(end)
{
}

//////////////////////////////////

// Macro to try MatchExpr, and return stream to original state if Match fails
#define TRYTOMATCH(MATCHEXPR)		{ StringMatchStream stream = (*this); if (MATCHEXPR) return true; (*this) = stream; return false; }

bool StringMatchStream::matchChar(char charRequired)
{
	if (!atEnd() && (*(*this) == charRequired))
	{
		++(*this);
		return true;
	}
	else
	{
		return false;
	}
}

bool StringMatchStream::matchCharInRange(char chLower, char chUpper)
{
	if (!atEnd() && (*(*this) >= chLower) && (*(*this) <= chUpper))
	{
		++(*this);
		return true;
	}
	else
	{
		return false;
	}
}

bool StringMatchStream::matchCharInSet(const char* pCharSet)
{
	if (!atEnd())
	{
		char ch = *(*this);
		while ((*pCharSet != '\0') && (*pCharSet != ch))
		{
			++pCharSet;
		}
		if (*pCharSet != '\0')		// match was found
		{
			++(*this);
			return true;
		}
	}
	return false;
}

bool StringMatchStream::matchWhitespace()
{
	bool bWhitespaceFound = false;
	while (!atEnd() && isspace(*m_Begin))
	{
		++m_Begin;
		bWhitespaceFound = true;
	}
	return bWhitespaceFound;
}

bool StringMatchStream::matchWhitespace(string* pOut)
{
	bool bWhitespaceFound = false;
	while (!atEnd() && isspace(*m_Begin))
	{
		(*pOut) += (*m_Begin);
		++m_Begin;
		bWhitespaceFound = true;
	}
	return bWhitespaceFound;
}

bool StringMatchStream::matchNonWhitespace(string* pStr)
{
	while (!atEnd() && !isspace(*m_Begin))
	{
		(*pStr) += *m_Begin;
		++m_Begin;
	}

	return !pStr->empty();
}

static bool _matchText(StringMatchStream& stream, const char* pText)
{
	while ((pText != '\0') && stream.matchChar(*pText))
		++pText;
	return (*pText == '\0');
}

bool StringMatchStream::matchText(const char* pText)	{ TRYTOMATCH(_matchText(*this, pText)); }

static bool _matchComment(StringMatchStream& stream)
{
	if (stream.matchText("/*"))
	{
		bool bMatch = false;
		while (!stream.atEnd() && !(bMatch = stream.matchText("*/")))
			++stream;
		if (bMatch)
			return true;
	}
	return false;
}

// Ident = {LETTER}({LETTER}|{DIGIT})*
bool StringMatchStream::matchIdent(string* pString)
{
	string::const_iterator si = begin();

	if (!atEnd() &&
		(matchCharInRange('a', 'z') ||
		matchCharInRange('A', 'Z') ||
		matchChar('_')))
	{
		while (!atEnd() &&
			(matchCharInRange('0', '9') ||
			matchCharInRange('a', 'z') ||
			matchCharInRange('A', 'Z') ||
			matchChar('_')))
			;
		if (pString) (*pString) = string(si, begin());
		return true;
	}
	return false;
}


// "."{DIGIT}+                           {(f|F|l|L)}?
// "."{DIGIT}+[Ee][+-]?{DIGIT}+          {(f|F|l|L)}?
// {DIGIT}+"."                           {(f|F|l|L)}?
// {DIGIT}+"."[Ee][+-]?{DIGIT}+          {(f|F|l|L)}?
// {DIGIT}+"."{DIGIT}+                   {(f|F|l|L)}?
// {DIGIT}+"."{DIGIT}+[Ee][+-]?{DIGIT}+  {(f|F|l|L)}?
static bool _matchFloatingPointNumber(StringMatchStream& stream, bool* pIsFloat, double *pDouble)
{
	string::const_iterator si = stream.begin();

	stream.matchChar('+');
	bool bNegated = stream.matchChar('-');

	// stage 1: find floating point
	// {DIGIT}*
	while (stream.matchCharInRange('0', '9'))
		;

	// optional "."
	if (!stream.matchChar('.'))
		return false;

	// stage 2: scan for digits after floating point
	// {DIGIT}*
	while (stream.matchCharInRange('0', '9'))
		;

	// stage 3: scan for exponent
	// [Ee][+-]?{DIGIT}+
	if (stream.matchCharInSet("Ee"))
	{
		bool bMatchComplete = false;
		stream.matchCharInSet("+-");
		while (stream.matchCharInRange('0', '9'))
		{
			bMatchComplete = true;
		}
		if (!bMatchComplete)
			return false;
	}

	// {(f|F|l|L)}?
	if (pIsFloat)
		(*pIsFloat) = stream.matchCharInSet("fF");

	if (pDouble)
		(*pDouble) = std::strtod(string(si, stream.begin()).c_str(), 0);

	return true;
}

bool StringMatchStream::matchFloatingPointNumber(bool* pIsFloat, double *pDouble)	{ TRYTOMATCH(_matchFloatingPointNumber(*this, pIsFloat, pDouble)); }


bool StringMatchStream::matchInt(int* pInt)
{
	bool bSuccess = true;
	if (matchChar('-'))
	{
		unsigned uval;
		bSuccess = matchUInt(&uval);
		(*pInt) = -(int)uval;
	}
	else
	{
		unsigned uval;
		bSuccess = matchUInt(&uval);
		(*pInt) = uval;
	}
	return bSuccess;
}


static bool _matchHexDigit(StringMatchStream& stream, unsigned* pUInt)
{
	if (!stream.atEnd())
	{
		char ch = *stream;
		if (stream.matchCharInRange('0', '9'))
			(*pUInt) = unsigned(ch - '0');
		else if (stream.matchCharInRange('a', 'f'))
			(*pUInt) = unsigned(ch - 'a' + 10);
		else if (stream.matchCharInRange('A', 'F'))
			(*pUInt) = unsigned(ch - 'A' + 10);
		else
			return false;
		return true;
	}
	return false;
}


bool StringMatchStream::matchHexDigit(unsigned* pUInt)
{
	return _matchHexDigit(*this, pUInt);
}


static bool _matchHexDigits(StringMatchStream& stream, unsigned* pUInt, unsigned numDigits)
{
	(*pUInt) = 0;
	for (unsigned i = 0; i < numDigits; ++i)
	{
		unsigned hexDigit;
		if (!stream.matchHexDigit(&hexDigit))
			return false;
		(*pUInt) = ((*pUInt) << 4) + hexDigit;
	}
	return true;
}

bool StringMatchStream::matchHexDigits(unsigned* pUInt, unsigned numDigits)		{ TRYTOMATCH(_matchHexDigits(*this, pUInt, numDigits)); }


// {DIGIT}+                              {(u|U|l|L)*}?
static bool _matchUInt(StringMatchStream& stream, unsigned* pUInt)
{
	(*pUInt) = 0;
	unsigned numDigits = 0;
	while (!stream.atEnd())
	{
		char ch = (*stream);
		if (stream.matchCharInRange('0', '9'))
		{
			++numDigits;
			(*pUInt) = ((*pUInt) * 10) + unsigned(ch - '0');
		}
		else
		{
			break;
		}
	}

	if (numDigits == 0)
		return false;		// no digits encountered 
	else
		return true;		// end of number
}

bool StringMatchStream::matchUInt(unsigned* pUInt)	{ TRYTOMATCH(_matchUInt(*this, pUInt)); }

