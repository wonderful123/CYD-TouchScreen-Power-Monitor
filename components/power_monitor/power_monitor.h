#pragma once

#include "esphome/core/component.h"
#include "model/config.h"
#include "model/clock.h"
#include "model/live.h"
#include "model/peaks.h"
#include "model/history.h"
#include "model/sampler.h"
#include "model/stats.h"
#include "ble/decoder.h"
#include "ble/ingest.h"
#include "ui/theme.h"
#include "ui/format.h"
#include "ui/status.h"
#include "ui/plot.h"
#include "ui/views/chrome.h"
#include "ui/views/overview.h"
#include "ui/views/scope.h"
#include "ui/views/stats.h"
#include "ui/views/settings.h"

namespace esphome {
namespace power_monitor {

class PowerMonitorComponent : public Component {
 public:
  void setup() override {}
  void loop() override {}
};

}  // namespace power_monitor
}  // namespace esphome
