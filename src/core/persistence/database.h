#ifndef OEMS_CORE_PERSISTENCE_DATABASE_H_
#define OEMS_CORE_PERSISTENCE_DATABASE_H_

/**
 * @file database.h
 * @brief SQLite-backed durable state for Mini OEMS.
 *
 * Persists orders, order events, executions, an audit log, and recovery
 * metadata in the five tables specified by the v1 architecture.
 */

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/matching/order_book.h"
#include "core/order/order.h"
#include "core/order/order_event.h"
#include "core/types/error.h"
#include "core/types/types.h"

struct sqlite3;

namespace oems::persistence {

/**
 * @brief Thin RAII wrapper around a SQLite handle.
 *
 * Non-copyable, movable.  All operations return @c Result<T>; no exceptions
 * are thrown.
 */
class Database {
 public:
  /**
   * @brief Open or create a database at the given path.
   * @param path File path, or ":memory:" for an in-memory DB.
   */
  static auto Open(std::string_view path) -> Result<Database>;

  /**
   * @brief Create all tables if they do not yet exist (idempotent).
   */
  auto Migrate() -> Result<void>;

  // --- Orders ---
  auto SaveOrder(const order::Order& order) -> Result<void>;
  auto LoadOrders() -> Result<std::vector<order::Order>>;
  auto LoadOrder(OrderId id) -> Result<order::Order>;

  // --- Events (append-only) ---
  auto AppendEvent(const order::OrderEvent& event) -> Result<void>;
  auto LoadEvents(OrderId id) -> Result<std::vector<order::OrderEvent>>;
  auto LoadAllEvents() -> Result<std::vector<order::OrderEvent>>;

  // --- Executions ---
  auto SaveExecution(const matching::Fill& fill) -> Result<void>;
  auto LoadExecutions() -> Result<std::vector<matching::Fill>>;

  // --- Audit log ---
  auto AppendAudit(std::string_view category, std::string_view message) -> Result<void>;

  // --- Service state (key-value) ---
  auto SaveServiceState(std::string_view key, std::string_view value) -> Result<void>;
  auto LoadServiceState(std::string_view key) -> Result<std::optional<std::string>>;

  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&& other) noexcept;
  Database& operator=(Database&& other) noexcept;

 private:
  explicit Database(sqlite3* db) : db_(db) {}
  auto Exec(std::string_view sql) -> Result<void>;

  sqlite3* db_;
};

}  // namespace oems::persistence

#endif  // OEMS_CORE_PERSISTENCE_DATABASE_H_
