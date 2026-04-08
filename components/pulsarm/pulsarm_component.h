#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../counters_processor/device.h"

namespace esphome {
namespace pulsarm {
class ChannelSensor {
public:
  ChannelSensor(u_int8_t channel, sensor::Sensor *sensor)
    : channel(channel),
      sensor(sensor) {
  }

  u_int8_t channel;
  sensor::Sensor *sensor;
};

class PulsarMComponent : public Component, public counters_processor::Device {
public:
  void setup() override;

  void loop() override;

  void dump_config() override;

  std::string device_type() override { return "Pulsar M"; }

  void add_channel(u_int8_t ch, sensor::Sensor *sensor) { channels_.emplace_back(ch, sensor); }
  void add_channel_indi(u_int8_t ch, sensor::Sensor *sensor) { channels_indi_.emplace_back(ch, sensor); }
  void add_volume_last_day(u_int8_t ch, sensor::Sensor *sensor) { volume_last_day_.emplace_back(ch, sensor); }
  void add_volume_last_hour(u_int8_t ch, sensor::Sensor *sensor) { volume_last_hour_.emplace_back(ch, sensor); }

protected:
  void uart_init(uart::UARTComponent *) override;

  bool process(uart::UARTDevice *) override;

  SUB_TEXT_SENSOR(datetime)
  SUB_TEXT_SENSOR(date_indication)
  SUB_SENSOR(datetime_diff)
  SUB_BINARY_SENSOR(connection)
  std::vector<ChannelSensor> channels_{};
  std::vector<ChannelSensor> channels_indi_{};
  std::vector<ChannelSensor> volume_last_day_{};
  std::vector<ChannelSensor> volume_last_hour_{};

private:
  // type 0x0001- часовой; 0x0002-суточный; 0x0003 месячный
  static constexpr uint8_t HISTORY_TYPE_HOUR = 0x01;
  static constexpr uint8_t HISTORY_TYPE_DAY = 0x02;
  static constexpr uint8_t HISTORY_TYPE_MONTH = 0x03;

  void do_get_device_time(uart::UARTDevice *serial, int next_state);

  void do_set_device_time(uart::UARTDevice *serial, int next_state);

  void do_get_device_channels(uart::UARTDevice *serial, int next_state);

  void do_get_device_channel_value_at_date(uart::UARTDevice *serial, int next_state,
                                           uint8_t channel,
                                           std::chrono::year_month_day date);

  void do_get_device_channel_value_at_dates(uart::UARTDevice *serial, int next_state,
                                            uint8_t channel, uint8_t history_type,
                                            std::chrono::year_month_day date_from,
                                            std::chrono::year_month_day date_to,
                                            uint8_t hour_from = 0, uint8_t hour_to = 0);

  std::vector<uint8_t> create_request(uint32_t addr, uint8_t func);

  std::vector<uint8_t> create_request(uint32_t addr, uint8_t func, std::vector<uint8_t> data);

  void do_wait_response(uart::UARTDevice *serial, int next_state, std::function<void(uint8_t *data, size_t len)>);

  uint16_t crc16(uint8_t *Data, uint16_t size);

  void log_string(uint8_t *bytes, int len, int width = 16);

  uint32_t addr{};
  uint8_t addr0{};
  uint8_t addr1{};
  uint8_t addr2{};
  uint8_t addr3{};
  uint8_t func{};
  uint16_t req{};
  uint8_t state{};
  int no_data_ticks{};
  int read_bytes{};
  int num_tries{};
  uint8_t buf[300];
  uint8_t current_channel_index{};
};
} // namespace pulsarm
} // namespace esphome