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
    : ALabel(config, "sndio", id, "{volume}%"),
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

  pfds_.reserve(sioctl_nfds(hdl_));

  event_box_.show();

  thread_ = [this] {
    dp.emit();

    int nfds = sioctl_pollfd(hdl_, pfds_.data(), POLLIN);
    if (nfds == 0) {
      throw std::runtime_error("sioctl_pollfd() failed.");
    }
    while (poll(pfds_.data(), nfds, -1) < 0) {
      if (errno != EINTR) {
        throw std::runtime_error("poll() failed.");
      }
    }

    int revents = sioctl_revents(hdl_, pfds_.data());
    if (revents & POLLHUP) {
      throw std::runtime_error("disconnected!");
    }
  };
}

Sndio::~Sndio() {
  sioctl_close(hdl_);
}

auto Sndio::update() -> void {
  auto format = format_;
  label_.set_markup(fmt::format(format,
                                fmt::arg("volume", volume_)));

  ALabel::update();
}

auto Sndio::set_desc(struct sioctl_desc *d, unsigned int val) -> void {
  std::string name{d->func};
  std::string node_name{d->node0.name};

  if (name == "level" && node_name == "output" && d->type == SIOCTL_NUM) {
    // store addr for output.level value, used in put_val
    addr_ = d->addr;
    maxval_ = static_cast<double>(d->maxval);

    auto fval = static_cast<double>(val);
    volume_ = fval / maxval_ * 100.;
  }
}

auto Sndio::put_val(unsigned int addr, unsigned int val) -> void {
  if (addr == addr_) {
    auto fval = static_cast<double>(val);
    volume_ = fval / maxval_ * 100.;
  }
}

} /* namespace waybar::modules */
