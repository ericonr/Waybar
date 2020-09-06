#include "modules/sndio.hpp"
#include <cstdlib>
#include <poll.h>
#include <fmt/format.h>

namespace waybar::modules {

void ondesc(void *arg, struct sioctl_desc *d, int curval) {
  auto self = static_cast<Sndio*>(arg);
  if (d == NULL) {
    // d is NULL when the list is done
    return;
  }
  self->set_desc(d, curval);
}

void onval(void *arg, unsigned int addr, unsigned int val) {
  auto self = static_cast<Sndio*>(arg);
  self->put_val(addr, val);
}

Sndio::Sndio(const std::string &id, const Json::Value &config)
    : ALabel(config, "sndio", id, "{volume}%", 2),
      hdl_(nullptr),
      addr_(0),
      maxval_(0),
      pfds_(0),
      volume_(0) {
  hdl_ = sioctl_open(SIO_DEVANY, SIOCTL_READ, 0);
  if (hdl_ == nullptr) {
    throw std::runtime_error("sioctl_open() failed.");
  }

  if(sioctl_ondesc(hdl_, ondesc, this) == 0) {
    throw std::runtime_error("sioctl_ondesc() failed.");
  }

  sioctl_onval(hdl_, onval, this);

  pfds_ = static_cast<struct pollfd *>(malloc(sizeof(struct pollfd) * sioctl_nfds(hdl_)));
  if (pfds_ == nullptr) {
    throw std::runtime_error("sioctl_nfds() failed.");
  }

  event_box_.show();

  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto diff = std::chrono::seconds(timeout.time_since_epoch().count() % interval_.count());
    thread_.sleep_until(timeout - diff);
  };
}

Sndio::~Sndio() {
  sioctl_close(hdl_);
}

auto Sndio::update() -> void {
  int nfds = sioctl_pollfd(hdl_, pfds_, POLLIN);
  if (nfds == 0) return;
  while (poll(pfds_, nfds, 10) < 0) {
    if (errno != EINTR) {
      throw std::runtime_error("poll() failed.");
    }
  }
  int revents = sioctl_revents(hdl_, pfds_);
  if (revents & POLLHUP) {
    throw std::runtime_error("disconnected!");
  }

  auto format = format_;
  label_.set_markup(fmt::format(format,
                                fmt::arg("volume", volume_)));

  getState(volume_);
  event_box_.show();
  ALabel::update();
}

auto Sndio::set_desc(struct sioctl_desc *d, unsigned int val) -> void {
  std::string name{d->func};
  std::string node_name{d->node0.name};

  if (name == "level" && node_name == "output" && d->type == SIOCTL_NUM) {
    maxval_ = double{d->maxval};
    addr_ = d->addr;

    auto fval = double{val};
    volume_ = fval / maxval_ * 100.;
  }
}

auto Sndio::put_val(unsigned int addr, unsigned int val) -> void {
  if (addr == addr_) {
    auto fval = double{val};
    volume_ = fval / maxval_ * 100.;
  }
}

} /* namespace waybar::modules */
