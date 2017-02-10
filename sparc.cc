#include "sparc.h"
#include "outputs.h"

using namespace std;

static const char * const bicc_map[] =
  {"n", "e",  "le", "l",  "leu", "cs", "neg", "vs",
   "a", "ne", "g",  "ge", "gu",  "cc", "pos", "vc" };

static const char * const fbfcc_map[] =
  {"n", "ne", "lg", "ul", "l",   "ug", "g",   "u",
   "a", "e",  "ue", "ge", "uge", "le", "ule", "o" };

enum format {
  fmt_Rd,		// rdy %rd
  fmt_Rs1_Regimm,	// wry rs1,imm or rs1,rs2
  fmt_Rs1_Regimm_Rd,	// xxx rd,rs1,imm   or   xxx rd,rs1,rs2
  fmt_Fp_Mem,
  fmt_Mem_Fp,
  fmt_Mem_Rd,
  fmt_Rd_Mem,
  fmt_Mem_Creg,
  fmt_Mem,
  fmt_Fregrs1_Fregrs2,
  fmt_Fregrs1_Fregrs2_Fregrd,
  fmt_Fregrs2_Fregrd,
  fmt_Asi,     //  format 3, rd, rs1, asi and rs2.
  fmt_SPECIAL, //  All formats beyond this one are specials.
  fmt_ticc,
  fmt_or,
  fmt_jmpl,
  fmt_rdy,
};


struct insn_desc_type
{
  const char *name;
  unsigned char op;
  format fmt;
};

static const insn_desc_type insn_desc_10[] =
  {
    // 0x00
    { "add",  0x00, fmt_Rs1_Regimm_Rd},
    { "and",  0x01, fmt_Rs1_Regimm_Rd},
    { "or",   0x02, fmt_or },
    { "xor",  0x03, fmt_Rs1_Regimm_Rd},
    { "sub",  0x04, fmt_Rs1_Regimm_Rd},
    { "andn", 0x05, fmt_Rs1_Regimm_Rd},
    { "orn",  0x06, fmt_Rs1_Regimm_Rd},
    { "xnor", 0x07, fmt_Rs1_Regimm_Rd},

    { "addx", 0x08, fmt_Rs1_Regimm_Rd},
    { "umul", 0x0a, fmt_Rs1_Regimm_Rd},
    { "smul", 0x0b, fmt_Rs1_Regimm_Rd},
    { "subx", 0x0c, fmt_Rs1_Regimm_Rd},
    { "udiv", 0x0e, fmt_Rs1_Regimm_Rd},
    { "sdiv", 0x0f, fmt_Rs1_Regimm_Rd},

    // 0x10
    { "addcc",  0x10, fmt_Rs1_Regimm_Rd},
    { "andcc",  0x11, fmt_Rs1_Regimm_Rd},
    { "orcc",   0x12, fmt_Rs1_Regimm_Rd},
    { "xorcc",  0x13, fmt_Rs1_Regimm_Rd},
    { "subcc",  0x14, fmt_Rs1_Regimm_Rd},
    { "andncc", 0x15, fmt_Rs1_Regimm_Rd},
    { "orncc",  0x16, fmt_Rs1_Regimm_Rd},
    { "xnorcc", 0x17, fmt_Rs1_Regimm_Rd},

    { "addxcc", 0x18, fmt_Rs1_Regimm_Rd},
    { "umulcc", 0x1a, fmt_Rs1_Regimm_Rd},
    { "smulcc", 0x1b, fmt_Rs1_Regimm_Rd},
    { "subxcc", 0x1c, fmt_Rs1_Regimm_Rd},
    { "udivcc", 0x1e, fmt_Rs1_Regimm_Rd},
    { "sdivcc", 0x1f, fmt_Rs1_Regimm_Rd},

    // 0x20
    { "taddcc",   0x20, fmt_Rs1_Regimm_Rd},
    { "tsubcc",   0x21, fmt_Rs1_Regimm_Rd},
    { "taddcctv", 0x22, fmt_Rs1_Regimm_Rd},
    { "tsubcctv", 0x23, fmt_Rs1_Regimm_Rd},
    { "mulscc",   0x24, fmt_Rs1_Regimm_Rd},
    { "sll",      0x25, fmt_Rs1_Regimm_Rd},
    { "srl",      0x26, fmt_Rs1_Regimm_Rd},
    { "sra",      0x27, fmt_Rs1_Regimm_Rd},

    { "rdy",      0x28, fmt_rdy },
    { "rdpsr",    0x29, fmt_Rd},
    { "rdwim",    0x2a, fmt_Rd},
    { "rdtbr",    0x2b, fmt_Rd},

    // 0x30
    { "wry",      0x30, fmt_Rs1_Regimm},
    { "wrpsr",    0x31, fmt_Rs1_Regimm},
    { "wrwim",    0x32, fmt_Rs1_Regimm},
    { "wrtbr",    0x33, fmt_Rs1_Regimm},

    { "jmpl",     0x38, fmt_jmpl},
    { "rett",     0x39, fmt_Rs1_Regimm_Rd},
    { "t",        0x3a, fmt_ticc},
    { "iflush",   0x3b, fmt_Mem},
    { "save",     0x3c, fmt_Rs1_Regimm_Rd},
    { "restore",  0x3d, fmt_Rs1_Regimm_Rd},
  };

