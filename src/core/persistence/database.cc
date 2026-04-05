#include "core/persistence/database.h"

#include <chrono>
#include <string>
#include <utility>

#include "sqlite3.h"

namespace oems::persistence {

namespace {

/// @brief Convert a chrono timestamp to nanoseconds-since-epoch.
auto ToNanos(Timestamp ts) -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(ts.time_since_epoch()).count();
}

auto FromNanos(std::int64_t nanos) -> Timestamp {
  return Timestamp(std::chrono::nanoseconds(nanos));
}

constexpr const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS orders (
  internal_id     INTEGER PRIMARY KEY,
  client_order_id TEXT    NOT NULL,
  symbol          TEXT    NOT NULL,
  side            INTEGER NOT NULL,
  type            INTEGER NOT NULL,
  price           INTEGER NOT NULL,
  order_qty       INTEGER NOT NULL,
  filled_qty      INTEGER NOT NULL,
  remaining_qty   INTEGER NOT NULL,
  status          INTEGER NOT NULL,
  created_at_ns   INTEGER NOT NULL,
  updated_at_ns   INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS order_events (
  event_id        INTEGER PRIMARY KEY,
  order_id        INTEGER NOT NULL,
  type            INTEGER NOT NULL,
  timestamp_ns    INTEGER NOT NULL,
  detail          TEXT    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_events_order_id ON order_events(order_id);

CREATE TABLE IF NOT EXISTS executions (
  execution_id          INTEGER PRIMARY KEY,
  symbol                TEXT    NOT NULL,
  aggressive_order_id   INTEGER NOT NULL,
  passive_order_id      INTEGER NOT NULL,
  aggressive_side       INTEGER NOT NULL,
  price                 INTEGER NOT NULL,
  quantity              INTEGER NOT NULL,
  timestamp_ns          INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS audit_log (
  id              INTEGER PRIMARY KEY AUTOINCREMENT,
  category        TEXT    NOT NULL,
  message         TEXT    NOT NULL,
  timestamp_ns    INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS service_state (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
);
)SQL";

}  // namespace

auto Database::Open(std::string_view path) -> Result<Database> {
  sqlite3* db = nullptr;
  std::string path_str(path);
  std::int32_t rc = sqlite3_open(path_str.c_str(), &db);
  if (rc != SQLITE_OK) {
    if (db != nullptr) {
      sqlite3_close(db);
    }
    return std::unexpected(OemsError::kDatabaseError);
  }
  // Enable WAL and foreign keys (best practice).
  sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
  return Database(db);
}

Database::~Database() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

Database::Database(Database&& other) noexcept : db_(other.db_) { other.db_ = nullptr; }

Database& Database::operator=(Database&& other) noexcept {
  if (this != &other) {
    if (db_ != nullptr) {
      sqlite3_close(db_);
    }
    db_ = other.db_;
    other.db_ = nullptr;
  }
  return *this;
}

auto Database::Exec(std::string_view sql) -> Result<void> {
  char* err = nullptr;
  std::string sql_str(sql);
  std::int32_t rc = sqlite3_exec(db_, sql_str.c_str(), nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    if (err != nullptr) {
      sqlite3_free(err);
    }
    return std::unexpected(OemsError::kDatabaseError);
  }
  return {};
}

auto Database::Migrate() -> Result<void> { return Exec(kSchema); }

auto Database::SaveOrder(const order::Order& order) -> Result<void> {
  const char* sql =
      "INSERT OR REPLACE INTO orders (internal_id, client_order_id, symbol, side, type, "
      "price, order_qty, filled_qty, remaining_qty, status, created_at_ns, updated_at_ns) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(order.internal_id));
  sqlite3_bind_text(stmt, 2, order.client_order_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, order.symbol.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, static_cast<std::int32_t>(order.side));
  sqlite3_bind_int(stmt, 5, static_cast<std::int32_t>(order.type));
  sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(order.price));
  sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(order.order_qty));
  sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(order.filled_qty));
  sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(order.remaining_qty));
  sqlite3_bind_int(stmt, 10, static_cast<std::int32_t>(order.status));
  sqlite3_bind_int64(stmt, 11, ToNanos(order.created_at));
  sqlite3_bind_int64(stmt, 12, ToNanos(order.updated_at));

  std::int32_t rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  return {};
}

