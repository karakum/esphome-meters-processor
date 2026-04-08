#include <map>
#include <ctime>
#include <format>
#include <chrono>
#include "esphome/core/time.h"
#include "esphome/core/log.h"
#include "iec61107_component.h"

namespace esphome {
namespace iec61107 {
static const char *TAG = "iec61107";
const char *Iec61107Component::ws = " \t\n\r\f\v";

void Iec61107Component::setup() {
  connection_binary_sensor_->publish_state(connected_);
}

void Iec61107Component::loop() {
}

void Iec61107Component::uart_init(uart::UARTComponent *uart) {
  if (!this->is_active()) {
    return;
  }
  Device::uart_init(uart);
  state = 1;
  num_tries = 3;
}

bool Iec61107Component::process(uart::UARTDevice *serial) {
  if (state == 1) {
    do_find_device(serial, address_, 2);
  } else if (state == 2) {
    do_wait_device(serial, 3, true);
  } else if (state == 3) {
    do_select_mode(serial, 4, "050");
  } else if (state == 4) {
    do_wait_reading_mode(serial, 21);
  } else if (state == 21) {
    do_find_device(serial, address_, 22);
  } else if (state == 22) {
    do_wait_device(serial, 23, false);
  } else if (state == 23) {
    do_select_mode(serial, 24, "051");
  } else if (state == 24) {
    do_wait_programming_mode(serial, 25);
  } else if (state == 25) {
    do_get_param(serial, 26, "ET0PE");
  } else if (state == 26) {
    do_wait_param_result(serial, "ET0PE", 27, 99, [this](std::string data) -> void {
      std::vector<std::string> list;
      split(trim(data), '\n', list);
      for (ChannelSensor z : tariff_zones_) {
        std::string val = trim(list[z.zone], "()");
        z.sensor->publish_state(std::stof(val));
      }
      if (device_hour_ != 255 && prev_loaded_hour_ != device_hour_) {
        if ((prev_loaded_hour_ + 1) % 24 != device_hour_) {
          // если был перерыв в данных - предыдущие показания отбрасываем
          last_hour_totals_.clear();
        }
        prev_loaded_hour_ = device_hour_;
        if (!last_hour_totals_.empty()) {
          for (ChannelSensor z : volume_last_hour_) {
            std::string val_now = trim(list[z.zone], "()");
            std::string val_hour_ago = trim(last_hour_totals_[z.zone], "()");
            z.sensor->publish_state(std::stof(val_now) - std::stof(val_hour_ago));
          }
          last_hour_totals_.clear();
        }
        split(trim(data), '\n', last_hour_totals_);
      }
    });
  } else if (state == 27) {
    if (prev_loaded_day_ != device_date_) {
      // читаем историю при старте и при смене суток
      prev_loaded_day_ = device_date_;

      // запросить показание на конец суток "<indication_prev_date_>"
      // - это будет показание на начало суток "<indication_date_>"
      state = 28;
    } else {
      state = 100;
    }
  } else if (state == 28) {
    std::string arg = std::format(
        "{:02}.{:02}.{:02}",
        static_cast<unsigned>(indication_prev_date_.day()),
        static_cast<unsigned>(indication_prev_date_.month()),
        int(indication_prev_date_.year()) % 100
        );
    do_get_param(serial, 29, "ENDPE", arg);
  } else if (state == 29) {
    do_wait_param_result(serial, "ENDPE", 30, 99, [this](std::string data) -> void {
      std::vector<std::string> list;
      split(trim(data), '\n', list);
      for (ChannelSensor z : tariff_zones_indi_) {
        std::string val = trim(list[z.zone], "()");
        z.sensor->publish_state(std::stof(val));
      }
      if (date_indication_text_sensor_) {
        std::string indi_date = std::format(
            "{}-{:02}-{:02}",
            int(indication_date_.year()),
            static_cast<unsigned>(indication_date_.month()),
            static_cast<unsigned>(indication_date_.day())
            );
        date_indication_text_sensor_->publish_state(indi_date);
      }
    });
  } else if (state == 30) {
    // запросить объем за предыдущие сутки
    std::string arg = std::format(
        "{:02}.{:02}.{:02}",
        static_cast<unsigned>(yesterday_.day()),
        static_cast<unsigned>(yesterday_.month()),
        int(yesterday_.year()) % 100
        );
    do_get_param(serial, 31, "EADPE", arg);
  } else if (state == 31) {
    do_wait_param_result(serial, "EADPE", 100, 99, [this](std::string data) -> void {
      std::vector<std::string> list;
      split(trim(data), '\n', list);
      for (ChannelSensor z : volume_last_day_) {
        std::string val = trim(list[z.zone], "()");
        z.sensor->publish_state(std::stof(val));
      }
    });
  } else if (state == 99) {
    state = 0;
    connected_ = false;
    connection_binary_sensor_->publish_state(connected_);
  } else if (state == 100) {
    // logout
    serial->write_array(std::vector<uint8_t>{SOH, 'B', '0', ETX, 0x75});
    serial->flush();
    state = 0;
    connected_ = true;
    connection_binary_sensor_->publish_state(connected_);
  }

  return state > 0;
}

void Iec61107Component::do_find_device(uart::UARTDevice *serial, std::string &addr, int next_state) {
  no_data_ticks = 0;
  read_bytes = 0;
  state = next_state;

  while (serial->available()) { serial->read(); }

  std::string r = "/?" + addr + "!\r\n";
  serial->write_str(r.c_str());
  serial->flush();
}

void Iec61107Component::do_wait_device(uart::UARTDevice *serial, int next_state, bool warn_error) {
  no_data_ticks++;
  if (no_data_ticks >= 30) {
    if (num_tries > 0) {
      num_tries--;
      state--;
    } else {
      ESP_LOGE(TAG, "No answer within 30 ticks and 3 filed tries");
      state = 99;
    }
  } else {
    size_t l = serial->available();
    if (l > 0) {
      if ((read_bytes + l) < sizeof(buf)) {
        serial->read_array(reinterpret_cast<uint8_t *>(&buf) + read_bytes, l);
        read_bytes += l;
      }
      std::string answer(reinterpret_cast<const char *>(&buf), read_bytes);
      if (answer.find("\r\n") != std::string::npos) {
        ESP_LOGV(TAG, "Tick %d, read: %d", no_data_ticks, read_bytes);
        if (str_startswith(answer, "/")) {
          std::string manuf = answer.substr(1, 3);
          std::string device = answer.substr(5);
          device = device.substr(0, device.find("\r\n"));
          if (warn_error) {
            ESP_LOGD(TAG, "Device found: %s, Manufacturer: %s, Speed: %c", device.c_str(), manuf.c_str(), answer.at(4));
          }
          state = next_state;
        } else {
          ESP_LOGE(TAG, "Unknown answer:");
          log_string(buf, read_bytes);
          state = 99;
        }
      }
    }
  }
}

void Iec61107Component::do_wait_reading_mode(uart::UARTDevice *serial, int next_state) {
  no_data_ticks++;
  if (no_data_ticks >= 150) {
    ESP_LOGE(TAG, "No answer within 100 ticks");
    state = 99;
  } else {
    size_t l = serial->available();
    if (l > 0) {
      if ((read_bytes + l) < sizeof(buf)) {
        serial->read_array(reinterpret_cast<uint8_t *>(&buf) + read_bytes, l);
        read_bytes += l;
      }
      std::string answer(reinterpret_cast<const char *>(&buf), read_bytes);
      size_t etx_pos = answer.find(ETX);
      if (etx_pos != std::string::npos && read_bytes > (etx_pos + 1)) {
        ESP_LOGV(TAG, "Tick %d, read: %d, ETX pos: %lu", no_data_ticks, read_bytes, etx_pos);
        if (answer[0] == STX) {
          ESP_LOGV(TAG, "Reading mode entered!");
          std::string data = answer.substr(1);
          data = data.substr(0, data.find(ETX));
          std::vector<uint8_t> check(data.length() + 1);
          std::copy(data.begin(), data.end(), check.begin());
          check.push_back(ETX);
          uint8_t bcc = answer[etx_pos + 1];
          uint8_t sum = calc_bcc(check);
          if (bcc == sum) {
            std::map<std::string, std::string> params;
            std::vector<std::string> list;
            split(trim(trim(data), "!"), '\n', list);
            for (size_t i = 0; i < list.size(); i++) {
              std::vector<std::string> pair;
              split(trim(trim(list[i]), ")"), '(', pair);
              if (pair.size() > 1) {
                std::string p = pair[0];
                std::string v = pair[1];
                params[p] = v;
              }
            }
            if (serial_number_text_sensor_ && params.count("SNUMB")) {
              serial_number_text_sensor_->publish_state(params["SNUMB"]);
            }
            if (flags_text_sensor_ && params.count("STAT_")) {
              flags_text_sensor_->publish_state(params["STAT_"]);
            }
            if ((datetime_text_sensor_ || datetime_diff_sensor_)
                && params.count("DATE_") && params.count("TIME_")) {
              std::string date_s = params["DATE_"];
              std::string time_s = params["TIME_"];
              std::vector<std::string> d_parts;
              std::vector<std::string> t_parts;
              split(date_s, '.', d_parts);
              split(time_s, ':', t_parts);
              time_t now = ::time(nullptr);

              auto t_now = ESPTime::from_epoch_local(now);
              auto t_dev = ESPTime::from_epoch_local(now);

              t_dev.year = std::stoi(d_parts[3]) + 2000;
              t_dev.month = std::stoi(d_parts[2]);
              t_dev.day_of_month = std::stoi(d_parts[1]);
              t_dev.day_of_week = std::stoi(d_parts[0]) + 1;
              t_dev.hour = std::stoi(t_parts[0]);
              t_dev.minute = std::stoi(t_parts[1]);
              t_dev.second = std::stoi(t_parts[2]);
              t_dev.recalc_timestamp_local();

              set_device_date(&t_dev);

              int diff = t_now.timestamp - t_dev.timestamp;

              if (datetime_diff_sensor_) {
                datetime_diff_sensor_->publish_state(diff);
              }
              if (datetime_text_sensor_) {
                auto dt = t_dev.strftime("%Y-%m-%dT%H:%M:%S");
                auto sec = ESPTime::timezone_offset();
                int h = sec / 3600;
                int m = (sec % 3600) / 60;
                datetime_text_sensor_->publish_state(std::format("{}{}{:02}{:02}", dt, sec >= 0 ? "+" : "-", h, m));
              }
            }
            for (const MetricSensor &metric : metrics_) {
              if (params.count(metric.code)) {
                metric.sensor->publish_state(std::stof(params[metric.code]));
              }
            }
            state = next_state;
          } else {
            ESP_LOGE(TAG, "Wrong BCC: 0x%02x, expect 0x%02x", bcc, sum);
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

void Iec61107Component::do_wait_programming_mode(uart::UARTDevice *serial, int next_state) {
  no_data_ticks++;
  if (no_data_ticks >= 30) {
    ESP_LOGE(TAG, "No answer within 30 ticks");
    state = 99;
  } else {
    size_t l = serial->available();
    if (l > 0) {
      if ((read_bytes + l) < sizeof(buf)) {
        serial->read_array(reinterpret_cast<uint8_t *>(&buf) + read_bytes, l);
        read_bytes += l;
      }
      std::string answer(reinterpret_cast<const char *>(&buf), read_bytes);
      size_t etx_pos = answer.find(ETX);
      if (etx_pos != std::string::npos && read_bytes > (etx_pos + 1)) {
        ESP_LOGV(TAG, "Tick %d, read: %d, ETX pos: %lu", no_data_ticks, read_bytes, etx_pos);
        if (answer[0] == SOH) {
          std::string data = answer.substr(1);
          data = data.substr(0, data.find(ETX));
          std::vector<uint8_t> check(data.length() + 1);
          std::copy(data.begin(), data.end(), check.begin());
          check.push_back(ETX);
          uint8_t bcc = answer[etx_pos + 1];
          uint8_t sum = calc_bcc(check);

          if (bcc == sum) {
            if (data[0] == 'P' && data[1] == '0' && data[2] == STX) {
              data = data.substr(4);
              data = data.substr(0, data.find(")"));
              if (data == address_) {
                ESP_LOGV(TAG, "Programming mode entered!");
                state = next_state;
              } else {
                ESP_LOGE(TAG, "Wrong device serial: %s, expect %s", data.c_str(), address_.c_str());
                log_string(buf, read_bytes);
                state = 99;
              }
            } else {
              ESP_LOGE(TAG, "Unknown answer:");
              log_string(buf, read_bytes);
              state = 99;
            }
          } else {
            ESP_LOGE(TAG, "Wrong BCC: 0x%02x, expect 0x%02x", bcc, sum);
            log_string(buf, read_bytes);
            state = 99;
          }
        }
      }
    }
  }
}

void Iec61107Component::do_wait_param_result(uart::UARTDevice *serial, const char *param_name, int next_state,
                                             int next_fail, std::function<void(std::string data)> success_callback) {
  no_data_ticks++;
  if (no_data_ticks >= 50) {
    ESP_LOGE(TAG, "No answer within 50 ticks");
    state = next_fail;
  } else {
    size_t l = serial->available();
    if (l > 0) {
      if ((read_bytes + l) < sizeof(buf)) {
        serial->read_array(reinterpret_cast<uint8_t *>(&buf) + read_bytes, l);
        read_bytes += l;
      }
      std::string answer(reinterpret_cast<const char *>(&buf), read_bytes);
      size_t etx_pos = answer.find(ETX);
      if (etx_pos != std::string::npos && read_bytes > etx_pos + 1) {
        ESP_LOGV(TAG, "Tick %d, read: %d", no_data_ticks, read_bytes);
        if (answer[0] == STX) {
          std::string data = answer.substr(1);
          data = data.substr(0, data.find(ETX));
          std::vector<uint8_t> check(data.length() + 1);
          std::copy(data.begin(), data.end(), check.begin());
          check.push_back(ETX);
          uint8_t bcc = answer[etx_pos + 1];
          uint8_t sum = calc_bcc(check);
          if (bcc == sum) {
            std::string param = data.substr(0, data.find("("));
            data = data.substr(data.find("("));
            if (param == param_name) {
              success_callback(data);
              state = next_state;
            } else {
              ESP_LOGE(TAG, "Wrong param: %s, expect %s", param.c_str(), param_name);
              log_string(buf, read_bytes);
              state = next_fail;
            }
          } else {
            ESP_LOGE(TAG, "Wrong BCC: 0x%02x, expect 0x%02x", bcc, sum);
            log_string(buf, read_bytes);
            state = next_fail;
          }
        } else {
          ESP_LOGE(TAG, "Unknown answer:");
          log_string(buf, read_bytes);
          state = next_fail;
        }
      }
    }
  }
}

void Iec61107Component::do_wait_ticks(int max_ticks, int next_state) {
  no_data_ticks++;
  if (no_data_ticks >= max_ticks) {
    state = next_state;
    no_data_ticks = 0;
  }
}

void Iec61107Component::do_select_mode(uart::UARTDevice *serial, int next_state, const char *mode) {
  serial->write_byte(ACK);
  serial->write_str(mode);
  serial->write_str("\r\n");
  serial->flush();
  state = next_state;
  read_bytes = 0;
  no_data_ticks = 0;
}

void Iec61107Component::do_get_param(uart::UARTDevice *serial, int next_state, std::string param, std::string arg) {
  auto r = get_cmd_read_param(param, arg);
  // ESP_LOGD(TAG, "REQUEST: %s(%s)", param.c_str(), arg.c_str());
  // log_string(r.data(), r.size());
  serial->write_array(r);
  serial->flush();
  read_bytes = 0;
  no_data_ticks = 0;
  state = next_state;
}

std::vector<uint8_t> Iec61107Component::get_cmd_read_param(std::string p, std::string arg) {
  std::string param = p + "(" + arg + ")";
  std::vector<uint8_t> data = std::vector<uint8_t>(4 + param.length());
  std::vector<uint8_t> res;
  data[0] = 'R';
  data[1] = '1';
  data[2] = STX;
  std::copy(param.begin(), param.end(), data.begin() + 3);
  data[data.size() - 1] = ETX;
  uint8_t bcc = calc_bcc(data);
  res.push_back(SOH);
  res.insert(res.end(), data.begin(), data.end());
  res.push_back(bcc);

  return res;
}

uint8_t Iec61107Component::calc_bcc(std::vector<uint8_t> data) {
  uint8_t sum = 0;
  for (uint8_t b : data) {
    sum += b;
  }
  return sum & 0x7f;
}

void Iec61107Component::log_string(uint8_t *bytes, int len, int width) {
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
    res += "  ";

    for (size_t i = 0; i < width; i++) {
      if (l * width + i < len) {
        b = bytes[l * width + i];
        if (b == 7) {
          res += "\\a";
        } else if (b == 8) {
          res += "\\b";
        } else if (b == 9) {
          res += "\\t";
        } else if (b == 10) {
          res += "\\n";
        } else if (b == 11) {
          res += "\\v";
        } else if (b == 12) {
          res += "\\f";
        } else if (b == 13) {
          res += "\\r";
        } else if (b == 27) {
          res += "\\e";
        } else if (b == 34) {
          res += "\\\"";
        } else if (b == 39) {
          res += "\\'";
        } else if (b == 92) {
          res += "\\\\";
        } else if (b < 32 || b > 127) {
          buf_append_printf(buf, sizeof(buf), 0, "\\x%02X", b);
          res += buf;
        } else {
          res += b;
        }
      }
    }
    ESP_LOGV(TAG, "%s", res.c_str());
  }
}

void Iec61107Component::dump_config() {
  ESP_LOGCONFIG(TAG, "IEC61107 device");
  ESP_LOGCONFIG(TAG, "  Address: %s", address_.c_str());
  ESP_LOGCONFIG(TAG,
                "  Baud Rate: %" PRIu32 " baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop bits: %u"
                ,
                this->baud_rate_, this->data_bits_, LOG_STR_ARG(parity_to_str(this->parity_)), this->stop_bits_);

  ESP_LOGCONFIG(TAG, "  is_enable lamda: %s", is_enable_f_.has_value()?"yes":"no");
  if (serial_number_text_sensor_) {
    LOG_TEXT_SENSOR("  ", "Serial number", serial_number_text_sensor_);
  }
  if (flags_text_sensor_) {
    LOG_TEXT_SENSOR("  ", "Device flags", flags_text_sensor_);
  }
  if (datetime_text_sensor_) {
    LOG_TEXT_SENSOR("  ", "Device datetime", datetime_text_sensor_);
  }
  if (datetime_diff_sensor_) {
    LOG_SENSOR("  ", "Device datetime diff", datetime_diff_sensor_);
  }
  ESP_LOGCONFIG(TAG, "  Tariff zones:");
  for (const ChannelSensor &zone : tariff_zones_) {
    std::string text = std::format("Tariff zone index: {}", zone.zone);
    LOG_SENSOR("    ", text.c_str(), zone.sensor);
  }
  if (!this->metrics_.empty()) {
    ESP_LOGCONFIG(TAG, "  Metrics:");
    for (const MetricSensor &metric : metrics_) {
      LOG_SENSOR("    ", metric.code.c_str(), metric.sensor);
    }
  }
  ESP_LOGCONFIG(TAG, "  Channels indication:");
  for (const ChannelSensor &zone : tariff_zones_indi_) {
    std::string text = std::format("Tariff zone index: {}", zone.zone);
    LOG_SENSOR("    ", text.c_str(), zone.sensor);
  }
  ESP_LOGCONFIG(TAG, "  Volume last DAY:");
  for (const ChannelSensor &zone : volume_last_day_) {
    std::string text = std::format("Tariff zone index: {}", zone.zone);
    LOG_SENSOR("    ", text.c_str(), zone.sensor);
  }
  ESP_LOGCONFIG(TAG, "  Volume last HOUR:");
  for (const ChannelSensor &zone : volume_last_hour_) {
    std::string text = std::format("Tariff zone index: {}", zone.zone);
    LOG_SENSOR("    ", text.c_str(), zone.sensor);
  }
}
} // namespace iec61107
} // namespace esphome