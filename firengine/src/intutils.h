#ifndef INTUTILS_H
#define INTUTILS_H


namespace IntUtils
{

inline unsigned findMostSignificantOne(unsigned bits)
{
	unsigned pos = 0;
	if ((bits & 0xFFFF0000) != 0)
	{
		bits >>= 16;
		pos += 16;
	}
	if ((bits & 0xFF00) != 0)
	{
		bits >>= 8;
		pos += 8;
	}
	if ((bits & 0xF0) != 0)
	{
		bits >>= 4;
		pos += 4;
	}
	if ((bits & 0xC) != 0)
	{
		bits >>= 2;
		pos += 2;
	}
	if ((bits & 0x2) != 0)
	{
		bits >>= 1;
		pos += 1;
	}
	return pos;
}

// Find the number of bits needed to represent a value
inline unsigned bitWidthToRepresentUnsignedValue(unsigned x)
{
	if (x == 0)
		return 0;
	else
		return findMostSignificantOne(x) + 1;
}

inline unsigned bitWidthToRepresentSignedValue(int x)
{
	if (x == 0)
		return 0;
	else if (x < 0)
		return findMostSignificantOne(~x) + 2;			// Note: includes 1 sign bit
	else
		return findMostSignificantOne(x) + 1;
}

// Find the number of bits needed to encode x distinct values
inline unsigned bitWidthForEncodingValues(unsigned x)
{
	if (x > 1)
		return findMostSignificantOne(x-1) + 1;
	else
		return 0;		// don't need any bits to represent zero or 1 value
}

inline bool isPowerOfTwo(unsigned x)
{
	for (int i = 0; i < 32; ++i)
	{
		if (x == (1u << i))
			return true;
	}
	return false;
}

inline unsigned roundUpToPowerOfTwo(unsigned x)
{
	for (unsigned i = 0; i < 32; ++i)
	{
		if (x <= (1u << i))
			return (1u << i);
	}
	return x;
}

inline unsigned roundDownToPowerOfTwo(unsigned x)
{
	for (unsigned i = 1; i < 32; ++i)
	{
		if (x < (1u << i))
			return (1u << (i - 1));
	}
	return (1u << 31);
}

inline bool isMultipleOf(unsigned large, unsigned small)
{
	unsigned scale = large / small;
	unsigned restoredLarge = scale * small;
	return (large == restoredLarge);
}


inline unsigned roundupToMultipleOf(unsigned x, unsigned factor)
{
	unsigned remainder = (x % factor);
	if (remainder > 0)
		x = x - remainder + factor;
	return x;
}

inline unsigned roundupToFactorOf(unsigned x, unsigned largerNum)
{
	assert(x <= largerNum);
	while (!isMultipleOf(largerNum, x))
		++x;
	return x;
}

// division with round up
inline unsigned ceilDiv(unsigned a, unsigned b)
{
	return (a + b-1) / b;
}


inline unsigned modulo(int val, unsigned modulo)
{
	while (val >= int(modulo))
		val -= modulo;
	while (val < 0)
		val += modulo;
	return unsigned(val);
}


inline unsigned alignAddressOnOrAfter(unsigned currentAddress, unsigned alignmentQuanta)
{
	assert(alignmentQuanta > 0);
	assert(IntUtils::isPowerOfTwo(alignmentQuanta));

	unsigned mask = alignmentQuanta - 1;
	currentAddress += mask;		// round up
	currentAddress &= ~mask;

	return currentAddress;
}


};

#endif
