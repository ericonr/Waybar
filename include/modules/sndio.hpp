#pragma once

#include <sndio.h>
#include <vector>
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Sndio : public ALabel {
 public:
  Sndio(const std::string&, const Json::Value&);
  ~Sndio();
  auto update() -> void;
  auto set_desc(struct sioctl_desc *, unsigned int) -> void;
  auto put_val(unsigned int, unsigned int) -> void;

 private:
  util::SleeperThread thread_;
  struct sioctl_hdl *hdl_;
  unsigned int addr_;
  double maxval_;
  std::vector<struct pollfd> pfds_;
  uint16_t volume_;
};

}  // namespace waybar::modules
