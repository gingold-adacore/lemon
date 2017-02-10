#include <iomanip>
#include <sstream>

#include "outputs.h"

using namespace std;

char xdigits[] = "0123456789abcdef";

ostream &
hex2 (std::ostream &stream)
{
  return stream << std::setw (2) << std::hex << std::setfill ('0');
}

ostream &
hex8 (std::ostream &stream)
{
  return stream << std::setw (8) << std::hex << std::setfill ('0');
}

template<int w>
string
hex (unsigned int v)
{
  std::stringstream s;

  s << std::setw (w) << std::hex << std::setfill ('0') << v;

  return s.str ();
}

string
hex1 (unsigned int v)
{
  return hex<1> (v);
}

string
hex2 (unsigned int v)
{
  return hex<2> (v);
}

string
hex4 (unsigned int v)
{
  return hex<4> (v);
}

string
hex8 (unsigned int v)
{
  return hex<8> (v);
}

string
dec (unsigned int v)
{
  std::stringstream s;

  s << v;

  return s.str ();
}

string
fdesc::str (word v)
{
  if (len <= 4)
    return hex<1> (v);
  else if (len <= 8)
    return hex<2> (v);
  else if (len <= 12)
    return hex<3> (v);
  else if (len <= 16)
    return hex<4> (v);
  else
    return hex<8> (v);
}

void
reg_desc::disp (std::ostream &stream, std::string pfx, word value)
{
  int width = 0;
  
  for (auto f: fields)
    {
      string name = f->get_name ();
      string val = f->str (f->extract (value));
      int field_width = name.size () + 1 + val.size ();

      if (width + field_width + 1 >= 80)
	{
	  stream << endl;
	  stream << pfx;
	  width = pfx.size ();
	}
      else if (width != 0)
	{
	  stream << " ";
	  width += 1;
	}
      else
	{
	  stream << pfx;
	  width = pfx.size ();
	}
      stream << f->get_name () << ":" << f->str (f->extract (value));
      width += field_width;
    }
  stream << endl;
}

word
reg_desc::extract (const char *reg, word value)
{
  for (auto f: fields)
    if (strcmp (f->get_name (), reg) == 0)
      return f->extract (value);
  throw "field does not exist";
}
