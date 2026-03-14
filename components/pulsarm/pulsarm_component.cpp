#include <format>
#include <ctime>
#include <chrono>
#include "esphome/core/log.h"
#include "pulsarm_component.h"

namespace esphome {
namespace pulsarm {
static const char *TAG = "pulsarm";

void PulsarMComponent::setup() {
  std::string str = "0x" + address_;
  addr = std::stoul(str, nullptr, 16);
  addr0 = (addr >> 24) & 0xFF;
  addr1 = (addr >> 16) & 0xFF;
  addr2 = (addr >> 8) & 0xFF;
  addr3 = (addr) & 0xFF;
  req = 0;
  set_time = false;
  connection_binary_sensor_->publish_state(connected_);
}

void PulsarMComponent::loop() {
}

void PulsarMComponent::uart_init(uart::UARTComponent *uart) {
  if (!this->is_active()) {
    return;
  }
  Device::uart_init(uart);
  state = 1;
  num_tries = 3;
}

bool PulsarMComponent::process(uart::UARTDevice *serial) {
  if (state == 1) {
    while (serial->available()) { serial->read(); }
    if (set_time) {
      set_time = false;
      do_set_device_time(serial, 2);
    } else {
      state = 3;
    }
  } else if (state == 2) {
    do_wait_response(serial, 3, [this](uint8_t *data, size_t len) -> void {
      if (data[0] > 0) {
        ESP_LOGI(TAG, "Device time set success");
      } else {
        ESP_LOGE(TAG, "Device time set failed");
      }
    });
  } else if (state == 3) {
    do_get_device_time(serial, 4);
  } else if (state == 4) {
    do_wait_response(serial, 5, [this](uint8_t *data, size_t len) -> void {
      time_t now = time(0);

      struct tm tm_now;
      struct tm tm_dev;
      localtime_r(&now, &tm_now);
      localtime_r(&now, &tm_dev);

      tm_dev.tm_year = data[0] + 2000 - 1900;
      tm_dev.tm_mon = data[1] - 1;
      tm_dev.tm_mday = data[2];
      tm_dev.tm_hour = data[3];
      tm_dev.tm_min = data[4];
      tm_dev.tm_sec = data[5];

      set_device_date(&tm_dev);

      time_t dev = mktime(&tm_dev);
      int diff = now - dev;

      if (datetime_diff_sensor_) {
        datetime_diff_sensor_->publish_state(diff);
      }
      if (datetime_text_sensor_) {
        char strftime_buf[30];
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M:%S%z", &tm_dev);
        datetime_text_sensor_->publish_state(strftime_buf);
      }
    });
  } else if (state == 5) {
    do_get_device_channels(serial, 6);
  } else if (state == 6) {
    do_wait_response(serial, 7, [this](uint8_t *data, size_t len) -> void {
      double_t *u = (double_t *) data;
      for (const ChannelSensor &ch : channels_) {
        ch.sensor->publish_state(u[ch.channel - 1]);
      }
    });
  } else if (state == 7) {
    if (prev_loaded_day_ != device_date_) {
      // читаем историю при старте и при смене суток
      prev_loaded_day_ = device_date_;
      state = 8;
      current_channel_index = 0;
    } else {
      state = 100;
    }
  } else if (state == 8) {
    // запросить показание на начало суток "<indication_date_>" для всех каналов
    do_get_device_channel_value_at_date(serial, 9, channels_indi_[current_channel_index].channel, indication_date_);
  } else if (state == 9) {
    do_wait_response(serial, 10, [this](uint8_t *data, size_t len) -> void {
      float_t *u = (float_t *) (data + 10);
      channels_indi_[current_channel_index].sensor->publish_state(u[0]);
    });
  } else if (state == 10) {
    if (current_channel_index + 1 < channels_indi_.size()) {
      current_channel_index++;
      state = 8;
    } else {
      if (date_indication_text_sensor_) {
        date_indication_text_sensor_->publish_state(std::format("{}", indication_date_));
      }
      state = 11;
      current_channel_index = 0;
    }
  } else if (state == 11) {
    // запросить показание на начало суток от "вчера" до "сегодня" для всех каналов и посчитать объем за "вчера"
    do_get_device_channel_value_at_dates(serial, 12,
                                         channels_indi_[current_channel_index].channel, HISTORY_TYPE_DAY,
                                         yesterday_, device_date_);
  } else if (state == 12) {
    do_wait_response(serial, 13, [this](uint8_t *data, size_t len) -> void {
      float_t *u = (float_t *) (data + 10);
      volume_last_day_[current_channel_index].sensor->publish_state(u[1] - u[0]);
    });
  } else if (state == 13) {
    if (current_channel_index + 1 < channels_indi_.size()) {
      current_channel_index++;
      state = 11;
    } else {
      state = 14;
      current_channel_index = 0;
    }
  } else if (state == 14) {
    if (device_hour_ != 255 && prev_loaded_hour_ != device_hour_) {
      prev_loaded_hour_ = device_hour_;
      state = 15;
      current_channel_index = 0;
    } else {
      state = 100;
    }
  } else if (state == 15) {
    do_get_device_channel_value_at_dates(serial, 16,
                                         channels_indi_[current_channel_index].channel, HISTORY_TYPE_HOUR,
                                         device_hour_ > 0 ? device_date_ : yesterday_,
                                         device_date_,
                                         device_hour_ > 0 ? device_hour_ - 1 : 23,
                                         device_hour_);
  } else if (state == 16) {
    do_wait_response(serial, 17, [this](uint8_t *data, size_t len) -> void {
      float_t *u = (float_t *) (data + 10);
      volume_last_hour_[current_channel_index].sensor->publish_state(u[1] - u[0]);
    });
  } else if (state == 17) {
    if (current_channel_index + 1 < channels_indi_.size()) {
      current_channel_index++;
      state = 15;
    } else {
      state = 100;
      current_channel_index = 0;
    }
  } else if (state == 99) {
    state = 0;
    connected_ = false;
    connection_binary_sensor_->publish_state(connected_);
  } else if (state == 100) {
    state = 0;
    connected_ = true;
    connection_binary_sensor_->publish_state(connected_);
  }
  return state > 0;
}

void PulsarMComponent::do_get_device_time(uart::UARTDevice *serial, int next_state) {
  no_data_ticks = 0;
  read_bytes = 0;
  state = next_state;
  std::vector<uint8_t> r = create_request(addr, 4);
  serial->write_array(r);
  serial->flush();
}

void PulsarMComponent::do_set_device_time(uart::UARTDevice *serial, int next_state) {
  no_data_ticks = 0;
  read_bytes = 0;
  state = next_state;
  struct tm tm_now;
  std::vector<uint8_t> data;
  time_t now = time(0);
  localtime_r(&now, &tm_now);
  data.push_back(tm_now.tm_year - 100);
  data.push_back(tm_now.tm_mon + 1);
  data.push_back(tm_now.tm_mday);
  data.push_back(tm_now.tm_hour);
  data.push_back(tm_now.tm_min);
  data.push_back(tm_now.tm_sec);
  std::vector<uint8_t> r = create_request(addr, 5, data);
  serial->write_array(r);
  serial->flush();
}

void PulsarMComponent::do_get_device_channels(uart::UARTDevice *serial, int next_state) {
  no_data_ticks = 0;
  read_bytes = 0;
  state = next_state;
  std::vector<uint8_t> mask;
  uint8_t max_ch = 0;
  for (const ChannelSensor &ch : channels_) {
    max_ch = (max_ch < ch.channel) ? ch.channel : max_ch;
  }
  uint16_t mask_int = (1 << max_ch) - 1;
  mask.push_back(mask_int & 0xFF);
  mask.push_back((mask_int >> 8) & 0xFF);
  mask.push_back(0);
  mask.push_back(0);
  std::vector<uint8_t> r = create_request(addr, 1, mask);
  serial->write_array(r);
  serial->flush();
}

void PulsarMComponent::do_get_device_channel_value_at_date(uart::UARTDevice *serial, int next_state,
                                                           uint8_t channel,
                                                           std::chrono::year_month_day date) {
  do_get_device_channel_value_at_dates(serial, next_state,
                                       channel, HISTORY_TYPE_DAY,
                                       date, date);
}

void PulsarMComponent::do_get_device_channel_value_at_dates(uart::UARTDevice *serial, int next_state,
                                                            uint8_t channel, uint8_t history_type,
                                                            std::chrono::year_month_day date_from,
                                                            std::chrono::year_month_day date_to,
                                                            uint8_t hour_from, uint8_t hour_to) {
  no_data_ticks = 0;
  read_bytes = 0;
  state = next_state;
  std::vector<uint8_t> data;
  uint16_t mask_int = 1 << (channel - 1);
  //channel
  data.push_back(mask_int & 0xFF);
  data.push_back((mask_int >> 8) & 0xFF);
  data.push_back(0);
  data.push_back(0);
  // type 0x0001- часовой; 0x0002-суточный; 0x0003 месячный
  data.push_back(history_type);
  data.push_back(0);
  // start date
  data.push_back(int(date_from.year()) - 2000);
  data.push_back(static_cast<unsigned>(date_from.month()));
  data.push_back(static_cast<unsigned>(date_from.day()));
  data.push_back(hour_from);
  data.push_back(0);
  data.push_back(0);
  // end date
  data.push_back(int(date_to.year()) - 2000);
  data.push_back(static_cast<unsigned>(date_to.month()));
  data.push_back(static_cast<unsigned>(date_to.day()));
  data.push_back(hour_to);
  data.push_back(0);
  data.push_back(0);

  std::vector<uint8_t> r = create_request(addr, 6, data);
  serial->write_array(r);
  serial->flush();
}

void PulsarMComponent::do_wait_response(uart::UARTDevice *serial, int next_state,
                                        std::function<void(uint8_t *data, size_t len)> success_callback) {
  no_data_ticks++;
  if (no_data_ticks >= 100) {
    if (num_tries > 0) {
      num_tries--;
      state--;
    } else {
      ESP_LOGE(TAG, "No answer within 100 ticks and 3 filed tries");
      state = 99;
    }
  } else {
    size_t l = serial->available();
    if (l > 0) {
      if ((read_bytes + l) < sizeof(buf)) {
        serial->read_array(reinterpret_cast<uint8_t *>(&buf) + read_bytes, l);
        read_bytes += l;
      }
      if (read_bytes >= 6) {
        uint8_t l = buf[5];
        if (read_bytes >= l) {
          ESP_LOGV(TAG, "Tick %d, read: %d", no_data_ticks, read_bytes);
          if (buf[0] == addr0 && buf[1] == addr1 && buf[2] == addr2 && buf[3] == addr3
              && buf[l - 4] == ((req >> 8) & 0xFF) && buf[l - 3] == (req & 0xFF)) {
            uint16_t crc = crc16(buf, l - 2);
            uint8_t crc1 = crc & 0xFF;
            uint8_t crc2 = (crc >> 8) & 0xFF;
            if (buf[l - 2] == crc1 && buf[l - 1] == crc2) {
              if (buf[4] == func) {
                success_callback(buf + 6, buf[5] - 10);
                state = next_state;
              } else if (buf[4] == 0) {
                ESP_LOGE(TAG, "Error answer: 0x%02x", buf[6]);
                state = 99;
              } else {
                ESP_LOGE(TAG, "Func mismatch: 0x%02x, expect 0x%02x", buf[4], func);
                state = 99;
              }
            } else {
              ESP_LOGE(TAG, "Wrong CRC16: 0x%02x 0x%02x, expect 0x%02x 0x%02x", buf[l-2], buf[l-1], crc1, crc2);
              log_string(buf, read_bytes);
              state = 99;
            }
          } else {
            ESP_LOGE(TAG, "Unknown answer:");
            log_string(buf, read_bytes);
            state = 99;
          }
        }
      }
    }
  }
}

std::vector<uint8_t> PulsarMComponent::create_request(uint32_t addr, uint8_t f) {
  return create_request(addr, f, std::vector<uint8_t>());
}

std::vector<uint8_t> PulsarMComponent::create_request(uint32_t addr, uint8_t f, std::vector<uint8_t> data) {
  func = f;
  std::vector<uint8_t> request;
  request.push_back(addr0);
  request.push_back(addr1);
  request.push_back(addr2);
  request.push_back(addr3);
  request.push_back(func);
  request.push_back(10 + data.size());
  request.insert(request.end(), data.begin(), data.end());
  req++;
  request.push_back((req >> 8) & 0xFF);
  request.push_back(req & 0xFF);

  uint16_t crc = crc16(request.data(), request.size());
  request.push_back(crc & 0xFF);
  request.push_back((crc >> 8) & 0xFF);

  return request;
}

uint16_t PulsarMComponent::crc16(uint8_t *Data, uint16_t size) {
  uint16_t w;
  uint8_t shift_cnt, f;
  uint8_t *ptrByte;

  uint16_t byte_cnt = size;
  ptrByte = Data;
  w = (uint16_t) 0xffff;
  for (; byte_cnt > 0; byte_cnt--) {
    w = (uint16_t) (w ^ (uint16_t) (*ptrByte++));
    for (shift_cnt = 0; shift_cnt < 8; shift_cnt++) {
      f = (uint8_t) ((w) & (0x1));
      w >>= 1;
      if ((f) == 1)
        w = (uint16_t) ((w) ^ 0xa001);
    }
  }
  return w;
}

void PulsarMComponent::log_string(uint8_t *bytes, int len, int width) {
  char buf[5];
  uint8_t b;
  for (int l = 0; l <= len / width; l++) {
    std::string res = "<<< ";
    for (size_t i = 0; i < width; i++) {
      if (l * width + i < len) {
        buf_append_printf(buf, sizeof(buf), 0, "%02X ", bytes[l * width + i]);
        res += buf;
      } else {
        res += "   ";
      }
    }
    ESP_LOGD(TAG, "%s", res.c_str());
  }
}

void PulsarMComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "PulsarM device");
  ESP_LOGCONFIG(TAG, "  Address: %s", address_.c_str());
  ESP_LOGCONFIG(TAG,
                "  Baud Rate: %" PRIu32 " baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop bits: %u"
                ,
                this->baud_rate_, this->data_bits_, LOG_STR_ARG(parity_to_str(this->parity_)), this->stop_bits_);

