#ifndef VAST_CONSOLE_H
#define VAST_CONSOLE_H

#include <deque>
#include "vast/actor.h"
#include "vast/cow.h"
#include "vast/expression.h"
#include "vast/individual.h"
#include "vast/util/command_line.h"

namespace vast {

/// A console-based, interactive query client.
struct console : actor<console>
{
  struct options
  {
    uint64_t paginate = 10;
    bool auto_follow = true;
  };

  /// A stream of events representing a result. One can add events to the
  /// result and seek forward/backward.
  class result : public individual
  {
  public:
    /// Constructs a result from a valid AST.
    result(expr::ast ast);

    /// Adds a new event to this result.
    /// @param e The event to add.
    void add(cow<event> e);

    /// Applies a function to a given number of existing events and advances
    /// the internal position.
    ///
    /// @param n The number of events to apply *f* to.
    ///
    /// @param f The function to apply to the *n* next events.
    ///
    /// @returns The number of events that *f* has been applied to.
    size_t apply(size_t n, std::function<void(event const&)> f);

    /// Applies a function to a given number of new events and removes the
    /// affected events from the list of new events.
    ///
    /// @param n The number of events to apply *f* to.
    ///
    /// @param f The function to apply to the *n* next events.
    ///
    /// @returns The number of inspected events.
    size_t absorb(size_t n, std::function<void(event const&)> f = {});

    /// Adjusts the stream position.
    /// @param n The number of events.
    /// @returns The number of events seeked.
    size_t seek_forward(size_t n);

    /// Adjusts the stream position.
    /// @param n The number of events.
    /// @returns The number of events seeked.
    size_t seek_backward(size_t n);

    /// Retrieves the AST of this query result.
    expr::ast const& ast() const;

    /// Retrieves the number of events in the result.
    /// @returns The number of all events.
    size_t size() const;

    /// Retrieves the number of new events in the result.
    /// @returns The number of new events.
    size_t size_new() const;

  private:
    using pos_type = uint64_t;

    pos_type pos_ = 0;
    std::deque<cow<event>> events_;
    std::deque<cow<event>> new_;
    expr::ast ast_;
  };

  /// Spawns the console client.
  /// @param search The search actor the console interacts with.
  console(cppa::actor_ptr search);

  void act();
  char const* description() const;

  void show_prompt(size_t ms = 100);
  result* to_result(std::string const& str);

  std::map<cppa::actor_ptr, result> results_;
  result* current_result_;
  cppa::actor_ptr search_;
  util::command_line cmdline_;
  options opts_;
  bool follow_mode_ = false;
};

} // namespace vast

#endif
