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

#include <iostream>
#include <memory>

#include "value-cst.hh"

value_type const value_cst::vtype = value_type::alloc ("T_CONST");

void
value_cst::show (std::ostream &o, brevity brv) const
{
  o << m_cst;
}

std::unique_ptr <value>
value_cst::clone () const
{
  return std::make_unique <value_cst> (*this);
}

cmp_result
value_cst::cmp (value const &that) const
{
  if (auto v = value::as <value_cst> (&that))
    {
      // We don't want to evaluate as equal two constants from
      // different domains just because they happen to have the same
      // value.
      if (! m_cst.dom ()->safe_arith () || ! v->m_cst.dom ()->safe_arith ())
	{
	  cmp_result ret = compare (m_cst.dom (), v->m_cst.dom ());
	  if (ret != cmp_result::equal)
	    return ret;
	}

      // Either they are both arithmetic, or they are both from the
      // same non-arithmetic domain.  We can directly compare the
      // values now.
      return compare (m_cst, v->m_cst);
    }
  else
    return cmp_result::fail;
}

std::unique_ptr <value>
op_value_cst::operate (std::unique_ptr <value_cst> a)
{
  constant cst {a->get_constant ().value (), &dec_constant_dom};
  return std::make_unique <value_cst> (cst, 0);
}

namespace
{
  template <class F>
  std::unique_ptr <value>
  simple_arith_op (value_cst const &a, value_cst const &b, F f)
  {
    constant const &cst_a = a.get_constant ();
    constant const &cst_b = b.get_constant ();

    check_arith (cst_a, cst_b);

    constant_dom const *d = cst_a.dom ()->plain ()
      ? cst_b.dom () : cst_a.dom ();

    try
      {
	return f (cst_a, cst_b, d);
      }
    catch (std::domain_error &e)
      {
	std::cerr << "Error: " << e.what () << std::endl;
	return nullptr;
      }
  }
}

std::unique_ptr <value>
op_add_cst::operate (std::unique_ptr <value_cst> a,
		     std::unique_ptr <value_cst> b)
{
  return simple_arith_op
    (*a, *b,
     [] (constant const &cst_a, constant const &cst_b,
	 constant_dom const *d)
     {
       constant r {cst_a.value () + cst_b.value (), d};
       return std::make_unique <value_cst> (r, 0);
     });
}

std::unique_ptr <value>
op_sub_cst::operate (std::unique_ptr <value_cst> a,
		     std::unique_ptr <value_cst> b)
{
  return simple_arith_op
    (*a, *b,
     [] (constant const &cst_a, constant const &cst_b,
	 constant_dom const *d)
     {
       constant r {cst_a.value () - cst_b.value (), d};
       return std::make_unique <value_cst> (r, 0);
     });
}

std::unique_ptr <value>
op_mul_cst::operate (std::unique_ptr <value_cst> a,
		     std::unique_ptr <value_cst> b)
{
  return simple_arith_op
    (*a, *b,
     [] (constant const &cst_a, constant const &cst_b,
	 constant_dom const *d) -> std::unique_ptr <value>
     {
       constant r {cst_a.value () * cst_b.value (), d};
       return std::make_unique <value_cst> (r, 0);
     });
}

std::unique_ptr <value>
op_div_cst::operate (std::unique_ptr <value_cst> a,
		     std::unique_ptr <value_cst> b)
{
  return simple_arith_op
    (*a, *b,
     [] (constant const &cst_a, constant const &cst_b,
	 constant_dom const *d) -> std::unique_ptr <value>
     {
       constant r {cst_a.value () / cst_b.value (), d};
       return std::make_unique <value_cst> (r, 0);
     });
}

std::unique_ptr <value>
op_mod_cst::operate (std::unique_ptr <value_cst> a,
		     std::unique_ptr <value_cst> b)
{
  return simple_arith_op
    (*a, *b,
     [] (constant const &cst_a, constant const &cst_b,
	 constant_dom const *d) -> std::unique_ptr <value>
     {
       constant r {cst_a.value () % cst_b.value (), d};
       return std::make_unique <value_cst> (r, 0);
     });
}