  ESP_LOGCONFIG(TAG, "  is_enable lamda: %s", is_enable_f_.has_value()?"yes":"no");
  if (datetime_text_sensor_) {
    LOG_TEXT_SENSOR("  ", "Device datetime", datetime_text_sensor_);
  }
  if (datetime_diff_sensor_) {
    LOG_SENSOR("    ", "Device datetime diff", datetime_diff_sensor_);
  }
  ESP_LOGCONFIG(TAG, "  Channels:");
  for (const ChannelSensor &ch : channels_) {
    std::string text = std::format("Channel: {}", ch.channel);
    LOG_SENSOR("    ", text.c_str(), ch.sensor);
  }
  ESP_LOGCONFIG(TAG, "  Channels indication:");
  for (const ChannelSensor &ch : channels_indi_) {
    std::string text = std::format("Channel: {}", ch.channel);
    LOG_SENSOR("    ", text.c_str(), ch.sensor);
  }
  ESP_LOGCONFIG(TAG, "  Volume last DAY:");
  for (const ChannelSensor &ch : volume_last_day_) {
    std::string text = std::format("Channel: {}", ch.channel);
    LOG_SENSOR("    ", text.c_str(), ch.sensor);
  }
  ESP_LOGCONFIG(TAG, "  Volume last HOUR:");
  for (const ChannelSensor &ch : volume_last_hour_) {
    std::string text = std::format("Channel: {}", ch.channel);
    LOG_SENSOR("    ", text.c_str(), ch.sensor);
  }
}
} // namespace pulsarm
} // namespace esphome