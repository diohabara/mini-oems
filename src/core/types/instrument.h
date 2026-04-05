#ifndef OEMS_CORE_TYPES_INSTRUMENT_H_
#define OEMS_CORE_TYPES_INSTRUMENT_H_

/**
 * @file instrument.h
 * @brief Per-symbol configuration used by TSE-specific risk validations.
 *
 * Holds the reference data the Risk Manager needs to reject orders that
 * violate exchange rules:
 *   - lot size (売買単位): TSE standard is 100 shares per lot.
 *   - tick size bands (呼値の刻み): TSE uses price-dependent tick sizes,
 *     e.g. 1 JPY tick under 3,000 JPY, 5 JPY tick in 3,000-5,000 JPY, ...
 *   - daily price limit (値幅制限): daily stop-high/stop-low anchored on
 *     the previous day's closing price.
 *
 * Each field has a sentinel value that means "disabled / not configured"
 * so partial configuration is possible (e.g. set only lot_size during
 * development) without breaking existing behaviour.
 */

#include <cstdint>
#include <vector>

#include "core/types/types.h"

namespace oems {

/**
 * @brief A single price band with its allowed tick size.
 *
 * A price p is valid inside this band iff @c low <= p <= high and
 * @c p % tick == 0.
 *
 * Bands are conceptually contiguous; callers should supply them sorted
 * by low price with no gaps.  The top band uses @c std::numeric_limits<Price>::max()
 * (or any sufficiently large value) as its high sentinel.
 */
struct TickBand {
  Price low{0};
  Price high{0};
  Price tick{0};
};

/**
 * @brief Per-symbol reference data used by pre-trade risk checks.
 *
 * All exchange rules are opt-in: a zero/empty field disables the
 * associated check so unconfigured symbols fall through unchanged.
 */
struct SymbolConfig {
  Symbol symbol{};
  /// @brief Required lot size. 0 disables the check; TSE default is 100.
  Quantity lot_size{0};
  /// @brief Previous-day closing price, used as the anchor for daily
  /// price-limit checks. 0 disables the check.
  Price previous_close{0};
  /// @brief Daily limit in basis points (e.g. 1000 = ±10%).  v1 uses a
  /// single flat bps as a simplification; the real TSE daily-limit table
  /// is a stepped function of the previous close and will replace this
  /// field in a future iteration. 0 disables the check.
  std::int32_t daily_limit_bps{0};
  /// @brief Tick bands, ordered by @c low ascending. Empty disables
  /// the tick-size check.
  std::vector<TickBand> tick_bands{};
};

}  // namespace oems

#endif  // OEMS_CORE_TYPES_INSTRUMENT_H_
