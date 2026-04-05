#include "core/order/order.h"

#include <gtest/gtest.h>

#include "core/order/order_event.h"

namespace oems::order {
namespace {

TEST(OrderTest, DefaultConstructed) {
  Order o;
  EXPECT_EQ(o.internal_id, 0U);
  EXPECT_EQ(o.status, OrderStatus::kPendingNew);
  EXPECT_EQ(o.filled_qty, 0);
}

TEST(OrderTest, DesignatedInit) {
  Order o{
      .internal_id = 42,
      .client_order_id = "ABC",
      .symbol = Symbol{"AAPL"},
      .side = Side::kBuy,
      .type = OrderType::kLimit,
      .price = 10000,
      .order_qty = 100,
      .remaining_qty = 100,
      .status = OrderStatus::kAccepted,
  };
  EXPECT_EQ(o.internal_id, 42U);
  EXPECT_EQ(o.client_order_id, "ABC");
  EXPECT_EQ(o.status, OrderStatus::kAccepted);
}

TEST(OrderEventTest, EventTypeNames) {
  EXPECT_STREQ(EventTypeName(EventType::kNewRequested), "NewRequested");
  EXPECT_STREQ(EventTypeName(EventType::kAccepted), "Accepted");
  EXPECT_STREQ(EventTypeName(EventType::kRejected), "Rejected");
  EXPECT_STREQ(EventTypeName(EventType::kPartialFill), "PartialFill");
  EXPECT_STREQ(EventTypeName(EventType::kFill), "Fill");
  EXPECT_STREQ(EventTypeName(EventType::kCancelRequested), "CancelRequested");
  EXPECT_STREQ(EventTypeName(EventType::kCancelled), "Cancelled");
}

}  // namespace
}  // namespace oems::order
