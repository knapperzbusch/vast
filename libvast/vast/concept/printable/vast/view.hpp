/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#pragma once

#include "vast/access.hpp"
#include "vast/concept/printable/core/printer.hpp"
#include "vast/concept/printable/numeric.hpp"
#include "vast/concept/printable/print.hpp"
#include "vast/concept/printable/std/chrono.hpp"
#include "vast/concept/printable/string.hpp"
#include "vast/concept/printable/vast/address.hpp"
#include "vast/concept/printable/vast/none.hpp"
#include "vast/concept/printable/vast/port.hpp"
#include "vast/concept/printable/vast/subnet.hpp"
#include "vast/detail/escapers.hpp"
#include "vast/detail/overload.hpp"
#include "vast/detail/string.hpp"
#include "vast/view.hpp"

namespace vast {

// view<T> resolves to just T for all primitive types such as numbers as well
// as IP addresses, etc. Hence, we only need to deal with a couple of view
// types here.

// -- printer implementations --------------------------------------------------

struct data_view_printer : printer<data_view_printer> {
  using attribute = view<data>;

  template <class Iterator>
  bool print(Iterator& out, const attribute& d) const {
    auto f = detail::overload(
      [&](const auto& x) {
        return make_printer<std::decay_t<decltype(x)>>{}(out, x);
      },
      [&](const view<integer>& x) {
        return printers::integral<integer, policy::force_sign>(out, x);
      },
      [&](const view<std::string>& x) {
        static auto escaper = detail::make_extra_print_escaper("\"");
        static auto p = '"' << printers::escape(escaper) << '"';
        return p(out, x);
      });
    return caf::visit(f, d);
  }
};

struct pattern_view_printer : printer<pattern_view_printer> {
  using attribute = view<pattern>;

  template <class Iterator>
  bool print(Iterator& out, const attribute& pat) const {
    auto p = '/' << printers::str << '/';
    return p.print(out, pat.string());
  }
};

struct vector_view_printer : printer<vector_view_printer> {
  using attribute = view<vector>;

  template <class Iterator>
  bool print(Iterator& out, const attribute& xs) const {
    if (!xs || xs->empty())
      return printers::str.print(out, "[]");
    auto p = '[' << ~(data_view_printer{} % ", ") << ']';
    return p.print(out, xs);
  }
};

struct set_view_printer : printer<set_view_printer> {
  using attribute = view<set>;

  template <class Iterator>
  bool print(Iterator& out, const attribute& xs) const {
    if (!xs || xs->empty())
      return printers::str.print(out, "{}");
    auto p = '{' << ~(data_view_printer{} % ", ") << '}';
    return p.print(out, xs);
  }
};

struct map_view_printer : printer<map_view_printer> {
  using attribute = view<map>;

  template <class Iterator>
  bool print(Iterator& out, const attribute& xs) const {
    if (!xs || xs->empty())
      return printers::str.print(out, "{-}");
    auto kvp = data_view_printer{} << " -> " << data_view_printer{};
    auto p = '{' << (kvp % ", ") << '}';
    return p.print(out, xs);
  }
};

// -- printer registry setup ---------------------------------------------------

template <>
struct printer_registry<view<data>> {
  using type = data_view_printer;
};

template <>
struct printer_registry<view<pattern>> {
  using type = pattern_view_printer;
};

template <>
struct printer_registry<view<vector>> {
  using type = vector_view_printer;
};

template <>
struct printer_registry<view<set>> {
  using type = set_view_printer;
};

template <>
struct printer_registry<view<map>> {
  using type = map_view_printer;
};

} // namespace vast
