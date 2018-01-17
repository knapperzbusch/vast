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

#ifndef VAST_CONCEPT_PARSEABLE_CORE_GUARD_HPP
#define VAST_CONCEPT_PARSEABLE_CORE_GUARD_HPP

#include "vast/concept/parseable/core/parser.hpp"

namespace vast {

/// Attaches a guard expression to a parser that must succeed after the parser
/// executes.
/// @tparam Parser The parser to augment with a guard expression.
/// @tparam Guard A function that takes the synthesized attribute by
///               const-reference and returns `bool`.
template <typename Parser, typename Guard>
class guard_parser : public parser<guard_parser<Parser, Guard>> {
public:
  using attribute = typename Parser::attribute;

  guard_parser(Parser p, Guard fun) : parser_{std::move(p)}, guard_(fun) {
  }

  template <typename Iterator>
  bool parse(Iterator& f, Iterator const& l, unused_type) const {
    return parser_(f, l, unused);
  }

  template <typename Iterator, typename Attribute>
  bool parse(Iterator& f, Iterator const& l, Attribute& a) const {
    attribute attr;
    if (!(parser_(f, l, attr) && guard_(attr)))
      return false;
    a = std::move(attr);
    return true;
  }

private:
  Parser parser_;
  Guard guard_;
};

} // namespace vast

#endif
