#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "device.h"

namespace esphome {
namespace counters_processor {
class CountersProcessor : public uart::UARTDevice, public PollingComponent {
public:
  void setup() override;

  void loop() override;

  void dump_config() override;

  void update() override;

  void send();

  int get_next_active(int i);

  void add_counter_device(Device *device) { devices_.push_back(device); }

protected:
  int active_device_ = -1;
  bool active_initialized_ = false;
  std::vector<Device *> devices_;
};
} // namespace counters_processor
} // namespace esphome