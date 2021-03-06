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

#include <cassert>
#include <algorithm>
#include <memory>

#include "cache.hh"
#include "dwpp.hh"
#include "dwgrep.hh"
#include "dwit.hh"

void
parent_cache::recursively_populate_unit (unit_cache_t &uc, Dwarf_Die die,
					 Dwarf_Off paroff)
{
  while (true)
    {
      Dwarf_Off off = dwarf_dieoffset (&die);
      uc.push_back (std::make_pair (off, paroff));

      if (dwarf_haschildren (&die))
	{
	  Dwarf_Die child;
	  if (dwarf_child (&die, &child) != 0)
	    throw_libdw ();
	  recursively_populate_unit (uc, child, off);
	}

      switch (dwarf_siblingof (&die, &die))
	{
	case 0:
	  break;
	case -1:
	  throw_libdw ();
	case 1:
	  return;
	}
    }
}

parent_cache::unit_cache_t
parent_cache::populate_unit (Dwarf_Die die)
{
  unit_cache_t uc;
  recursively_populate_unit (uc, die, no_off);
  return uc;
}

Dwarf_Off
parent_cache::find (Dwarf_Die die)
{
  Dwarf_Die cudie;
  if (dwarf_diecu (&die, &cudie, nullptr, nullptr) == nullptr)
    throw_libdw ();

  Dwarf_Off cuoff = dwarf_dieoffset (&cudie);
  auto key = unit_key {unit_type::INFO, cuoff};
  auto it = m_cache.find (key);
  if (it == m_cache.end ())
    it = m_cache.insert (std::make_pair (key, populate_unit (cudie))).first;

  Dwarf_Off dieoff = dwarf_dieoffset (&die);
  auto jt = std::lower_bound
    (it->second.begin (), it->second.end (), dieoff,
     [] (std::pair <Dwarf_Off, Dwarf_Off> const &a, Dwarf_Off b)
     {
       return a.first < b;
     });

  assert (jt != it->second.end ());
  assert (jt->first == dieoff);
  return jt->second;
}


bool
root_cache::is_root (Dwarf_Die die, Dwarf *dw)
{
  if (m_roots == nullptr)
    {
      m_roots = std::make_unique <root_map_t> ();
      for (auto it = cu_iterator { dw }; it != cu_iterator::end (); ++it)
	m_roots->insert (std::make_pair (unit_type::INFO,
					 dwarf_dieoffset (*it)));
    }

  Dwarf_Off off = dwarf_dieoffset (&die);
  return m_roots->find (std::make_pair (unit_type::INFO, off))
    != m_roots->end ();
}
