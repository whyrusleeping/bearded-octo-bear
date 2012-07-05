#include "strExt.h"

bool startsWith(string source, string token)
{
	if(source.substr(0, token.length()) == token)
		return true;
	else
		return false;
}

bool contains(string source, string token)
{
	return string::npos != (source.find(token));
}