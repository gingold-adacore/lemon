#include <list>
#include <string>
#include <iostream>

#include "menu.h"
#include "parse.h"

using namespace std;

bool
operator== (menu_item *mi, string s)
{
  return mi->get_cmd () == s;
}

void
menu_item_submenu::execute_help (void)

{
  for (auto e: this->cmds)
    cout << e->get_cmd () << " - " << e->get_help () << endl;
}

static bool do_repeat = false;
void
menu_repeat (void)
{
  do_repeat = true;
}

bool
menu_repeat_p (void)
{
  return do_repeat;
}

static string
extract_str (string &p)
{
  size_t sp_pos = p.find_first_of (' ');
  if (sp_pos == string::npos)
    {
      string res = p;
      p.clear();
      return res;
    }
  else
    {
      string res = p.substr (0, sp_pos);
      p.erase (0, sp_pos + 1);
      return res;
    }
}

bool
cmd_arg_file::parse (string &p)
{
  p = skip_blanks (p);

  if (p.empty ())
    return false;

  filename = extract_str (p);
  return true;
}

bool
cmd_arg_ipaddr::parse (string &p)
{
  p = skip_blanks (p);

  if (p.empty ())
    return false;

  string str = extract_str (p);
  unsigned int idx = 0;

  addr.a[idx] = 0;
  for (auto c: str)
    {
      if (c >= '0' && c <= '9')
	addr.a[idx] = addr.a[idx] * 10 + c - '0';
      else if (c == '.')
	{
	  if (idx == 3)
	    return false;
	  idx++;
	  addr.a[idx] = 0;
	}
      else
	return false;
    }
  return true;
}

bool
cmd_arg_bool::parse (string &p)
{
  p = skip_blanks (p);

  if (p.empty ())
    return false;

  string s = extract_str (p);
  if (s == "0" || s == "off")
    value = false;
  else if (s == "1" || s == "on")
    value = true;
  else
    return false;
  return true;
}

bool
cmd_arg_expr::parse (string &p)
{
  p = skip_blanks (p);

  if (p.empty ())
    return false;
  value = parse_expr (p);
  return true;
}

void
cmd_arg::disp_help(void)
{
  if (is_opt)
    cout << "[" << name << "]";
  else
    cout << name;
}

void
menu_item_arg::execute (string line)
{
  string l = line;

  for (auto a: args)
    {
      l = skip_blanks (l);

      if (l.empty ())
	{
	  a->present = false;
	  if (!a->get_is_opt ())
	    throw parse_error
	      ("missing " + string (a->get_name ()) + " argument");
	}
      else
	{
	  a->present = true;
	  if (!a->parse (l))
	    throw parse_error
	      ("syntax error for argument " + string (a->get_name ()));
	}
    }
  func (*this);
}

void
menu_item_submenu::execute (string line)
{
  string l = skip_blanks (line);

  if (l.empty ())
    func ();
  else
    {
      string s = extract_str (l);

      for (auto m: cmds)
	{
	  //cout << "'" << s << "' vs '" << m->get_cmd() << "'" << endl;
	  if (s.compare (m->get_cmd()) == 0)
	    {
	      //cout << "got it, continue with " << l << endl;
	      m->execute (l);
	      return;
	    }
	}
      throw parse_error
	("no submenu " + s + " in " + string (get_cmd ()));
    }
}
