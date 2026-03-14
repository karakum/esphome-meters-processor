#pragma once

#include <sstream>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../counters_processor/device.h"

namespace esphome {
namespace iec61107 {
class ChannelSensor {
public:
  ChannelSensor(u_int8_t zone, sensor::Sensor *sensor)
    : zone(zone),
      sensor(sensor) {
  }

  u_int8_t zone;
  sensor::Sensor *sensor;
};

class MetricSensor {
public:
  MetricSensor(const std::string &code, sensor::Sensor *sensor)
    : code(code),
      sensor(sensor) {
  }

  std::string code;
  sensor::Sensor *sensor;
};


class Iec61107Component : public Component, public counters_processor::Device {
public:
  void setup() override;

  void loop() override;

  void dump_config() override;

  std::string device_type() override { return "IEC61107"; }

  void add_tariff_zone(u_int8_t zone, sensor::Sensor *sensor) { tariff_zones_.emplace_back(zone, sensor); }
  void add_tariff_zone_indi(u_int8_t zone, sensor::Sensor *sensor) { tariff_zones_indi_.emplace_back(zone, sensor); }
  void add_volume_last_day(u_int8_t zone, sensor::Sensor *sensor) { volume_last_day_.emplace_back(zone, sensor); }
  void add_volume_last_hour(u_int8_t zone, sensor::Sensor *sensor) { volume_last_hour_.emplace_back(zone, sensor); }
  void add_metric(const std::string &code, sensor::Sensor *sensor) { metrics_.emplace_back(code, sensor); }

protected:
  void uart_init(uart::UARTComponent *) override;

  bool process(uart::UARTDevice *) override;

  SUB_TEXT_SENSOR(serial_number)
  SUB_TEXT_SENSOR(flags)
  SUB_TEXT_SENSOR(datetime)
  SUB_TEXT_SENSOR(date_indication)
  SUB_SENSOR(datetime_diff)
  SUB_BINARY_SENSOR(connection)
  std::vector<ChannelSensor> tariff_zones_{};
  std::vector<ChannelSensor> tariff_zones_indi_{};
  std::vector<ChannelSensor> volume_last_day_{};
  std::vector<ChannelSensor> volume_last_hour_{};
  std::vector<MetricSensor> metrics_{};

  std::vector<std::string> last_hour_totals_{};
private:
  static const char *ws;

  static std::string &rtrim(std::string &s, const char *t = ws) {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
  }

  static std::string &ltrim(std::string &s, const char *t = ws) {
    s.erase(0, s.find_first_not_of(t));
    return s;
  }

  static std::string &trim(std::string &s, const char *t = ws) {
    return ltrim(rtrim(s, t), t);
  }

  static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
      elems.push_back(trim(item));
    }
    return elems;
  }

  void do_find_device(uart::UARTDevice *serial, std::string &addr, int next_state);

  void do_wait_ticks(int max_ticks, int next_state);

  void do_wait_device(uart::UARTDevice *serial, int next_state, bool warn_error);

  void do_select_mode(uart::UARTDevice *serial, int next_state, const char *mode);

  void do_wait_reading_mode(uart::UARTDevice *serial, int next_state);

  void do_wait_programming_mode(uart::UARTDevice *serial, int next_state);

  void do_get_param(uart::UARTDevice *serial, int next_state, std::string param, std::string arg = "");

  void do_wait_param_result(uart::UARTDevice *serial, const char *param_name, int next_state, int next_fail,
                            std::function<void(std::string data)> success_callback);

  void log_string(uint8_t *bytes, int len, int width = 16);

  std::vector<uint8_t> get_cmd_read_param(std::string param, std::string arg = "");

  uint8_t calc_bcc(std::vector<uint8_t> data);

  static constexpr uint8_t SOH = 0x01;
  static constexpr uint8_t STX = 0x02;
  static constexpr uint8_t ETX = 0x03;
  static constexpr uint8_t EOT = 0x04;
  static constexpr uint8_t ACK = 0x06;

  uint8_t state{};
  int no_data_ticks{};
  int read_bytes{};
  int num_tries{};
  uint8_t buf[1024];
};
} // namespace iec61107
} // namespace esphome