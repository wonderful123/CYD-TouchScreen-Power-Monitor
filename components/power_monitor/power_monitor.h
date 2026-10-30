#pragma once

#include "esphome/core/component.h"
namespace esphome {
namespace power_monitor {

class PowerMonitorComponent : public Component {
 public:
  void setup() override;
  void loop() override {}
  const char *get_reset_reason() const;
};

}  // namespace power_monitor
}  // namespace esphome