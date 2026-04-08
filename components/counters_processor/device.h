#pragma once

#include <chrono>

#include "esphome/core/time.h"
#include "esphome/core/template_lambda.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace counters_processor {
class Device {
  friend class CountersProcessor;

public:
  virtual ~Device() = default;

  virtual std::string device_type() = 0;

  bool is_active() {
    auto s = is_enable_f_();
    if (s.has_value()) {
      return *s;
    }
    return false;
  };

  void set_address(const std::string &address) { address_ = address; }
  std::string get_address() const { return address_; }

  void set_password(const std::string &password) { password_ = password; }
  std::string get_password() const { return password_; }

  void sync_device_datetime() { set_time_ = true; };

  void set_indication_day(uint8_t d) { indication_day_ = d; }

  template<typename F> void set_is_enable_lambda(F &&f) { this->is_enable_f_.set(std::forward<F>(f)); }

  void set_stop_bits(uint8_t stop_bits) { this->stop_bits_ = stop_bits; }
  uint8_t get_stop_bits() const { return this->stop_bits_; }

  void set_data_bits(uint8_t data_bits) { this->data_bits_ = data_bits; }
  uint8_t get_data_bits() const { return this->data_bits_; }

  void set_parity(uart::UARTParityOptions parity) { this->parity_ = parity; }
  uart::UARTParityOptions get_parity() const { return this->parity_; }

  void set_baud_rate(uint32_t baud_rate) { baud_rate_ = baud_rate; }
  uint32_t get_baud_rate() const { return baud_rate_; }

  void set_device_date(ESPTime *t_dev) {
    uint16_t dev_year = t_dev->year;
    uint8_t dev_mon = t_dev->month;
    uint8_t dev_day = t_dev->day_of_month;

    uint8_t indi_day = indication_day_; // в истории показания "на начало суток"
    uint8_t indi_mon = dev_mon;
    uint16_t indi_year = dev_year;
    if (dev_day < indication_day_) {
      indi_mon = dev_mon > 1 ? dev_mon - 1 : 12;         // если сейчас январь, то предыдущий ДЕКАБРЬ
      indi_year = dev_mon > 1 ? dev_year : dev_year - 1; // если сейчас январь, то предыдущий ГОД
    }

    device_hour_ = t_dev->hour;

    std::chrono::sys_days sys_indi_date{std::chrono::month(indi_mon) / indi_day / indi_year};
    indication_date_ = sys_indi_date;
    indication_prev_date_ = sys_indi_date - std::chrono::days{1};

    std::chrono::sys_days sys_dev_date{std::chrono::month(dev_mon) / dev_day / dev_year};
    device_date_ = sys_dev_date;
    yesterday_ = sys_dev_date - std::chrono::days{1};
  }

protected:
  virtual void uart_init(uart::UARTComponent *uart) {
    uart->set_baud_rate(baud_rate_);
    uart->set_stop_bits(stop_bits_);
    uart->set_parity(parity_);
    uart->set_data_bits(data_bits_);
  };

  virtual bool process(uart::UARTDevice *) = 0;

  std::string address_{};
  std::string password_{};
  TemplateLambda<bool> is_enable_f_;
  bool connected_{true};
  bool set_time_{};
  uint32_t baud_rate_{0};
  uint8_t stop_bits_{0};
  uint8_t data_bits_{0};
  uart::UARTParityOptions parity_{uart::UART_CONFIG_PARITY_NONE};

  uint8_t indication_day_{0};
  std::chrono::year_month_day device_date_;
  std::chrono::year_month_day yesterday_;
  std::chrono::year_month_day indication_date_;
  std::chrono::year_month_day indication_prev_date_;

  std::chrono::year_month_day prev_loaded_day_{std::chrono::January / 1 / 2000};
  uint8_t device_hour_{255};
  uint8_t prev_loaded_hour_{255};
};
} // namespace counters_processor
} // namespace esphome