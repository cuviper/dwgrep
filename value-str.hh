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

#ifndef _VALUE_STR_H_
#define _VALUE_STR_H_

#include <string>
#include "value.hh"
#include "op.hh"
#include "overload.hh"

class value_str
  : public value
{
  std::string m_str;

public:
  static value_type const vtype;

  value_str (std::string &&str, size_t pos)
    : value {vtype, pos}
    , m_str {std::move (str)}
  {}

  std::string &get_string ()
  { return m_str; }

  void show (std::ostream &o, brevity brv) const override;
  std::unique_ptr <value> clone () const override;
  cmp_result cmp (value const &that) const override;
};

struct op_add_str
  : public op_overload <value_str, value_str>
{
  using op_overload::op_overload;

  std::unique_ptr <value> operate (std::unique_ptr <value_str> a,
				   std::unique_ptr <value_str> b) override;
};

struct op_length_str
  : public op_overload <value_str>
{
  using op_overload::op_overload;

  std::unique_ptr <value> operate (std::unique_ptr <value_str> a) override;
};

struct op_elem_str
  : public op_yielding_overload <value_str>
{
  using op_yielding_overload::op_yielding_overload;

  std::unique_ptr <value_producer>
  operate (std::unique_ptr <value_str> a) override;
};

struct op_relem_str
  : public op_yielding_overload <value_str>
{
  using op_yielding_overload::op_yielding_overload;

  std::unique_ptr <value_producer>
  operate (std::unique_ptr <value_str> a) override;
};

struct pred_empty_str
  : public pred_overload <value_str>
{
  using pred_overload::pred_overload;
  pred_result result (value_str &a) override;
};

struct pred_find_str
  : public pred_overload <value_str, value_str>
{
  using pred_overload::pred_overload;
  pred_result result (value_str &haystack, value_str &needle) override;
};

struct pred_starts_str
  : public pred_overload <value_str, value_str>
{
  using pred_overload::pred_overload;
  pred_result result (value_str &haystack, value_str &needle) override;
};

struct pred_ends_str
  : public pred_overload <value_str, value_str>
{
  using pred_overload::pred_overload;
  pred_result result (value_str &haystack, value_str &needle) override;
};

struct pred_match_str
  : public pred_overload <value_str, value_str>
{
  using pred_overload::pred_overload;
  pred_result result (value_str &haystack, value_str &needle) override;
};

#endif /* _VALUE_STR_H_ */
