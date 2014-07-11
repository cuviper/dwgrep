#include <iostream>

#include "atval.hh"
#include "dwcst.hh"
#include "dwit.hh"
#include "make_unique.hh"
#include "op.hh"
#include "tree.hh"
#include "value.hh"
#include "vfcst.hh"

namespace
{
  template <class T>
  cmp_result
  compare (T const &a, T const &b)
  {
    if (a < b)
      return cmp_result::less;
    if (b < a)
      return cmp_result::greater;
    return cmp_result::equal;
  }

  value_type
  alloc_vtype (char const *name)
  {
    static uint8_t last = 0;
    value_type vt {++last, name};
    if (last == 0)
      {
	std::cerr << "Ran out of value type identifiers." << std::endl;
	std::terminate ();
      }
    return vt;
  }

  std::vector <std::pair <uint8_t, char const *>> &
  get_vtype_names ()
  {
    static std::vector <std::pair <uint8_t, char const *>> names;
    return names;
  }

  char const *
  find_vtype_name (uint8_t code)
  {
    auto &vtn = get_vtype_names ();
    auto it = std::find_if (vtn.begin (), vtn.end (),
			    [code] (std::pair <uint8_t, char const *> const &v)
			    { return v.first == code; });
    if (it == vtn.end ())
      return nullptr;
    else
      return it->second;
  }
}

void
value_type::register_name (uint8_t code, char const *name)
{
  auto &vtn = get_vtype_names ();
  assert (find_vtype_name (code) == nullptr);
  vtn.push_back (std::make_pair (code, name));
}

char const *
value_type::name () const
{
  auto ret = find_vtype_name (m_code);
  assert (ret != nullptr);
  return ret;
}

value_type const value_type::none = alloc_vtype ("T_NONE");

std::ostream &
operator<< (std::ostream &o, value const &v)
{
  v.show (o);
  return o;
}

value_type const value_cst::vtype = alloc_vtype ("T_CST");

void
value_cst::show (std::ostream &o) const
{
  o << m_cst;
}

std::unique_ptr <value>
value_cst::clone () const
{
  return std::make_unique <value_cst> (*this);
}