namespace {

auto RowToOrder(sqlite3_stmt* stmt) -> order::Order {
  order::Order o;
  o.internal_id = static_cast<OrderId>(sqlite3_column_int64(stmt, 0));
  const auto* cid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  o.client_order_id = cid != nullptr ? cid : "";
  const auto* sym = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  o.symbol = Symbol{sym != nullptr ? sym : ""};
  o.side = static_cast<Side>(sqlite3_column_int(stmt, 3));
  o.type = static_cast<OrderType>(sqlite3_column_int(stmt, 4));
  o.price = static_cast<Price>(sqlite3_column_int64(stmt, 5));
  o.order_qty = static_cast<Quantity>(sqlite3_column_int64(stmt, 6));
  o.filled_qty = static_cast<Quantity>(sqlite3_column_int64(stmt, 7));
  o.remaining_qty = static_cast<Quantity>(sqlite3_column_int64(stmt, 8));
  o.status = static_cast<OrderStatus>(sqlite3_column_int(stmt, 9));
  o.created_at = FromNanos(sqlite3_column_int64(stmt, 10));
  o.updated_at = FromNanos(sqlite3_column_int64(stmt, 11));
  return o;
}

}  // namespace

auto Database::LoadOrders() -> Result<std::vector<order::Order>> {
  const char* sql =
      "SELECT internal_id, client_order_id, symbol, side, type, price, order_qty, "
      "filled_qty, remaining_qty, status, created_at_ns, updated_at_ns FROM orders "
      "ORDER BY internal_id";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  std::vector<order::Order> out;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.push_back(RowToOrder(stmt));
  }
  sqlite3_finalize(stmt);
  return out;
}

auto Database::LoadOrder(OrderId id) -> Result<order::Order> {
  const char* sql =
      "SELECT internal_id, client_order_id, symbol, side, type, price, order_qty, "
      "filled_qty, remaining_qty, status, created_at_ns, updated_at_ns FROM orders "
      "WHERE internal_id=?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(id));
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::unexpected(OemsError::kOrderNotFound);
  }
  auto order = RowToOrder(stmt);
  sqlite3_finalize(stmt);
  return order;
}

auto Database::AppendEvent(const order::OrderEvent& event) -> Result<void> {
  const char* sql =
      "INSERT INTO order_events (event_id, order_id, type, timestamp_ns, detail) "
      "VALUES (?,?,?,?,?)";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(event.event_id));
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(event.order_id));
  sqlite3_bind_int(stmt, 3, static_cast<std::int32_t>(event.type));
  sqlite3_bind_int64(stmt, 4, ToNanos(event.timestamp));
  sqlite3_bind_text(stmt, 5, event.detail.c_str(), -1, SQLITE_TRANSIENT);
  std::int32_t rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  return {};
}

namespace {

auto RowToEvent(sqlite3_stmt* stmt) -> order::OrderEvent {
  order::OrderEvent ev;
  ev.event_id = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  ev.order_id = static_cast<OrderId>(sqlite3_column_int64(stmt, 1));
  ev.type = static_cast<order::EventType>(sqlite3_column_int(stmt, 2));
  ev.timestamp = FromNanos(sqlite3_column_int64(stmt, 3));
  const auto* detail = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
  ev.detail = detail != nullptr ? detail : "";
  return ev;
}

}  // namespace

auto Database::LoadEvents(OrderId id) -> Result<std::vector<order::OrderEvent>> {
  const char* sql =
      "SELECT event_id, order_id, type, timestamp_ns, detail FROM order_events "
      "WHERE order_id=? ORDER BY event_id";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(id));
  std::vector<order::OrderEvent> out;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.push_back(RowToEvent(stmt));
  }
  sqlite3_finalize(stmt);
  return out;
}

