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

#include "vast/aliases.hpp"
#include "vast/data.hpp"
#include "vast/filesystem.hpp"
#include "vast/system/accountant.hpp"
#include "vast/system/archive.hpp"
#include "vast/system/consensus.hpp"
#include "vast/system/instrumentation.hpp"
#include "vast/system/type_registry.hpp"

#include <caf/event_based_actor.hpp>
#include <caf/stateful_actor.hpp>

#include <chrono>
#include <vector>

namespace vast::system {

/// Receives chunks from SOURCEs, imbues them with an ID, and relays them to
/// ARCHIVE, INDEX and continuous queries.
struct importer_state {
  // -- member types -----------------------------------------------------------

  /// Type of incoming stream elements.
  using input_type = table_slice_ptr;

  /// Type of outgoing stream elements.
  using output_type = table_slice_ptr;

  /// Stream object for managing downstream actors.
  using downstream_manager = caf::broadcast_downstream_manager<output_type>;

  /// Base type for stream drivers implementing the importer.
  using driver_base = caf::stream_stage_driver<input_type, downstream_manager>;

  /// A simple generator for IDs.
  struct id_generator {
    /// The next available ID.
    id i;

    /// The first unavailable ID.
    id last;

    id_generator(id from, id to) : i(from), last(to) {
      // nop
    }

    /// @returns whether this generator is exhausted.
    bool at_end() const noexcept {
      return i == last;
    }

    /// @returns the next ID and advances the position in the range.
    id next(size_t num = 1) noexcept {
      VAST_ASSERT(remaining() >= num);
      auto result = i;
      i += num;
      return result;
    }

    /// @returns how many more times `next` returns a valid ID.
    uint64_t remaining() const noexcept {
      return last - i;
    }
  };

  importer_state(caf::event_based_actor* self_ptr);

  ~importer_state();

  caf::error read_state();

  caf::error write_state();

  void send_report();

  /// Handle to the consensus module for obtaining more IDs.
  consensus_type consensus;

  /// Stores currently available IDs.
  std::vector<id_generator> id_generators;

  /// @returns the number of currently available IDs.
  int32_t available_ids() const noexcept;

  /// @returns the first ID for an ID block of size `max_table_slice_size`.
  /// @pre `available_ids() >= max_table_slice_size`
  id next_id_block();

  /// @returns various status metrics.
  caf::dictionary<caf::config_value> status() const;

  /// Forwards listeners to all INDEX actors and clears the listeners vector.
  void notify_flush_listeners();

  /// Stores how many slices inbound paths can still send us.
  int32_t in_flight_slices = 0;

  /// User-configured maximum for table slice sizes. This is the granularity
  /// for credit generation (each received slice consumes that many IDs).
  int32_t max_table_slice_size;

  /// Number of ID blocks we acquire per replenish, e.g., setting this to 10
  /// will acquire `max_table_slize * 10` IDs per replenish.
  size_t blocks_per_replenish = 100;

  /// Stores when we received new IDs for the last time.
  std::chrono::steady_clock::time_point last_replenish;

  /// State directory.
  path dir;

  /// Stores whether we've contacted the consensus module to obtain more IDs.
  bool awaiting_ids = false;

  /// The continous stage that moves data from all sources to all subscribers.
  caf::stream_stage_ptr<input_type, downstream_manager> stg;

  /// Pointer to the owning actor.
  caf::event_based_actor* self;

  /// List of actors that wait for the next flush event.
  std::vector<caf::actor> flush_listeners;

  measurement measurement_;
  stopwatch::time_point last_report;

  /// Stores all actor handles of connected INDEX actors.
  std::vector<caf::actor> index_actors;

  accountant_type accountant;

  /// Name of this actor in log events.
  static inline const char* name = "importer";
};

using importer_actor = caf::stateful_actor<importer_state>;

/// Spawns an IMPORTER.
/// @param self The actor handle.
/// @param dir The directory for persistent state.
/// @param max_table_slice_size The suggested maximum size for table slices.
/// @param archive A handle to the ARCHIVE.
/// @param consensus A handle to the consensus module.
/// @param index A handle to the INDEX.
/// @param batch_size The initial number of IDs to request when replenishing.
/// @param type_registry A handle to the type-registry module.
caf::behavior
importer(importer_actor* self, path dir, size_t max_table_slice_size,
         archive_type archive, consensus_type consensus, caf::actor index,
         type_registry_type type_registry);

} // namespace vast::system
