#ifndef OUTPUTS_H_
#define OUTPUTS_H_

#include <iostream>
#include <string>
#include <list>

#include "lemon.h"

extern char xdigits[];

std::ostream &hex2 (std::ostream &stream);
std::ostream &hex8 (std::ostream &stream);

std::string hex1 (word v);
std::string hex2 (word v);
std::string hex4 (word v);
std::string hex8 (word v);

std::string dec (unsigned int v);

class base_desc
{
 public:
  base_desc (const char *name) : name (name) {}

  //  Extract the bits from a register.
  virtual word extract (word v) = 0;

  //  Print bits
  virtual std::string str (word v) = 0;

  //  Get field name.
  const char *get_name (void) { return name; };
 private:
  const char *name;
};

class fdesc : public base_desc
{
 public:
  fdesc (const char *name, unsigned int pos, unsigned int len = 1) :
    base_desc (name), pos (pos), len (len) {};
  virtual word extract (word v) { return (v >> pos) & ((1 << len) - 1); }
  virtual std::string str (word v);
 private:
  unsigned int pos;
  unsigned int len;
};

class reg_desc
{
 public:
  reg_desc (const char *name, std::list<base_desc *> fields) : name (name),
    fields (fields) {}
  virtual void disp (std::ostream &stream, std::string pfx, word value);
  word extract (const char *reg, word value);
 private:
  const char *name;
  std::list<base_desc *> fields;
};
#endif /* OUTPUTS_H_ */
