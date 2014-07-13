#include <memory>
#include "make_unique.hh"
#include <iostream>

#include "value-seq.hh"
#include "vfcst.hh"
#include "overload.hh"
#include "builtin-add.hh"
#include "builtin-length.hh"

value_type const value_seq::vtype = value_type::alloc ("T_SEQ");

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

valfile::uptr
op_add_seq::next ()
{
  if (auto vf = m_upstream->next ())
    {
      auto vp = vf->pop ();
      assert (vp->is <value_seq> ());
      auto &v = static_cast <value_seq &> (*vp);

      auto wp = vf->pop ();
      // XXX add arity to the framework
      assert (wp->is <value_seq> ());
      auto &w = static_cast <value_seq &> (*wp);

      value_seq::seq_t res;
      for (auto const &x: *w.get_seq ())
	res.emplace_back (x->clone ());
      for (auto const &x: *v.get_seq ())
	res.emplace_back (x->clone ());

      vf->push (std::make_unique <value_seq> (std::move (res), 0));
      return vf;
    }

  return nullptr;
}

valfile::uptr
op_length_seq::next ()
{
  if (auto vf = m_upstream->next ())
    {
      auto vp = vf->pop ();
      assert (vp->is <value_seq> ());
      auto &v = static_cast <value_seq &> (*vp);
      constant t {v.get_seq ()->size (), &dec_constant_dom};
      vf->push (std::make_unique <value_cst> (t, 0));
      return vf;
    }

  return nullptr;
}