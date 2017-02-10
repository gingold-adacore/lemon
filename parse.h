#ifndef PARSE_H_
#define PARSE_H_

#include <string>

#include "lemon.h"

class parse_error
{
 public:
  parse_error (std::string msg) : msg (msg) {}
  std::string get_msg (void) { return msg; }
 private:
  std::string msg;
};

// Update P to point to the first non-blank character.
extern void skip_blanks (char *&p);
extern std::string skip_blanks (std::string s);

// True if there is nothing after p (skip blanks).
extern bool end_of_arg (char *&p);

// Throw a parse_error if not at end of arg.
extern void check_no_arg (char *&p);

// Return the value of the expression at P
// Throw parse_error in case of error.
extern word parse_expr (std::string &p);

// Extract a string from P.  A string is any letter but a blank or NUL.
// Throw parse_error if no string.
extern std::string parse_str (std::string s);

// Return true iff the next word of P is STR.  If true, the word is extracted.
extern bool parse_eq (char *&p, const char *str);
#endif /* PARSE_H_ */
