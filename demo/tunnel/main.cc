#include "lib/armv7m/exception_table.h"

#include "runtime/startup.h"

#include "vga/timing.h"
#include "vga/vga.h"

#include "demo/tunnel/tunnel.h"

__attribute__((noreturn))
void v7m_reset_handler() {
  crt_init();
  vga::init();
  vga::configure_timing(vga::timing_vesa_800x600_60hz);

  while (true) {
    demo::tunnel::run(0);
  }
}