static const insn_desc_type insn_desc_11[] =
  {

    { "ld",   0x00, fmt_Mem_Rd},
    { "ldub", 0x01, fmt_Mem_Rd},
    { "lduh", 0x02, fmt_Mem_Rd},
    { "ldd",  0x03, fmt_Mem_Rd},
    { "st",   0x04, fmt_Rd_Mem},
    { "stb",  0x05, fmt_Rd_Mem},
    { "sth",  0x06, fmt_Rd_Mem},
    { "std",  0x07, fmt_Rd_Mem},

    { "ldsb", 0x09, fmt_Mem_Rd},
    { "ldsh", 0x0a, fmt_Mem_Rd},

    // 0x10
    { "lda",  0x10, fmt_Asi},
    { "ldda", 0x13, fmt_Asi},

    // 0x20
    { "ldf",   0x20, fmt_Mem_Fp},
    { "ldfsr", 0x21, fmt_Mem},
    { "lddf",  0x23, fmt_Mem_Fp},
    { "stf",   0x24, fmt_Fp_Mem},
    { "stfsr", 0x25, fmt_Mem},
    { "stdfq", 0x26, fmt_Mem},
    { "stdf",  0x27, fmt_Fp_Mem},

    // 0x30
    { "ldc",   0x30, fmt_Mem_Creg},
    { "ldcsr", 0x31, fmt_Mem},
  };

static const insn_desc_type insn_desc_34[] =
  {
    { "fmovs",  0x01, fmt_Fregrs2_Fregrd},
    { "fnegs",  0x05, fmt_Fregrs2_Fregrd},
    { "fabss",  0x09, fmt_Fregrs2_Fregrd},
    { "fsqrts", 0x29, fmt_Fregrs2_Fregrd},
    { "fsqrtd", 0x2a, fmt_Fregrs2_Fregrd},
    { "fsqrtx", 0x2b, fmt_Fregrs2_Fregrd},
    { "fadds",  0x41, fmt_Fregrs2_Fregrd},
    { "faddd",  0x42, fmt_Fregrs2_Fregrd},
    { "faddx",  0x43, fmt_Fregrs2_Fregrd},
    { "fsubs",  0x45, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fsubd",  0x46, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fsubx",  0x47, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fmuls",  0x49, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fmuld",  0x4a, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fmulx",  0x4b, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fdivs",  0x4d, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fdivd",  0x4e, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fdivx",  0x4f, fmt_Fregrs1_Fregrs2_Fregrd},
    { "fitos",  0xc4, fmt_Fregrs2_Fregrd},
    { "fdtos",  0xc6, fmt_Fregrs2_Fregrd},
    { "fxtos",  0xc7, fmt_Fregrs2_Fregrd},
    { "fitod",  0xc8, fmt_Fregrs2_Fregrd},
    { "fstod",  0xc9, fmt_Fregrs2_Fregrd},
    { "fxtod",  0xcb, fmt_Fregrs2_Fregrd},
    { "fitox",  0xcc, fmt_Fregrs2_Fregrd},
    { "fstox",  0xcd, fmt_Fregrs2_Fregrd},
    { "fdtox",  0xce, fmt_Fregrs2_Fregrd},
    { "fstoi",  0xd1, fmt_Fregrs2_Fregrd},
    { "fdtoi",  0xd2, fmt_Fregrs2_Fregrd},
    { "fxtoi",  0xd3, fmt_Fregrs2_Fregrd},
  };


static const insn_desc_type insn_desc_35[] =
  {

    { "fcmps",  0x51, fmt_Fregrs1_Fregrs2},
    { "fcmpd",  0x52, fmt_Fregrs1_Fregrs2},
    { "fcmpx",  0x53, fmt_Fregrs1_Fregrs2},
    { "fcmpes", 0x55, fmt_Fregrs1_Fregrs2},
    { "fcmped", 0x56, fmt_Fregrs1_Fregrs2},
    { "fcmpex", 0x57, fmt_Fregrs1_Fregrs2},
  };

static word
get_disp22 (word insn)
{
  return (sword (insn) << 10) >> 10;
}

static word
get_simm13 (word insn)
{
  return (sword (insn) << 19) >> 19;
}

static word
get_a (word insn)
{
  return (insn >> 29) & 1;
}

