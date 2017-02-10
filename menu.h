#ifndef MENU_H_
#define MENU_H_

#include <string>
#include <list>
#include <vector>

#include "lemon.h"

class menu_item;
class menu_item_arg;
class cmd_arg;

class cmd_arg
{
public:
  //  Create a new argument for a command.
  cmd_arg (const char *name, bool opt, const char *help) :
    present(false), is_opt(opt), name(name), help(help) {}

  //  Get command name.
  const char *get_name(void) { return name; }

  //  Return true if argument is optional.
  bool get_is_opt (void) { return is_opt; }

  //  Parse P and set PRESENT and VALUE.
  //  Return false in case of error.
  virtual bool parse(std::string &p) = 0;

  //  Disp help for the argument.
  virtual void disp_help(void);

  //  True if the argument is present.
  bool present;

protected:
  bool is_opt;
private:
  const char *name;
  const char *help;
};

//  IP address argument.
class cmd_arg_ipaddr : public cmd_arg
{
public:
  cmd_arg_ipaddr (const char *name, bool opt, const char *help) :
    cmd_arg (name, opt, help) {}
  virtual bool parse (std::string &p);

  struct ip_addr
  {
    unsigned char a[4];
  } addr;
};

//  Expression argument.
class cmd_arg_expr : public cmd_arg
{
public:
  cmd_arg_expr (const char *name, bool opt, const char *help) :
    cmd_arg (name, opt, help) {}
  virtual bool parse (std::string &p);

  word value;
};

//  A boolean (on/off/1/0).
class cmd_arg_bool : public cmd_arg
{
public:
  cmd_arg_bool (const char *name, bool opt, const char *help) :
    cmd_arg (name, opt, help) {}
  virtual bool parse (std::string &p);

  bool value;
};

//  A filename
class cmd_arg_file : public cmd_arg
{
public:
  cmd_arg_file (const char *name, bool opt, const char *help) :
    cmd_arg (name, opt, help) {}
  virtual bool parse (std::string &p);

  std::string filename;
};

class menu_item
{
public:
  const char *get_cmd (void) { return cmd; }
  virtual void execute (std::string line) = 0;
  virtual const char *get_help (void) = 0;
private:
  const char *cmd;
public:
  menu_item (const char *cmd) : cmd (cmd) {}
};

class menu_item_arg : public menu_item
{
private:
  const char *help;
  std::vector<cmd_arg *>args;
  std::function<void(menu_item_arg &)> func;
public:
  menu_item_arg (const char *name,
		 const char *help,
		 std::vector<cmd_arg *>args,
		 std::function<void(menu_item_arg &)> func) :
    menu_item (name), help (help), args (args), func (func) {}
  void execute (std::string line);
  const char *get_help (void) { return help; }
  cmd_arg *get_arg (unsigned int n) { return args[n]; };
};

class menu_item_submenu : public menu_item
{
private:
  const char *help;
  std::list<menu_item *>cmds;
  std::function<void(void)> func;
public:
  menu_item_submenu (const char *name,
		     const char *help,
		     std::list<menu_item *>cmds,
		     std::function<void(void)> func) :
    menu_item (name), help (help), cmds (cmds), func (func) {}
  void execute (std::string line);
  const char *get_help (void) { return help; }
  void execute_help (void);
  void add (class menu_item *ent) { cmds.push_back (ent); }
};

void menu_help (menu_item_submenu *menu);

// Called by a command so that if the user press enter, the command will
// execute again.
extern void menu_repeat (void);
extern bool menu_repeat_p (void);

#endif /* MENU_H_ */
