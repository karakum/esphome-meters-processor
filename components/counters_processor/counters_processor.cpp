#include "counters_processor.h"
#include "esphome/components/network/util.h"


namespace esphome {
namespace counters_processor {
static const char *TAG = "counters_processor";

void CountersProcessor::setup() {
  active_device_ = -1;
}

void CountersProcessor::update() {
  //send();
}

void CountersProcessor::loop() {
  if (active_device_ >= 0) {
    if (devices_.size() > 0) {
      Device *d = devices_[active_device_];
      if (!active_initialized_) {
        ESP_LOGD(TAG, "-----------------------------------------------");
        ESP_LOGD(TAG, "INIT UART device %s, address %s", d->device_type().c_str(), d->get_address().c_str());
        d->uart_init(this->parent_);
        this->parent_->load_settings(false);
        active_initialized_ = true;
        ESP_LOGD(TAG, "START processing device %s, address %s", d->device_type().c_str(), d->get_address().c_str());
      }
      bool res = d->process(this);
      if (res == false) {
        ESP_LOGD(TAG, "FINISH processing device %s, address %s", d->device_type().c_str(), d->get_address().c_str());
        active_device_ = get_next_active(active_device_ + 1);
        active_initialized_ = false;
      }
    } else {
      active_device_ = -1;
    }
  }
}

void CountersProcessor::send() {
  if (active_device_ == -1) {
    active_device_ = get_next_active(0);
    active_initialized_ = false;
  }
}

int CountersProcessor::get_next_active(int i) {
  if (i < devices_.size()) {
    for (int j = i; j < devices_.size(); j++) {
      if (devices_[j]->is_active()) {
        return j;
      }
    }
  }
  return -1;
}

void CountersProcessor::dump_config() {
  ESP_LOGCONFIG(TAG, "Counters processor");
  for (Device *d : devices_) {
    ESP_LOGCONFIG(TAG, "  Device type: %s, Address: %s", d->device_type().c_str(), d->get_address().c_str());
  }
}
} // namespace counters_processor
} // namespace esphome