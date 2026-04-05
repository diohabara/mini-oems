#include "core/fix/fix_message.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <utility>

namespace oems::fix {

auto FixMessage::ComputeCheckSum(std::string_view buf) -> unsigned {
  unsigned sum = 0;
  for (std::uint8_t c : buf) {
    sum += c;
  }
  return sum % 256;
}

auto FixMessage::Parse(std::string_view raw) -> Result<FixMessage> {
  FixMessage msg;
  std::size_t pos = 0;
  while (pos < raw.size()) {
    auto eq = raw.find('=', pos);
    if (eq == std::string_view::npos) {
      return std::unexpected(OemsError::kFixParseError);
    }
    auto soh = raw.find(kSoh, eq + 1);
    if (soh == std::string_view::npos) {
      return std::unexpected(OemsError::kFixParseError);
    }
    std::int32_t tag_num = 0;
    auto tag_view = raw.substr(pos, eq - pos);
    auto [ptr, ec] = std::from_chars(tag_view.data(), tag_view.data() + tag_view.size(), tag_num);
    if (ec != std::errc{} || tag_num <= 0) {
      return std::unexpected(OemsError::kFixParseError);
    }
    std::string value(raw.substr(eq + 1, soh - eq - 1));
    msg.fields_.emplace_back(tag_num, std::move(value));
    pos = soh + 1;
  }
  return msg;
}

auto FixMessage::ParseStrict(std::string_view raw) -> Result<FixMessage> {
  auto parsed = Parse(raw);
  if (!parsed.has_value()) {
    return parsed;
  }
  // Verify checksum: last field should be 10=XXX (3 digits).
  if (parsed->fields_.empty()) {
    return std::unexpected(OemsError::kFixParseError);
  }
  const auto& last = parsed->fields_.back();
  if (last.first != tag::kCheckSum) {
    return std::unexpected(OemsError::kFixParseError);
  }
  // CheckSum covers bytes up to (but not including) the "10=" field.
  auto checksum_pos = raw.rfind(
      "\x01"
      "10=");
  if (checksum_pos == std::string_view::npos) {
    checksum_pos = raw.find("10=");
    if (checksum_pos == std::string_view::npos) {
      return std::unexpected(OemsError::kFixParseError);
    }
  } else {
    ++checksum_pos;  // skip the SOH, include it in the checksum
  }
  unsigned computed = ComputeCheckSum(raw.substr(0, checksum_pos));
  std::int32_t claimed = 0;
  auto [ptr, ec] =
      std::from_chars(last.second.data(), last.second.data() + last.second.size(), claimed);
  if (ec != std::errc{} || std::cmp_not_equal(claimed, computed)) {
    return std::unexpected(OemsError::kFixParseError);
  }
  return parsed;
}

auto FixMessage::Get(std::int32_t tag) const -> std::optional<std::string_view> {
  for (const auto& [t, v] : fields_) {
    if (t == tag) {
      return v;
    }
  }
  return std::nullopt;
}

auto FixMessage::GetInt(std::int32_t tag) const -> std::int64_t {
  auto v = Get(tag);
  if (!v.has_value()) {
    return 0;
  }
  std::int64_t out = 0;
  std::from_chars(v->data(), v->data() + v->size(), out);
  return out;
}

void FixMessage::Set(std::int32_t tag, std::string value) {
  for (auto& [t, v] : fields_) {
    if (t == tag) {
      v = std::move(value);
      return;
    }
  }
  fields_.emplace_back(tag, std::move(value));
}

void FixMessage::Set(std::int32_t tag, std::int64_t value) { Set(tag, std::to_string(value)); }

namespace {
auto FindField(const std::vector<std::pair<std::int32_t, std::string>>& fields, std::int32_t tag)
    -> const std::string* {
  for (const auto& [t, v] : fields) {
    if (t == tag) {
      return &v;
    }
  }
  return nullptr;
}
}  // namespace

auto FixMessage::Serialize() const -> std::string {
  // Header must be 8=, 9=, 35= in that order.
  const std::string* begin_string = FindField(fields_, tag::kBeginString);
  const std::string* msg_type_val = FindField(fields_, tag::kMsgType);
  std::string begin = begin_string != nullptr ? *begin_string : std::string("FIX.4.4");
  std::string mtype = msg_type_val != nullptr ? *msg_type_val : std::string("0");

  // Build body (after "9=N" up to but not including "10=").
  std::string body;
  body.reserve(128);
  body.append("35=").append(mtype).push_back(kSoh);
  for (const auto& [t, v] : fields_) {
    if (t == tag::kBeginString || t == tag::kBodyLength || t == tag::kMsgType ||
        t == tag::kCheckSum) {
      continue;
    }
    body.append(std::to_string(t)).push_back('=');
    body.append(v).push_back(kSoh);
  }

  std::string prefix;
  prefix.append("8=").append(begin).push_back(kSoh);
  prefix.append("9=").append(std::to_string(body.size())).push_back(kSoh);

  std::string without_checksum = prefix + body;
  unsigned cs = ComputeCheckSum(without_checksum);
  std::string trailer = std::format("10={:03d}{}", cs, kSoh);
  return without_checksum + trailer;
}

}  // namespace oems::fix
