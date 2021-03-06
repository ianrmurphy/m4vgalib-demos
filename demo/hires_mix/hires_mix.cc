#include "demo/hires_mix/hires_mix.h"

#include "etl/assert.h"
#include "etl/scope_guard.h"

#include "etl/armv7m/crt0.h"
#include "etl/armv7m/exception_table.h"

#include "vga/arena.h"
#include "vga/graphics_1.h"
#include "vga/timing.h"
#include "vga/vga.h"
#include "vga/rast/bitmap_1.h"

#include "demo/config.h"
#include "demo/input.h"
#include "demo/terminal.h"

DEMO_REQUIRE_RESOLUTION(800, 600)

using vga::rast::Bitmap_1;

using Pixel = vga::Rasterizer::Pixel;

namespace demo {
namespace hires_mix {

/*******************************************************************************
 * Screen partitioning
 */

static constexpr unsigned
  text_cols = 800,
  text_rows = 16 * 16,
  gfx_cols = 800,
  gfx_rows = 600 - text_rows;


/*******************************************************************************
 * The Graphics Parts
 */

class Particle {
public:
  static unsigned seed;

  void randomize() {
    _x[0] = _x[1] = rand() % gfx_cols;
    _y[0] = _y[1] = rand() % (gfx_rows * 2/3);
    _dx = rand() % 9 - 5;
    _dy = rand() % 3 - 2;
  }

  void nudge(int ddx, int ddy) {
    _dx += ddx;
    _dy += ddy;
  }

  void step(vga::Graphics1 &g) {
    g.clear_pixel(_x[1], _y[1]);
    g.clear_pixel(_x[1] - 1, _y[1]);
    g.clear_pixel(_x[1] + 1, _y[1]);
    g.clear_pixel(_x[1], _y[1] - 1);
    g.clear_pixel(_x[1], _y[1] + 1);

    _x[1] = _x[0];
    _y[1] = _y[0];

    int x_ = _x[0] + _dx;
    int y_ = _y[0] + _dy;

    if (x_ < 0) {
      x_ = 0;
      _dx = -_dx;
    }

    if (y_ < 0) {
      y_ = 0;
      _dy = -_dy;
    }

    if (x_ >= static_cast<int>(gfx_cols)) {
      x_ = gfx_cols - 1;
      _dx = -_dx;
    }

    if (y_ >= static_cast<int>(gfx_rows)) {
      y_ = gfx_rows - 1;
      _dy = -_dy;
    }

    _x[0] = x_;
    _y[0] = y_;

    g.set_pixel(_x[0], _y[0]);
    g.set_pixel(_x[0] - 1, _y[0]);
    g.set_pixel(_x[0] + 1, _y[0]);
    g.set_pixel(_x[0], _y[0] - 1);
    g.set_pixel(_x[0], _y[0] + 1);
  }

private:
  int _x[2], _y[2];
  int _dx, _dy;

  unsigned rand() {
    seed = (seed * 1664525) + 1013904223;
    return seed;
  }
};

unsigned Particle::seed = 1118;

struct GfxDemo {
  static constexpr unsigned particle_count = 500;

  Particle particles[particle_count];

  Bitmap_1 gfx_rast{gfx_cols, gfx_rows};

  GfxDemo() {
    gfx_rast.set_fg_color(0b111111);
    gfx_rast.set_bg_color(0b100000);

    if (!gfx_rast.can_bg_use_bitband()) {
      gfx_rast.flip_now();
      ETL_ASSERT(gfx_rast.can_bg_use_bitband());
    }

    gfx_rast.make_bg_graphics().clear_all();

    for (Particle &p : particles) p.randomize();
  }

  void update_particles() {
    vga::Graphics1 g = gfx_rast.make_bg_graphics();
    for (Particle &p : particles) {
      p.step(g);
      p.nudge(0, 1);
    }
    gfx_rast.copy_bg_to_fg();
  }
};


/*******************************************************************************
 * The Text Parts
 */

struct TextDemo : demo::Terminal {
  TextDemo() : demo::Terminal(text_cols, text_rows, gfx_rows) {}

  void startup_banner() {
    using namespace demo;  // for colors

    text_centered(0, white, dk_gray, "800x600 Mixed Graphics Modes Demo");
    cursor_to(0, 2);
    type(white, black, "Bitmap framebuffer combined with ");
    type(black, white, "attributed");
    type(red, black, " 256");
    type(green, black, " color ");
    type(blue, black, "text.");
    cursor_to(0, 4);
    rainbow_type("The quick brown fox jumped over the lazy dog. "
                 "0123456789!@#$%^{}");

    text_at(0, 6, white, 0b100000,
      "     Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam ut\n"
      "     tellus quam. Cras ornare facilisis sollicitudin. Quisque quis\n"
      "     imperdiet mauris. Proin malesuada nibh dolor, eu luctus mauris\n"
      "     ultricies vitae. Interdum et malesuada fames ac ante ipsum primis\n"
      "     in faucibus. Aenean tincidunt viverra ultricies. Quisque rutrum\n"
      "     vehicula pulvinar.\n");

    text_at(0, 15, white, black, "60 fps / 40MHz pixel clock");
    text_at(58, 36, white, black, "Frame number:");
  }
};


/*******************************************************************************
 * Combining Code
 */

struct SplitDemo {
  GfxDemo g;
  TextDemo t;

  vga::Band const bands[2] {
    { &g.gfx_rast,   gfx_rows,  &bands[1] },
    { &t.rasterizer, text_rows, nullptr },
  };
};

/*******************************************************************************
 * The Startup Routine
 */

void run() {
  vga::arena_reset();
  input_init();

  auto d = vga::arena_make<SplitDemo>();

  d->t.startup_banner();

  vga::configure_band_list(d->bands);
  ETL_ON_SCOPE_EXIT { vga::clear_band_list(); };

  vga::video_on();
  ETL_ON_SCOPE_EXIT { vga::video_off(); };

  char fc[9];
  fc[8] = 0;
  unsigned frame = 0;

  while (!user_button_pressed()) {
    unsigned f = ++frame;

    for (unsigned i = 8; i > 0; --i) {
      unsigned n = f & 0xF;
      fc[i - 1] = n > 9 ? 'A' + n - 10 : '0' + n;
      f >>= 4;
    }
    vga::sync_to_vblank();
    d->t.text_at(72, 15, demo::red, demo::black, fc);
    d->g.update_particles();
  }
}

}  // namespace hires_mix
}  // namespace demo
