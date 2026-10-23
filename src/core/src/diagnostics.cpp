/**
 * @file diagnostics.cpp
 * @brief Implementation of the structured diagnostics collector.
 */
#include <srsp/core/diagnostics.hpp>

#include <algorithm>
#include <utility>

namespace srsp::core {
namespace {

const char* severity_name(Severity severity) noexcept {
  switch (severity) {
    case Severity::kTrace:
      return "trace";
    case Severity::kDebug:
      return "debug";
    case Severity::kInfo:
      return "info";
    case Severity::kWarn:
      return "warn";
    case Severity::kError:
      return "error";
    case Severity::kCritical:
      return "critical";
  }
  return "info";
}

void append_escaped(std::string& out, const std::string& value) {
  for (const char c : value) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
}

}  // namespace

Diagnostics::Diagnostics(Sink sink) : sink_(std::move(sink)) {}

void Diagnostics::set_history_capacity(std::size_t capacity) noexcept {
  capacity_ = capacity;
  if (records_.size() > capacity_) {
    const auto excess = records_.size() - capacity_;
    records_.erase(records_.begin(),
                   records_.begin() + static_cast<std::ptrdiff_t>(excess));
  }
}

void Diagnostics::emit(Severity severity, std::string code, std::string message,
                       std::string context_json) {
  Diagnostic record;
  record.severity = severity;
  record.code = std::move(code);
  record.message = std::move(message);
  record.context_json = std::move(context_json);

  if (capacity_ > 0) {
    records_.push_back(record);
    if (records_.size() > capacity_) {
      records_.erase(records_.begin());
    }
  }
  if (sink_) {
    sink_(record);
  }
}

std::size_t Diagnostics::count(Severity severity) const noexcept {
  return static_cast<std::size_t>(
      std::count_if(records_.begin(), records_.end(),
                    [severity](const Diagnostic& d) { return d.severity == severity; }));
}

std::string Diagnostics::to_json_line(const Diagnostic& record) {
  std::string out;
  out.reserve(128 + record.message.size() + record.context_json.size());
  out += "{\"severity\":\"";
  out += severity_name(record.severity);
  out += "\",\"code\":\"";
  append_escaped(out, record.code);
  out += "\",\"message\":\"";
  append_escaped(out, record.message);
  out += "\"";
  if (!record.context_json.empty()) {
    out += ",\"context\":";
    out += record.context_json;
  }
  out += "}";
  return out;
}

}  // namespace srsp::core
