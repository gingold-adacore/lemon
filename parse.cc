#include "parse.h"
#include <cassert>

using namespace std;

void
skip_blanks (char *&p)
{
  while (*p == ' ')
    p++;
}

string
skip_blanks (string s)
{
  for (int i = 0; i < s.length(); i++)
    if (!::isspace (s[i]))
      return s.substr(i);
  return "";
}

bool
end_of_arg (char *&p)
{
  skip_blanks (p);
  return *p == '\0';
}

void
check_no_arg (char *&p)
{
  if (!end_of_arg (p))
    throw parse_error ("garbage after last argument: " + string (p));
}

word
parse_number (string &p)
{
  word base;

  assert (!p.empty ());

  if (p[0] == '0'
      && (p[1] == 'x' || p[1] == 'X'))
    {
      p.erase (0, 2);
      base = 16;
    }
  else
    base = 10;

  bool has_digit;
  word res = 0;

  while (1)
    {
      char c = p[0];
      word d;
      if (c >= '0' && c <= '9')
	d = c - '0';
      else if (c >= 'a' && c <= 'f')
	d = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
	d = c - 'A' + 10;
      else
	break;
      if (d >= base)
	throw
	  parse_error ("digit '" + string (1, c) + "' greather than base");
      res = res * base + d;
      has_digit = true;
      p.erase (0, 1);
    }
  if (!has_digit)
    throw parse_error ("missing digit");
  return res;
}

word
parse_expr (string &p)
{
  while (1)
    {
      if (p.empty())
	throw parse_error ("expression expected");

      switch (p[0])
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  return parse_number (p);
	case ' ':
	  p.erase(0, 1);
	  break;
	default:
	  throw parse_error ("unhandled character '" + p.substr (0, 1) + "'");
	}
    }
}

char *
parse_str (char *&p)
{
  skip_blanks (p);

  if (end_of_arg (p))
    throw parse_error ("string expected");

  char *res = p;

  while (*p && *p != ' ')
    p++;
  if (*p == ' ')
    *p++ = 0;

  return res;
}

bool
parse_eq (char *&p, const char *str)
{
  int len = strlen (str);
  
  skip_blanks (p);

  if (memcmp (p, str, len) == 0
      && (p[len] == ' ' || p[len] == 0))
    {
      p += len;
      return true;
    }
  else
    return false;
}

string
parse_str (string s)
{
  string r = skip_blanks (s);
  if (r.empty())
    throw parse_error ("string expected");
  return r.substr (0, r.find_first_of (' '));
}

