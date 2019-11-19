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

#include <cmath>

#include "vast/base.hpp"
#include "vast/value_index.hpp"

namespace vast {

// -- value_index --------------------------------------------------------------

value_index::value_index(vast::type t) : type_{std::move(t)} {
  // nop
}

value_index::~value_index() {
  // nop
}

caf::expected<void> value_index::append(data_view x) {
  return append(x, offset());
}

caf::expected<void> value_index::append(data_view x, id pos) {
  auto off = offset();
  if (pos < off)
    // Can only append at the end
    return make_error(ec::unspecified, pos, '<', off);
  if (caf::holds_alternative<caf::none_t>(x)) {
    none_.append_bits(false, pos - none_.size());
    none_.append_bit(true);
    return caf::no_error;
  }
  // TODO: let append_impl return caf::error
  if (!append_impl(x, pos))
    return make_error(ec::unspecified, "append_impl");
  mask_.append_bits(false, pos - mask_.size());
  mask_.append_bit(true);
  return caf::no_error;
}

caf::expected<ids>
value_index::lookup(relational_operator op, data_view x) const {
  if (caf::holds_alternative<caf::none_t>(x)) {
    if (op == equal) {
      auto result = none_;
      if (result.size() < mask_.size())
        result.append_bits(false, mask_.size() - result.size());
      return result;
    }
    if (op == not_equal) {
      auto result = ~none_;
      if (result.size() < mask_.size())
        result.append_bits(true, mask_.size() - result.size());
      return result;
    }
    return make_error(ec::unsupported_operator, op);
  }
  auto result = lookup_impl(op, x);
  if (!result)
    return result;
  *result &= mask_;
  if (none_.size() > mask_.size())
    result->append_bits(false, none_.size() - mask_.size());
  return std::move(*result);
}

value_index::size_type value_index::offset() const {
  return std::max(none_.size(), mask_.size());
}

const type& value_index::type() const {
  return type_;
}

caf::error value_index::serialize(caf::serializer& sink) const {
  return sink(mask_, none_);
}

caf::error value_index::deserialize(caf::deserializer& source) {
  return source(mask_, none_);
}

const ewah_bitmap& value_index::mask() const {
  return mask_;
}

const ewah_bitmap& value_index::none() const {
  return none_;
}

caf::error inspect(caf::serializer& sink, const value_index& x) {
  return x.serialize(sink);
}

caf::error inspect(caf::deserializer& source, value_index& x) {
  return x.deserialize(source);
}

caf::error inspect(caf::serializer& sink, const value_index_ptr& x) {
  static auto nullptr_type = type{};
  if (x == nullptr)
    return sink(nullptr_type);
  return caf::error::eval([&] { return sink(x->type()); },
                          [&] { return x->serialize(sink); });
}

caf::error inspect(caf::deserializer& source, value_index_ptr& x) {
  type t;
  if (auto err = source(t))
    return err;
  if (caf::holds_alternative<none_type>(t)) {
    x = nullptr;
    return caf::none;
  }
  x = factory<value_index>::make(std::move(t));
  if (x == nullptr)
    return make_error(ec::unspecified, "failed to construct value index");
  return x->deserialize(source);
}

// -- string_index -------------------------------------------------------------

string_index::string_index(vast::type t, size_t max_length)
  : value_index{std::move(t)},
    max_length_{max_length},
    length_{base::uniform(10, std::log10(max_length) + !!(max_length % 10))} {
}

caf::error string_index::serialize(caf::serializer& sink) const {
  return caf::error::eval([&] { return value_index::serialize(sink); },
                          [&] { return sink(max_length_, length_, chars_); });
}

caf::error string_index::deserialize(caf::deserializer& source) {
  return caf::error::eval([&] { return value_index::deserialize(source); },
                          [&] { return source(max_length_, length_, chars_); });
}

bool string_index::append_impl(data_view x, id pos) {
  auto str = caf::get_if<view<std::string>>(&x);
  if (!str)
    return false;
  auto length = str->size();
  if (length > max_length_)
    length = max_length_;
  if (length > chars_.size())
    chars_.resize(length, char_bitmap_index{8});
  for (auto i = 0u; i < length; ++i) {
    chars_[i].skip(pos - chars_[i].size());
    chars_[i].append(static_cast<uint8_t>((*str)[i]));
  }
  length_.skip(pos - length_.size());
  length_.append(length);
  return true;
}

caf::expected<ids>
string_index::lookup_impl(relational_operator op, data_view x) const {
  return caf::visit(
    detail::overload(
      [&](auto x) -> caf::expected<ids> {
        return make_error(ec::type_clash, materialize(x));
      },
      [&](view<std::string> str) -> caf::expected<ids> {
        auto str_size = str.size();
        if (str_size > max_length_)
          str_size = max_length_;
        switch (op) {
          default:
            return make_error(ec::unsupported_operator, op);
          case equal:
          case not_equal: {
            if (str_size == 0) {
              auto result = length_.lookup(equal, 0);
              if (op == not_equal)
                result.flip();
              return result;
            }
            if (str_size > chars_.size())
              return ids{offset(), op == not_equal};
            auto result = length_.lookup(less_equal, str_size);
            if (all<0>(result))
              return ids{offset(), op == not_equal};
            for (auto i = 0u; i < str_size; ++i) {
              auto b = chars_[i].lookup(equal, static_cast<uint8_t>(str[i]));
              result &= b;
              if (all<0>(result))
                return ids{offset(), op == not_equal};
            }
            if (op == not_equal)
              result.flip();
            return result;
          }
          case ni:
          case not_ni: {
            if (str_size == 0)
              return ids{offset(), op == ni};
            if (str_size > chars_.size())
              return ids{offset(), op == not_ni};
            // TODO: Be more clever than iterating over all k-grams (#45).
            ids result{offset(), false};
            for (auto i = 0u; i < chars_.size() - str_size + 1; ++i) {
              ids substr{offset(), true};
              auto skip = false;
              for (auto j = 0u; j < str_size; ++j) {
                auto bm = chars_[i + j].lookup(equal, str[j]);
                if (all<0>(bm)) {
                  skip = true;
                  break;
                }
                substr &= bm;
              }
              if (!skip)
                result |= substr;
            }
            if (op == not_ni)
              result.flip();
            return result;
          }
        }
      },
      [&](view<vector> xs) { return detail::container_lookup(*this, op, xs); },
      [&](view<set> xs) { return detail::container_lookup(*this, op, xs); }),
    x);
}

// -- enumeration_index --------------------------------------------------------

enumeration_index::enumeration_index(vast::type t)
  : value_index{std::move(t)},
    index_{std::numeric_limits<enumeration>::max() + 1} {
  // nop
}

caf::error enumeration_index::serialize(caf::serializer& sink) const {
  return caf::error::eval([&] { return value_index::serialize(sink); },
                          [&] { return sink(index_); });
}

caf::error enumeration_index::deserialize(caf::deserializer& source) {
  return caf::error::eval([&] { return value_index::deserialize(source); },
                          [&] { return source(index_); });
}

bool enumeration_index::append_impl(data_view x, id pos) {
  if (auto e = caf::get_if<view<enumeration>>(&x)) {
    index_.skip(pos - index_.size());
    index_.append(*e);
    return true;
  }
  return false;
}

caf::expected<ids>
enumeration_index::lookup_impl(relational_operator op, data_view d) const {
  return caf::visit(
    detail::overload(
      [&](auto x) -> caf::expected<ids> {
        return make_error(ec::type_clash, materialize(x));
      },
      [&](view<enumeration> x) -> caf::expected<ids> {
        if (op == in || op == not_in)
          return make_error(ec::unsupported_operator, op);
        return index_.lookup(op, x);
      },
      [&](view<vector> xs) { return detail::container_lookup(*this, op, xs); },
      [&](view<set> xs) { return detail::container_lookup(*this, op, xs); }),
    d);
}

// -- address_index ------------------------------------------------------------

caf::error address_index::serialize(caf::serializer& sink) const {
  return caf::error::eval([&] { return value_index::serialize(sink); },
                          [&] { return sink(bytes_, v4_); });
}

caf::error address_index::deserialize(caf::deserializer& source) {
  return caf::error::eval([&] { return value_index::deserialize(source); },
                          [&] { return source(bytes_, v4_); });
}

address_index::address_index(vast::type t) : value_index{std::move(t)} {
  bytes_.fill(byte_index{8});
}

bool address_index::append_impl(data_view x, id pos) {
  auto addr = caf::get_if<view<address>>(&x);
  if (!addr)
    return false;
  auto& bytes = addr->data();
  for (auto i = 0u; i < 16; ++i) {
    bytes_[i].skip(pos - bytes_[i].size());
    bytes_[i].append(bytes[i]);
  }
  v4_.skip(pos - v4_.size());
  v4_.append(addr->is_v4());
  return true;
}

caf::expected<ids>
address_index::lookup_impl(relational_operator op, data_view d) const {
  return caf::visit(
    detail::overload(
      [&](auto x) -> caf::expected<ids> {
        return make_error(ec::type_clash, materialize(x));
      },
      [&](view<address> x) -> caf::expected<ids> {
        if (!(op == equal || op == not_equal))
          return make_error(ec::unsupported_operator, op);
        auto result = x.is_v4() ? v4_.coder().storage() : ids{offset(), true};
        for (auto i = x.is_v4() ? 12u : 0u; i < 16; ++i) {
          auto bm = bytes_[i].lookup(equal, x.data()[i]);
          result &= bm;
          if (all<0>(result))
            return ids{offset(), op == not_equal};
        }
        if (op == not_equal)
          result.flip();
        return result;
      },
      [&](view<subnet> x) -> caf::expected<ids> {
        if (!(op == in || op == not_in))
          return make_error(ec::unsupported_operator, op);
        auto topk = x.length();
        if (topk == 0)
          return make_error(ec::unspecified,
                            "invalid IP subnet length: ", topk);
        auto is_v4 = x.network().is_v4();
        if ((is_v4 ? topk + 96 : topk) == 128)
          // Asking for /32 or /128 membership is equivalent to an equality
          // lookup.
          return lookup_impl(op == in ? equal : not_equal, x.network());
        auto result = is_v4 ? v4_.coder().storage() : ids{offset(), true};
        auto& bytes = x.network().data();
        size_t i = is_v4 ? 12 : 0;
        for (; i < 16 && topk >= 8; ++i, topk -= 8)
          result &= bytes_[i].lookup(equal, bytes[i]);
        for (auto j = 0u; j < topk; ++j) {
          auto bit = 7 - j;
          auto& bm = bytes_[i].coder().storage()[bit];
          result &= (bytes[i] >> bit) & 1 ? ~bm : bm;
        }
        if (op == not_in)
          result.flip();
        return result;
      },
      [&](view<vector> xs) { return detail::container_lookup(*this, op, xs); },
      [&](view<set> xs) { return detail::container_lookup(*this, op, xs); }),
    d);
}

// -- subnet_index -------------------------------------------------------------

subnet_index::subnet_index(vast::type x)
  : value_index{std::move(x)}, network_{address_type{}}, length_{128 + 1} {
  // nop
}

caf::error subnet_index::serialize(caf::serializer& sink) const {
  return caf::error::eval([&] { return value_index::serialize(sink); },
                          [&] { return sink(network_, length_); });
}

caf::error subnet_index::deserialize(caf::deserializer& source) {
  return caf::error::eval([&] { return value_index::deserialize(source); },
                          [&] { return source(network_, length_); });
}

bool subnet_index::append_impl(data_view x, id pos) {
  if (auto sn = caf::get_if<view<subnet>>(&x)) {
    length_.skip(pos - length_.size());
    length_.append(sn->length());
    return static_cast<bool>(network_.append(sn->network(), pos));
  }
  return false;
}

caf::expected<ids>
subnet_index::lookup_impl(relational_operator op, data_view d) const {
  return caf::visit(
    detail::overload(
      [&](auto x) -> caf::expected<ids> {
        return make_error(ec::type_clash, materialize(x));
      },
      [&](view<address> x) -> caf::expected<ids> {
        if (!(op == ni || op == not_ni))
          return make_error(ec::unsupported_operator, op);
        auto result = ids{offset(), false};
        uint8_t bits = x.is_v4() ? 32 : 128;
        for (uint8_t i = 0; i <= bits; ++i) { // not an off-by-one
          auto masked = x;
          masked.mask(128 - bits + i);
          ids len = length_.lookup(equal, i);
          auto net = network_.lookup(equal, masked);
          if (!net)
            return net;
          len &= *net;
          result |= len;
        }
        if (op == not_ni)
          result.flip();
        return result;
      },
      [&](view<subnet> x) -> caf::expected<ids> {
        switch (op) {
          default:
            return make_error(ec::unsupported_operator, op);
          case equal:
          case not_equal: {
            auto result = network_.lookup(equal, x.network());
            if (!result)
              return result;
            auto n = length_.lookup(equal, x.length());
            *result &= n;
            if (op == not_equal)
              result->flip();
            return result;
          }
          case in:
          case not_in: {
            // For a subnet index U and subnet x, the in operator signifies a
            // subset relationship such that `U in x` translates to U ⊆ x, i.e.,
            // the lookup returns all subnets in U that are a subset of x.
            auto result = network_.lookup(in, x);
            if (!result)
              return result;
            *result &= length_.lookup(greater_equal, x.length());
            if (op == not_in)
              result->flip();
            return result;
          }
          case ni:
          case not_ni: {
            // For a subnet index U and subnet x, the ni operator signifies a
            // subset relationship such that `U ni x` translates to U ⊇ x, i.e.,
            // the lookup returns all subnets in U that include x.
            ids result;
            for (auto i = uint8_t{1}; i <= x.length(); ++i) {
              auto xs = network_.lookup(in, subnet{x.network(), i});
              if (!xs)
                return xs;
              *xs &= length_.lookup(equal, i);
              result |= *xs;
            }
            if (op == not_ni)
              result.flip();
            return result;
          }
        }
      },
      [&](view<vector> xs) { return detail::container_lookup(*this, op, xs); },
      [&](view<set> xs) { return detail::container_lookup(*this, op, xs); }),
    d);
}

// -- port_index ---------------------------------------------------------------

port_index::port_index(vast::type t)
  : value_index{std::move(t)},
    num_{base::uniform(10, 5)}, // [0, 2^16)
    proto_{256}                 // 8-bit proto/next-header field
{
  // nop
}

caf::error port_index::serialize(caf::serializer& sink) const {
  return caf::error::eval([&] { return value_index::serialize(sink); },
                          [&] { return sink(num_, proto_); });
}

caf::error port_index::deserialize(caf::deserializer& source) {
  return caf::error::eval([&] { return value_index::deserialize(source); },
                          [&] { return source(num_, proto_); });
}

bool port_index::append_impl(data_view x, id pos) {
  if (auto p = caf::get_if<view<port>>(&x)) {
    num_.skip(pos - num_.size());
    num_.append(p->number());
    proto_.skip(pos - proto_.size());
    proto_.append(p->type());
    return true;
  }
  return false;
}

caf::expected<ids>
port_index::lookup_impl(relational_operator op, data_view d) const {
  return caf::visit(
    detail::overload(
      [&](auto x) -> caf::expected<ids> {
        return make_error(ec::type_clash, materialize(x));
      },
      [&](view<port> x) -> caf::expected<ids> {
        if (op == in || op == not_in)
          return make_error(ec::unsupported_operator, op);
        auto n = num_.lookup(op, x.number());
        if (all<0>(n))
          return ids{offset(), false};
        if (x.type() != port::unknown)
          n &= proto_.lookup(equal, x.type());
        return n;
      },
      [&](view<vector> xs) { return detail::container_lookup(*this, op, xs); },
      [&](view<set> xs) { return detail::container_lookup(*this, op, xs); }),
    d);
}

// -- sequence_index -----------------------------------------------------------

sequence_index::sequence_index(vast::type t, size_t max_size)
  : value_index{std::move(t)},
    max_size_{max_size},
    size_{base::uniform(10, std::log10(max_size) + !!(max_size % 10))} {
  auto f = [](const auto& x) -> vast::type {
    using concrete_type = std::decay_t<decltype(x)>;
    if constexpr (detail::is_any_v<concrete_type, vector_type, set_type>)
      return x.value_type;
    else
      return none_type{};
  };
  value_type_ = caf::visit(f, value_index::type());
  VAST_ASSERT(!caf::holds_alternative<none_type>(value_type_));
  size_t components = std::log10(max_size_);
  if (max_size_ % 10 != 0)
    ++components;
  size_ = size_bitmap_index{base::uniform(10, components)};
}

caf::error sequence_index::serialize(caf::serializer& sink) const {
  return caf::error::eval([&] { return value_index::serialize(sink); },
                          [&] {
                            return sink(elements_, size_, max_size_,
                                        value_type_);
                          });
}

caf::error sequence_index::deserialize(caf::deserializer& source) {
  return caf::error::eval(
    [&] { return value_index::deserialize(source); },
    [&] { return source(elements_, size_, max_size_, value_type_); }
  );
}

bool sequence_index::append_impl(data_view x, id pos) {
  auto f = [&](const auto& v) {
    using view_type = std::decay_t<decltype(v)>;
    if constexpr (detail::is_any_v<view_type, view<vector>, view<set>>) {
      auto seq_size = std::min(v->size(), max_size_);
      if (seq_size > elements_.size()) {
        auto old = elements_.size();
        elements_.resize(seq_size);
        for (auto i = old; i < elements_.size(); ++i) {
          elements_[i] = factory<value_index>::make(value_type_);
          VAST_ASSERT(elements_[i]);
        }
      }
      auto x = v->begin();
      for (auto i = 0u; i < seq_size; ++i)
        elements_[i]->append(*x++, pos);
      size_.skip(pos - size_.size());
      size_.append(seq_size);
      return true;
    }
    return false;
  };
  return caf::visit(f, x);
}

caf::expected<ids>
sequence_index::lookup_impl(relational_operator op, data_view x) const {
  if (!(op == ni || op == not_ni))
    return make_error(ec::unsupported_operator, op);
  if (elements_.empty())
    return ids{};
  auto result = elements_[0]->lookup(equal, x);
  if (!result)
    return result;
  for (auto i = 1u; i < elements_.size(); ++i) {
    auto mbm = elements_[i]->lookup(equal, x);
    if (mbm)
      *result |= *mbm;
    else
      return mbm;
  }
  if (op == not_ni)
    result->flip();
  return result;
}

} // namespace vast
