#include "strExt.h"

bool startsWith(string source, string token)
{
	bool b = true;
	for(int i = 0; b && i < source.length(); i++)
	{
		b &= (source[i] == token[i]);
	}
	return b;
}

bool contains(string source, string token)
{
	return string::npos != (source.find(token));
}