constant
value_cst::get_type_const () const
{
  return {(int) slot_type_id::T_CONST, &slot_type_dom};
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


value_type const value_str::vtype = alloc_vtype ("T_STR");

void
value_str::show (std::ostream &o) const
{
  o << m_str;
}

std::unique_ptr <value>
value_str::clone () const
{
  return std::make_unique <value_str> (*this);
}

constant
value_str::get_type_const () const
{
  return {(int) slot_type_id::T_STR, &slot_type_dom};
}

cmp_result
value_str::cmp (value const &that) const
{
  if (auto v = value::as <value_str> (&that))
    return compare (m_str, v->m_str);
  else
    return cmp_result::fail;
}


value_type const value_seq::vtype = alloc_vtype ("T_SEQ");

namespace
{
  value_seq::seq_t
  clone_seq (value_seq::seq_t const &seq)
  {
    value_seq::seq_t seq2;
    for (auto const &v: seq)
      seq2.emplace_back (std::move (v->clone ()));
    return seq2;
  }
}

value_seq::value_seq (value_seq const &that)
  : value {that}
  , m_seq {std::make_shared <seq_t> (clone_seq (*that.m_seq))}
{}

void
value_seq::show (std::ostream &o) const
{
  o << "[";
  bool seen = false;
  for (auto const &v: *m_seq)
    {
      if (seen)
	o << ", ";
      seen = true;
      o << *v;
    }
  o << "]";
}

std::unique_ptr <value>
value_seq::clone () const
{
  return std::make_unique <value_seq> (*this);
}

constant
value_seq::get_type_const () const
{
  return {(int) slot_type_id::T_SEQ, &slot_type_dom};
}

namespace
{
  template <class Callable>
  cmp_result
  compare_sequences (value_seq::seq_t const &sa, value_seq::seq_t const &sb,
		     Callable cmp)
  {
    cmp_result ret = cmp_result::fail;
    auto mm = std::mismatch (sa.begin (), sa.end (), sb.begin (),
			     [&ret, cmp] (std::unique_ptr <value> const &a,
					  std::unique_ptr <value> const &b)
			     {
			       ret = cmp (a, b);
			       assert (ret != cmp_result::fail);
			       return ret == cmp_result::equal;
			     });

    if (mm.first != sa.end ())
      {
	assert (mm.second != sb.end ());
	assert (ret != cmp_result::fail);
	return ret;
      }

    return cmp_result::equal;
  }
}

cmp_result
value_seq::cmp (value const &that) const
{
  if (auto v = value::as <value_seq> (&that))
    {
      cmp_result ret = compare (m_seq->size (), v->m_seq->size ());
      if (ret != cmp_result::equal)
	return ret;

      ret = compare_sequences (*m_seq, *v->m_seq,
			       [] (std::unique_ptr <value> const &a,
				   std::unique_ptr <value> const &b)
			       {
				 return compare (a->get_type (),
						 b->get_type ());
			       });
      if (ret != cmp_result::equal)
	return ret;

      return compare_sequences (*m_seq, *v->m_seq,
				[] (std::unique_ptr <value> const &a,
				    std::unique_ptr <value> const &b)
				{ return a->cmp (*b); });
    }
  else
    return cmp_result::fail;
}


value_type const value_closure::vtype = alloc_vtype ("T_CLOSURE");

value_closure::value_closure (tree const &t, dwgrep_graph::sptr q,
			      std::shared_ptr <scope> scope,
			      std::shared_ptr <frame> frame, size_t pos)
  : value {vtype, pos}
  , m_t {std::make_unique <tree> (t)}
  , m_q {q}
  , m_scope {scope}
  , m_frame {frame}
{}

value_closure::value_closure (value_closure const &that)
  : value_closure {*that.m_t, that.m_q, that.m_scope, that.m_frame,
		   that.get_pos ()}
{}

value_closure::~value_closure()
{}

void
value_closure::show (std::ostream &o) const
{
  o << "closure(" << *m_t << ")";
}

std::unique_ptr <value>
value_closure::clone () const
{
  return std::make_unique <value_closure> (*this);
}

constant
value_closure::get_type_const () const
{
  return {(int) slot_type_id::T_CLOSURE, &slot_type_dom};
}

cmp_result
value_closure::cmp (value const &v) const
{
  if (auto that = value::as <value_closure> (&v))
    {
      auto a = std::make_tuple (static_cast <tree &> (*m_t), m_scope, m_frame);
      auto b = std::make_tuple (static_cast <tree &> (*that->m_t),
				that->m_scope, that->m_frame);
      return compare (a, b);
    }
  else
    return cmp_result::fail;
}


value_type const value_die::vtype = alloc_vtype ("T_DIE");

void
value_die::show (std::ostream &o) const
{
  std::ios::fmtflags f {o.flags ()};

  Dwarf_Die *die = const_cast <Dwarf_Die *> (&m_die);
  o << "[" << std::hex << dwarf_dieoffset (die) << "]\t"
    << constant (dwarf_tag (die), &dw_tag_short_dom);

  for (auto it = attr_iterator {die}; it != attr_iterator::end (); ++it)
    {
      o << "\n\t";
      o.flags (f);
      value_attr {m_gr, **it, m_die, 0}.show (o);
    }

  o.flags (f);
}

std::unique_ptr <value>
value_die::clone () const
{
  return std::make_unique <value_die> (*this);
}

constant
value_die::get_type_const () const
{
  return {(int) slot_type_id::T_NODE, &slot_type_dom};
}

cmp_result
value_die::cmp (value const &that) const
{
  if (auto v = value::as <value_die> (&that))
    return compare (dwarf_dieoffset ((Dwarf_Die *) &m_die),
		    dwarf_dieoffset ((Dwarf_Die *) &v->m_die));
  else
    return cmp_result::fail;
}


value_type const value_attr::vtype = alloc_vtype ("T_AT");

void
value_attr::show (std::ostream &o) const
{
  unsigned name = (unsigned) dwarf_whatattr ((Dwarf_Attribute *) &m_attr);
  unsigned form = dwarf_whatform ((Dwarf_Attribute *) &m_attr);
  std::ios::fmtflags f {o.flags ()};
  o << constant (name, &dw_attr_short_dom) << " ("
    << constant (form, &dw_form_short_dom) << ")\t";
  auto v = at_value (m_attr, m_die, m_gr);
  if (auto d = value::as <value_die> (v.get ()))
    o << "[" << std::hex
      << dwarf_dieoffset ((Dwarf_Die *) &d->get_die ()) << "]";
  else
    v->show (o);
  o.flags (f);
}

std::unique_ptr <value>
value_attr::clone () const
{
  return std::make_unique <value_attr> (*this);
}

constant
value_attr::get_type_const () const
{
  return {(int) slot_type_id::T_ATTR, &slot_type_dom};
}

cmp_result
value_attr::cmp (value const &that) const
{
  if (auto v = value::as <value_attr> (&that))
    {
      Dwarf_Off a = dwarf_dieoffset ((Dwarf_Die *) &m_die);
      Dwarf_Off b = dwarf_dieoffset ((Dwarf_Die *) &v->m_die);
      if (a != b)
	return compare (a, b);
      else
	return compare (dwarf_whatattr ((Dwarf_Attribute *) &m_attr),
			dwarf_whatattr ((Dwarf_Attribute *) &v->m_attr));
    }
  else
    return cmp_result::fail;
}
