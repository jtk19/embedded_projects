#ifndef STRINGMATCHSTREAM_H
#define STRINGMATCHSTREAM_H

#include <string>
using namespace std;


///////////////////////////////////////////////////////////////////
/// StringMatch is used for pattern matching in strings
///  Acts like a 'std::ostream' - Must be loaded with a string to begin with
/// All these Matching functions behave as follows:
///   return true if a complete match is made
///   advance the Iterator only if a complete match is made
///////////////////////////////////////////////////////////////////
class StringMatchStream
{
public:
	StringMatchStream(const string&);
	StringMatchStream(string::const_iterator& begin, string::const_iterator& end);
public:
	string getString() const					{ return string(m_Begin, m_End); }
	string::const_iterator begin() const		{ return m_Begin; }
	string::const_iterator end() const			{ return m_End; }
public:
	/// Detect when string-stream is exhausted
	bool atEnd() const					{ return m_Begin == m_End; }
	/// Get next character in stream
	char operator *() const				{ return *m_Begin; }
	/// Advance to next char
	void operator ++ ()					{ ++m_Begin; }
public:
	// matches a complete string of characters
	bool matchText(const char* pText);

	// matches a single character
	bool matchChar(char charRequired);

	// matches a single character, c, such that  chLower <= c <= chUpper
	bool matchCharInRange(char chLower, char chUpper);

	// matches a single character in the set pCharSet
	bool matchCharInSet(const char* pCharSet);
public:
	bool matchWhitespace();
	bool matchWhitespace(string* pOut);
	bool matchNonWhitespace(string* pOut);
	bool matchText(const string&);
public:
	// Note: if Optional argument is non-zero store matching string
	bool matchIdent(string* pOut);
	bool matchFloatingPointNumber(bool* pIsFloat, double* pOut);
	bool matchHexDigit(unsigned* pOut);
	bool matchHexDigits(unsigned* pOut, unsigned numDigits);		///< must match specified number of digits
	bool matchInt(int* pOut);				///< Decimal only
	bool matchUInt(unsigned* pOut);			///< Decimal only
private:
	string::const_iterator		m_Begin;
	string::const_iterator		m_End;
};


#endif