template <unsigned int b, unsigned int w>
word
get_field (word insn)
{
  return (insn >> b) & ((1 << w) - 1);
}

static word get_op (word insn)    { return get_field<30,2> (insn); }
static word get_op2 (word insn)   { return get_field<22,3> (insn); }
static word get_op3 (word insn)   { return get_field<19,6> (insn); }
static word get_opf (word insn)   { return get_field<5,9>  (insn); }
static word get_i (word insn)     { return get_field<13,1> (insn); }
static word get_cond (word insn)  { return get_field<25,4> (insn); }
static word get_rd (word insn)    { return get_field<25,5> (insn); }
static word get_rs1 (word insn)   { return get_field<14,5> (insn); }
static word get_rs2 (word insn)   { return get_field<0,5>  (insn); }
static word get_imm22 (word insn) { return get_field<0,22> (insn); }
static word get_asi (word insn)   { return get_field<5,8>  (insn); }

static void
str_insn (string &s, const char *m = nullptr)
{
  if (m != nullptr)
    s += m;
  size_t sz = s.size();
  if (sz < 8)
    s += &("        "[sz]);
}

static string
str_ireg (word reg)
{
  if (reg == 30)
    return string ("%fp");
  else if (reg == 14)
    return string ("%sp");
  else
    {
      char res[4];

      res[0] = '%';
      res[1] = "goli"[(reg >> 3) & 3];
      res[2] = "01234567"[reg & 7];
      res[3] = 0;

      return string (res);
    }
}

static string
str_freg (word reg)
{
  string res ("%f");
  return res + dec (reg);
}

static string
str_branch (const char *pfx, const char * const map[], word addr, word insn)
{
  string res = pfx + string (map[get_cond (insn)]);

  if (get_a (insn))
    res.append (",a");

  str_insn (res);

  res.append (hex8 (addr + (get_disp22 (insn) << 2)));

  return res;
}

static string
str_unknown (word insn)
{
  return string ("?? " + hex8 (insn));
}

// rs2 (if i=0) or simm13 (if i=1)
static string
str_regimm (word insn)
{
  if (get_i (insn))
    return hex8 (get_simm13 (insn));
  else
    return str_ireg (get_rs2 (insn));
}

static string
str_rs1_regimm (word insn)
{
  string res;
  word rs1 = get_rs1 (insn);

  if (rs1 != 0)
    {
      res += str_ireg (get_rs1 (insn));
      if (get_i (insn) == 0 && get_rs2 (insn) == 0)
	return res;
      res += "+";
    }
  res += str_regimm (insn);
  return res;
}

static string
str_mem (word insn)
{
  return "[" + str_rs1_regimm (insn) + "]";
}


static string
str_move (string src, word insn)
{
  string res;
  
  str_insn (res, "mov");
  res += src;
  res.append (" -> ");
  res.append (str_ireg (get_rd (insn)));
  return res;
}