auto Database::LoadAllEvents() -> Result<std::vector<order::OrderEvent>> {
  const char* sql =
      "SELECT event_id, order_id, type, timestamp_ns, detail FROM order_events "
      "ORDER BY event_id";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  std::vector<order::OrderEvent> out;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.push_back(RowToEvent(stmt));
  }
  sqlite3_finalize(stmt);
  return out;
}

auto Database::SaveExecution(const matching::Fill& fill) -> Result<void> {
  const char* sql =
      "INSERT OR REPLACE INTO executions (execution_id, symbol, aggressive_order_id, "
      "passive_order_id, aggressive_side, price, quantity, timestamp_ns) "
      "VALUES (?,?,?,?,?,?,?,?)";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(fill.execution_id));
  sqlite3_bind_text(stmt, 2, fill.symbol.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(fill.aggressive_order_id));
  sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(fill.passive_order_id));
  sqlite3_bind_int(stmt, 5, static_cast<std::int32_t>(fill.aggressive_side));
  sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(fill.price));
  sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(fill.quantity));
  sqlite3_bind_int64(stmt, 8, ToNanos(fill.timestamp));
  std::int32_t rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  return {};
}

auto Database::LoadExecutions() -> Result<std::vector<matching::Fill>> {
  const char* sql =
      "SELECT execution_id, symbol, aggressive_order_id, passive_order_id, "
      "aggressive_side, price, quantity, timestamp_ns FROM executions "
      "ORDER BY execution_id";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  std::vector<matching::Fill> out;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    matching::Fill f;
    f.execution_id = static_cast<ExecutionId>(sqlite3_column_int64(stmt, 0));
    const auto* sym = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    f.symbol = Symbol{sym != nullptr ? sym : ""};
    f.aggressive_order_id = static_cast<OrderId>(sqlite3_column_int64(stmt, 2));
    f.passive_order_id = static_cast<OrderId>(sqlite3_column_int64(stmt, 3));
    f.aggressive_side = static_cast<Side>(sqlite3_column_int(stmt, 4));
    f.price = static_cast<Price>(sqlite3_column_int64(stmt, 5));
    f.quantity = static_cast<Quantity>(sqlite3_column_int64(stmt, 6));
    f.timestamp = FromNanos(sqlite3_column_int64(stmt, 7));
    out.push_back(f);
  }
  sqlite3_finalize(stmt);
  return out;
}

auto Database::AppendAudit(std::string_view category, std::string_view message) -> Result<void> {
  const char* sql = "INSERT INTO audit_log (category, message, timestamp_ns) VALUES (?,?,?)";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  std::string cat(category);
  std::string msg(message);
  sqlite3_bind_text(stmt, 1, cat.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, msg.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, ToNanos(Now()));
  std::int32_t rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  return {};
}

auto Database::SaveServiceState(std::string_view key, std::string_view value) -> Result<void> {
  const char* sql = "INSERT OR REPLACE INTO service_state (key, value) VALUES (?,?)";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  std::string key_s(key);
  std::string val_s(value);
  sqlite3_bind_text(stmt, 1, key_s.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, val_s.c_str(), -1, SQLITE_TRANSIENT);
  std::int32_t rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  return {};
}

auto Database::LoadServiceState(std::string_view key) -> Result<std::optional<std::string>> {
  const char* sql = "SELECT value FROM service_state WHERE key=?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(OemsError::kDatabaseError);
  }
  std::string key_s(key);
  sqlite3_bind_text(stmt, 1, key_s.c_str(), -1, SQLITE_TRANSIENT);
  std::int32_t rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    const auto* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    std::string out = val != nullptr ? val : "";
    sqlite3_finalize(stmt);
    return std::optional<std::string>{out};
  }
  sqlite3_finalize(stmt);
  if (rc == SQLITE_DONE) {
    return std::optional<std::string>{std::nullopt};
  }
  return std::unexpected(OemsError::kDatabaseError);
}

}  // namespace oems::persistence
