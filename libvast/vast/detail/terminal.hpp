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

#ifndef VAST_DETAIL_TERMINAL_HPP
#define VAST_DETAIL_TERMINAL_HPP

namespace vast {
namespace detail {

/// This namesapce hosts various functions related to controling the terminal.
/// @warn All functions in this namesapce are not thread-safe.
namespace terminal {

/// RAII helper for scope-wise terminal unbuffering.
struct unbufferer {
  unbufferer();
  ~unbufferer();
};

/// Makes the stdin unbuffered.
/// @returns `true` on success.
bool unbuffer();

/// Makes the stdin buffered again after a call to ::unbuffer.
/// @returns `true` on success.
bool buffer();

/// Disables printing of characters to the terminal.
/// @returns `true` on success.
bool disable_echo();

/// Enables printing of characters to the terminal.
/// @returns `true` on success.
bool enable_echo();

/// Tries to extract a single a character from STDIN within a short time
/// interval. The function blocks until either input arrives or the timeout
/// expires. Moreover, the function sets the terminal to unbuffered before
/// attempting to read a character and sets it back to buffered upon returning.
///
/// @param c The character from STDIN.
///
/// @param timeout The maximum number of milliseconds to block.
///
/// @returns `true` *iff* extracting a character from STDIN was successful
/// within *timeout* milliseconds.
///
/// @post Iff the function returned `true`, *c* contains the next character
/// from STDIN.
bool get(char& c, int timeout = 100);

} // namespace terminal
} // namespace detail
} // namespace vast

#endif