static string
str_disa (const struct insn_desc_type *map, int nbr,
	  word op, word addr, word insn)
{
  int lo = 0;
  int hi = nbr - 1;
  const struct insn_desc_type *r;

  while (1)
    {
      int mid = (lo + hi) / 2;
      if (map[mid].op > op)
	hi = mid - 1;
      else if (map[mid].op < op)
	lo = mid + 1;
      else
	{
	  r = &map[mid];
	  break;
	}
      if (lo > hi)
	return str_unknown (insn);
    }


  string res;
  if (r->fmt < fmt_SPECIAL)
    str_insn (res, r->name);

  switch (r->fmt)
    {
    case fmt_Rd:		// rdy %rd
      res.append (str_ireg (get_rd (insn)));
      break;
    case fmt_Rs1_Regimm:	// wry rs1,imm or rs1,rs2
      res += str_ireg (get_rs1 (insn));
      res += ",";
      res += str_regimm (insn);
      break;
    case fmt_Rs1_Regimm_Rd:	// xxx rd,rs1,imm   or   xxx rd,rs1,rs2
      res.append (str_ireg (get_rs1 (insn)));
      res.append (", ");
      res.append (str_regimm (insn));
      res.append (" -> ");
      res.append (str_ireg (get_rd (insn)));
      break;
      // fmt_Fp_Mem,
    case fmt_Mem_Fp:
      res.append (str_freg (get_rd (insn)));
      res.append (" <- ");
      res.append (str_mem (insn));
      break;
    case fmt_Mem_Rd:
      res.append (str_mem (insn));
      res.append (" -> ");
      res.append (str_ireg (get_rd (insn)));
      break;
    case fmt_Rd_Mem:
      res.append (str_ireg (get_rd (insn)));
      res.append (" -> ");
      res.append (str_mem (insn));
      break;
      // fmt_Mem_Creg,
    case fmt_Mem:
      res.append (str_mem (insn));
      break;
  // fmt_Ticc,
  // fmt_Fregrs1_Fregrs2,
  // fmt_Fregrs1_Fregrs2_Fregrd,
  // fmt_Fregrs2_Fregrd,
    case fmt_Asi:
      res.append (str_mem (insn));
      res.append (hex2 (get_asi (insn)));
      res.append (" -> ");
      res.append (str_ireg (get_rd (insn)));
      break;
    case fmt_ticc:
      res += "t";
      res += bicc_map[get_cond(insn)];
      str_insn (res);
      res += str_rs1_regimm (insn);
      return res;
    case fmt_or:
      {
	word rs1 = get_rs1 (insn);

	if (rs1 == 0)
	  {
	    if (get_i (insn) == 0)
	      {
		if (get_rs2 (insn) == 0)
		  return str_move (str_ireg (get_rs1 (insn)), insn);
		else if (get_rs1 (insn) == 0)
		  return str_move (str_ireg (get_rs2 (insn)), insn);
	      }
	    else
	      {
		//  Immediate: mov
		word val = get_simm13 (insn);
		if (val == 0)
		  {
		    str_insn (res, "clr");
		    res.append (str_ireg (get_rd (insn)));
		    return res;
		  }
		else
		  {
		    str_insn (res, "mov");
		    res += "0x" + hex8 (val);
		    res.append (" -> ");
		    res.append (str_ireg (get_rd (insn)));
		    return res;
		  }
	      }
	  }
	else if (get_i (insn) == 0 &&  get_rs2 (insn) == 0)
	  return str_move (str_ireg (get_rs2 (insn)), insn);
	str_insn (res, r->name);
	res.append (str_ireg (rs1));
	res.append (", ");
	res.append (str_regimm (insn));
	res.append (" -> ");
	res.append (str_ireg (get_rd (insn)));
	return res;
      }
    case fmt_jmpl:
      {
	word rd = get_rd (insn);

	if (rd == 0)
	  {
	    //  TODO: retl (jmp %o7+8)
	    str_insn (res, "jmp");
	  }
	else
	  {
	    str_insn (res, "jmpl");
	    res += str_ireg (rd);
	    res += ",";
	  }
	res += str_rs1_regimm (insn);
	return res;
      }
      break;
    case fmt_rdy:
      {
	word rs1 = get_rs1 (insn);
	if (rs1 == 0)
	  str_insn (res, "rdy");
	else
	  {
	    str_insn (res, "rd");
	    res += "%asr" + dec (rs1);
	    res += " -> ";
	  }
	res += str_ireg (get_rd (insn));
	return res;
      }
    default:
      res += "???";
    }
  return res;
}

#define countof(ARR) (sizeof(ARR) / sizeof (ARR[0]))

string
disa_sparc (word addr, word insn)
{
  string res;

  switch (get_op (insn))
    {
    case 0x00:
      //  BIcc, SETHI
      switch (get_op2 (insn))
	{
	case 0x0:
	  // unimp
	  {
	    string res;
	    str_insn (res, "unimp");
	    res += hex8 (get_imm22 (insn));
	    return res;
	  }
	case 0x2:
	  // bicc
	  return str_branch ("b", bicc_map, addr, insn);
	case 0x4:
	  // sethi
	  {
	    string res;
	    word imm = get_imm22 (insn) << 10;
	    word rd = get_rd (insn);

	    if (rd == 0)
	      {
		str_insn (res, "nop");
	      }
	    else
	      {
		str_insn (res, "sethi");
		res += "%hi(" + hex8 (imm) + ")";
		res += " -> " + str_ireg (get_rd (insn));
	      }
	    return res;
	  }
	case 0x6:
	  // fbfcc
	  return str_branch ("fb", fbfcc_map, addr, insn);
	case 0x7:
	  // cbcc
	  return str_branch ("cb", bicc_map, addr, insn);
	default:
	  return str_unknown (insn);
	}
      break;
    case 0x01:
      // Call
      {
	string res;
	str_insn (res, "call");
	word targ = addr + (insn << 2);
	res.append (hex8 (targ));
	return res;
      }
    case 0x02:
      //  desc_10
      {
	word op3 = get_op3 (insn);

	if (op3 == 0x34)
	  return str_disa (insn_desc_34, countof (insn_desc_34),
			   get_opf (insn), addr, insn);
	else if (op3 == 0x35)
	  return str_disa (insn_desc_35, countof (insn_desc_35),
			   get_opf (insn), addr, insn);
	else
	  return str_disa (insn_desc_10, countof (insn_desc_10),
			   op3, addr, insn);
      }
    case 0x03:
      //  desc_11
      return str_disa (insn_desc_11, countof (insn_desc_11),
		       get_op3 (insn), addr, insn);
    }
  return str_unknown (insn);
}
