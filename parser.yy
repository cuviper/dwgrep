%code top { // -*-c++-*-
/*
   Copyright (C) 2014 Red Hat, Inc.
   This file is part of dwgrep.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   dwgrep is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include "parser.hh"
}

%code requires {
  #include <memory>
  #include "tree.hh"

  struct strlit
  {
    const char *buf;
    size_t len;
  };

  // A helper structure that the lexer uses when parsing string
  // literals.
  struct fmtlit
  {
    std::string str;
    tree t;
    size_t level;
    bool in_string;
    bool raw;

    explicit fmtlit (bool a_raw);

    void flush_str ();
    std::string yank_str ();
  };
}

%code provides {
  tree parse_query (builtin_dict const &builtins, std::string str);
  tree parse_query (builtin_dict const &builtins,
		    char const *begin, char const *end);

  // These two are for sub-expression parsing.
  tree parse_subquery (builtin_dict const &builtins, std::string str);
  tree parse_subquery (builtin_dict const &builtins,
		       char const *begin, char const *end);
}

%{
  #include <sstream>
  #include <iostream>

  #include "lexer.hh"
  #include "constant.hh"
  #include "tree_cr.hh"
  #include "builtin.hh"

  namespace
  {
    void
    yyerror (std::unique_ptr <tree> &t, yyscan_t lex,
	     builtin_dict const &builtins, char const *s)
    {
      fprintf (stderr, "%s\n", s);
    }

    template <tree_type TT>
    tree *
    positive_assert ()
    {
      return tree::create_assert (tree::create_nullary <TT> ());
    }

    template <tree_type TT>
    tree *
    negative_assert ()
    {
      auto u = tree::create_neg (tree::create_nullary <TT> ());
      return tree::create_assert (u);
    }

    constant
    parse_int (strlit str)
    {
      const char *buf = str.buf;
      size_t len = str.len;

      bool sign = buf[0] == '-';
      if (sign)
	{
	  buf += 1;
	  len -= 1;
	}

      int base;
      constant_dom const *dom;
      if (len > 2 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'))
	{
	  base = 16;
	  buf += 2;
	  len -= 2;
	  dom = &hex_constant_dom;
	}
      else if (len > 2 && buf[0] == '0' && (buf[1] == 'b' || buf[1] == 'B'))
	{
	  base = 2;
	  buf += 2;
	  len -= 2;
	  dom = &bin_constant_dom;
	}
      else if (len > 2 && buf[0] == '0' && (buf[1] == 'o' || buf[1] == 'O'))
	{
	  base = 8;
	  buf += 2;
	  len -= 2;
	  dom = &oct_constant_dom;
	}
      else if (len > 1 && buf[0] == '0')
	{
	  base = 8;
	  buf += 1;
	  len -= 1;
	  dom = &oct_constant_dom;
	}
      else
	{
	  base = 10;
	  dom = &dec_constant_dom;
	}

      size_t pos;
      uint64_t val = std::stoull ({buf, len}, &pos, base);
      if (pos < len)
	throw std::runtime_error
	    (std::string ("Invalid integer literal: `") + str.buf + "'");

      mpz_class ret = val;
      if (sign)
	ret = -ret;

      return constant {ret, dom};
    }

    tree *
    parse_word (builtin_dict const &builtins, std::string str)
    {
      if (auto bi = builtins.find (str))
	return tree::create_builtin (bi);
      else
	return tree::create_str <tree_type::READ> (str);
    }

    tree *
    parse_cmp (builtin_dict const &builtins, tree *a, tree *b, char const *word)
    {
      // A <op> B → ?([A] elem ->.tmp; [B] elem .tmp swap <WORD>)
      auto bi_elem = builtins.find ("elem");
      auto bi_swap = builtins.find ("swap");
      auto bi_cmp = builtins.find (word);

      if (a == nullptr)
	a = tree::create_nullary <tree_type::NOP> ();
      if (b == nullptr)
	b = tree::create_nullary <tree_type::NOP> ();

      tree *ret = nullptr;

      ret = tree::create_cat <tree_type::CAT>
		(ret, tree::create_unary <tree_type::CAPTURE> (a));
      ret = tree::create_cat <tree_type::CAT>
		(ret, tree::create_builtin (bi_elem));
      ret = tree::create_cat <tree_type::CAT>
		(ret, tree::create_str <tree_type::BIND> (".tmp"));

      ret = tree::create_cat <tree_type::CAT>
		(ret, tree::create_unary <tree_type::CAPTURE> (b));
      ret = tree::create_cat <tree_type::CAT>
		(ret, tree::create_builtin (bi_elem));

      ret = tree::create_cat <tree_type::CAT>
		(ret, tree::create_str <tree_type::READ> (".tmp"));
      ret = tree::create_cat <tree_type::CAT>
		(ret, tree::create_builtin (bi_swap));
      ret = tree::create_cat <tree_type::CAT>
		(ret, tree::create_builtin (bi_cmp));

      ret = tree::create_unary <tree_type::PRED_SUBX_ANY> (ret);
      ret = tree::create_assert (ret);

      return ret;
    }
  }

  fmtlit::fmtlit (bool a_raw)
    : t {tree_type::FORMAT}
    , level {0}
    , in_string {false}
    , raw {a_raw}
  {}

  void
  fmtlit::flush_str ()
  {
    t.take_child (tree::create_str <tree_type::STR> (str));
    str = "";
  }

  std::string
  fmtlit::yank_str ()
  {
    std::string tmp = str;
    str = "";
    return tmp;
  }

  tree *
  tree_for_id_block (builtin_dict const &builtins,
		     std::vector <std::string> *ids)
  {
    tree *ret = nullptr;
    for (auto const &s: *ids)
      if (builtins.find (s) == nullptr)
	{
	  auto t = tree::create_str <tree_type::BIND> (s);
	  ret = tree::create_cat <tree_type::CAT> (ret, t);
	}
      else
	throw std::runtime_error
	    (std::string ("Can't rebind a builtin: `") + s + "'");

    return ret;
  }
%}

%pure-parser
%error-verbose
%parse-param { std::unique_ptr <tree> &ret }
%parse-param { void *yyscanner }
%parse-param { builtin_dict const &builtins }
%lex-param { yyscanner }

%token TOK_LPAREN TOK_RPAREN TOK_LBRACKET TOK_RBRACKET TOK_LBRACE TOK_RBRACE
%token TOK_QMARK_LPAREN TOK_BANG_LPAREN

%token TOK_ASTERISK TOK_PLUS TOK_QMARK TOK_COMMA TOK_COLON
%token TOK_SEMICOLON TOK_VBAR TOK_DOUBLE_VBAR TOK_ARROW TOK_ASSIGN
%token TOK_EQ TOK_NE TOK_LT TOK_LE TOK_GT TOK_GE

%token TOK_IF TOK_THEN TOK_ELSE TOK_LET TOK_WORD TOK_LIT_STR
%token TOK_LIT_INT

   // XXX These should eventually be moved to builtins.
%token TOK_DEBUG

%token TOK_EOF

%union {
  tree *t;
  strlit s;
  fmtlit *f;
  std::vector <std::string> *ids;
 }

%type <t> Program AltList OrList EqList StatementList Statement
%type <ids> IdList IdListOpt IdBlockOpt
%type <s> TOK_LIT_INT
%type <s> TOK_WORD
%type <t> TOK_LIT_STR

%%

Query: Program TOK_EOF
  {
    ret.reset ($1);
    YYACCEPT;
  }

Program: AltList
  {
    $$ = $1 != nullptr ? $1 : tree::create_nullary <tree_type::NOP> ();
  }

AltList:
   OrList

   | OrList TOK_COMMA AltList
   {
     $$ = tree::create_cat <tree_type::ALT>
       ($1 != nullptr ? $1 : tree::create_nullary <tree_type::NOP> (),
	$3 != nullptr ? $3 : tree::create_nullary <tree_type::NOP> ());
   }

OrList:
  EqList

  | EqList TOK_DOUBLE_VBAR OrList
  {
    $$ = tree::create_cat <tree_type::OR>
       ($1 != nullptr ? $1 : tree::create_nullary <tree_type::NOP> (),
	$3 != nullptr ? $3 : tree::create_nullary <tree_type::NOP> ());
  }

EqList:
  StatementList

  | StatementList TOK_EQ StatementList
  { $$ = parse_cmp (builtins, $1, $3, "?eq"); }

  | StatementList TOK_NE StatementList
  { $$ = parse_cmp (builtins, $1, $3, "?ne"); }

  | StatementList TOK_LT StatementList
  { $$ = parse_cmp (builtins, $1, $3, "?lt"); }

  | StatementList TOK_LE StatementList
  { $$ = parse_cmp (builtins, $1, $3, "?le"); }

  | StatementList TOK_GT StatementList
  { $$ = parse_cmp (builtins, $1, $3, "?gt"); }

  | StatementList TOK_GE StatementList
  { $$ = parse_cmp (builtins, $1, $3, "?ge"); }

StatementList:
  /* eps. */
  { $$ = nullptr; }

  | Statement StatementList
  {
    $$ = tree::create_cat <tree_type::CAT> ($1, $2);
  }

IdListOpt:
  /* eps. */
  {
    $$ = new std::vector <std::string> ();
  }

  | IdList

IdList:
  TOK_WORD IdListOpt
  {
    $2->push_back (std::string {$1.buf, $1.len});
    $$ = $2;
  }

IdBlockOpt:
  /* eps. */
  {
    $$ = new std::vector <std::string> ();
  }

  | TOK_VBAR IdList TOK_VBAR
  { $$ = $2; }

Statement:
  TOK_LPAREN Program TOK_RPAREN
  { $$ = $2; }

  | TOK_QMARK_LPAREN Program TOK_RPAREN
  {
    auto t = tree::create_unary <tree_type::PRED_SUBX_ANY> ($2);
    $$ = tree::create_assert (t);
  }

  | TOK_BANG_LPAREN Program TOK_RPAREN
  {
    auto t = tree::create_unary <tree_type::PRED_SUBX_ANY> ($2);
    auto u = tree::create_neg (t);
    $$ = tree::create_assert (u);
  }

  | TOK_LBRACKET TOK_RBRACKET
  {
    $$ = tree::create_nullary <tree_type::EMPTY_LIST> ();
  }

  | TOK_LBRACKET IdBlockOpt Program TOK_RBRACKET
  {
    $$ = tree::create_cat <tree_type::CAT>
	  (tree_for_id_block (builtins, $2),
	   tree::create_unary <tree_type::CAPTURE> ($3));

    if ($2->size () > 0)
      $$ = tree::create_scope <tree_type::SCOPE> ($$);

    delete $2;
  }

  | TOK_LBRACE IdBlockOpt Program TOK_RBRACE
  {
    $$ = tree::create_cat <tree_type::CAT>
	  (tree_for_id_block (builtins, $2), $3);
    $$ = tree::create_unary <tree_type::BLOCK> ($$);
  }

  | TOK_ARROW IdList TOK_SEMICOLON
  {
    assert ($2->size () > 0);
    $$ = nullptr;
    for (auto const &s: *$2)
      if (builtins.find (s) == nullptr)
	{
	  auto t = tree::create_str <tree_type::BIND> (s);
	  $$ = tree::create_cat <tree_type::CAT> ($$, t);
	}
      else
	throw std::runtime_error
	    (std::string ("Can't rebind a builtin: `") + s + "'");
  }

  | TOK_LET IdList TOK_ASSIGN Program TOK_SEMICOLON
  {
    $$ = tree::create_const <tree_type::SUBX_EVAL>
	  (constant {$2->size (), &dec_constant_dom});
    $$->take_child ($4);

    for (auto const &s: *$2)
      if (builtins.find (s) == nullptr)
	{
	  auto t = tree::create_str <tree_type::BIND> (s);
	  $$ = tree::create_cat <tree_type::CAT> ($$, t);
	}
      else
	throw std::runtime_error
	    (std::string ("Can't rebind a builtin: `") + s + "'");

    delete $2;
  }

  | Statement TOK_ASTERISK
  { $$ = tree::create_unary <tree_type::CLOSE_STAR> ($1); }

  | Statement TOK_PLUS
  {
    auto t = new tree (*$1);
    auto u = tree::create_unary <tree_type::CLOSE_STAR> ($1);
    $$ = tree::create_cat <tree_type::CAT> (t, u);
  }

  | Statement TOK_QMARK
  {
    auto t = tree::create_nullary <tree_type::NOP> ();
    $$ = tree::create_cat <tree_type::ALT> ($1, t);
  }

  | TOK_IF Statement TOK_THEN Statement TOK_ELSE Statement
  { $$ = tree::create_ternary <tree_type::IFELSE> ($2, $4, $6); }

  | TOK_LIT_INT
  { $$ = tree::create_const <tree_type::CONST> (parse_int ($1)); }


  | TOK_WORD
  { $$ = parse_word (builtins, {$1.buf, $1.len}); }

  | TOK_WORD TOK_COLON Statement
  {
    $$ = tree::create_cat <tree_type::CAT>
	($3, parse_word (builtins, {$1.buf, $1.len}));
  }

  | TOK_LIT_STR
  {
    // For string literals, we get back a tree_type::FMT node with
    // children that are a mix of tree_type::STR (which are actual
    // literals) and other node types with the embedded programs.
    // That comes directly from lexer, just return it.
    $$ = $1;
  }


  | TOK_DEBUG
  { $$ = tree::create_nullary <tree_type::F_DEBUG> (); }

%%

struct lexer
{
  yyscan_t m_sc;

  explicit lexer (builtin_dict const &builtins,
		  char const *begin, char const *end)
  {
    if (yylex_init_extra (&builtins, &m_sc) != 0)
      throw std::runtime_error ("Can't init lexer.");
    yy_scan_bytes (begin, end - begin, m_sc);
  }

  ~lexer ()
  {
    yylex_destroy (m_sc);
  }

  lexer (lexer const &that) = delete;
};

tree
parse_query (builtin_dict const &builtins, std::string str)
{
  char const *buf = str.c_str ();
  return parse_query (builtins, buf, buf + str.length ());
}

tree
parse_subquery (builtin_dict const &builtins, std::string str)
{
  char const *buf = str.c_str ();
  return parse_subquery (builtins, buf, buf + str.length ());
}

tree
parse_query (builtin_dict const &builtins,
	     char const *begin, char const *end)
{
  return tree::promote_scopes (parse_subquery (builtins, begin, end));
}

tree
parse_subquery (builtin_dict const &builtins,
		char const *begin, char const *end)
{
  lexer lex {builtins, begin, end};
  std::unique_ptr <tree> t;
  if (yyparse (t, lex.m_sc, builtins) == 0)
    return *t;
  throw std::runtime_error ("syntax error");
}
