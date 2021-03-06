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

#include <string>

#include "vast/concept/parseable/core/parser.hpp"

namespace vast {

class string_parser : public parser<string_parser> {
public:
  using attribute = std::string;

  string_parser(std::string str) : str_{std::move(str)} {
  }

  template <class Iterator>
  bool parse(Iterator& f, const Iterator& l, unused_type) const {
    auto i = f;
    auto begin = str_.begin();
    auto end = str_.end();
    while (begin != end)
      if (i == l || *i++ != *begin++)
        return false;
    f = i;
    return true;
  }

  template <class Iterator, class Attribute>
  bool parse(Iterator& f, const Iterator& l, Attribute& a) const {
    a.clear();
    auto out = std::back_inserter(a);
    auto i = f;
    auto begin = str_.begin();
    auto end = str_.end();
    while (begin != end)
      if (i == l || *i != *begin++)
        return false;
      else
        *out++ = *i++;
    f = i;
    return true;
  }

private:
  std::string str_;
};

namespace parsers {

using str = string_parser;

} // namespace parsers
} // namespace vast

