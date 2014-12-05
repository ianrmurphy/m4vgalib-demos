#include "etl/armv7m/exception_table.h"

#include "etl/armv7m/crt0.h"

#include "vga/timing.h"
#include "vga/vga.h"

#include "demo/wipe/wipe.h"

__attribute__((noreturn))
void etl_armv7m_reset_handler() {
  etl::armv7m::crt0_init();
  vga::init();
  vga::configure_timing(vga::timing_vesa_800x600_60hz);

  while (true) {
    demo::wipe::run();
  }
}
