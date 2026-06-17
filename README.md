# hyprland-noscreen-blur-plugin (`blurcap`)

A [Hyprland](https://hyprland.org/) plugin that **blurs windows in screen
captures instead of blacking them out**.

Hyprland already lets you hide a window from screen sharing with the
`no_screen_share = true` window rule — but it paints a solid **black box** over
the window in the captured stream. This plugin replaces that black box with a
**blur**: the window stays fully visible on your real monitor, but anyone
watching your screencast/recording sees only a blurred smudge where the window is.

Great for keeping a messenger, password manager, or any sensitive window
readable to you while it's unreadable in a call/recording.

![demo placeholder](https://via.placeholder.com/800x200?text=before%3A+black+box+%E2%86%92+after%3A+blur)

## How it works

Screen capture in Hyprland runs in a separate render pass
(`CScreenshareFrame::renderMonitor`) that draws a mirror texture of your monitor
into the capture buffer and then fills a black rectangle over every window
flagged with `no_screen_share`.

`blurcap` installs two function hooks:

1. **`CScreenshareFrame::renderMonitor`** — wraps the call to mark "a capture
   frame is being rendered right now".
2. **`CHyprRenderer::draw(SRectData)`** — while a capture frame is rendering, any
   opaque-black rectangle is turned into a `blur = true` rectangle. The same box
   now shows a blur in the captured stream instead of a solid black box.

Because it reuses the native `no_screen_share` rule, there is **no plugin-specific
config** — you flag windows the normal Hyprland way and they get blurred.

### Notes & limitations

- Targets **video capture / screen-sharing** (PipeWire, OBS, `wf-recorder`, etc.),
  where the monitor mirror buffer is continuously refreshed. Single static
  screenshots (`grim`) are generally copied from a cached buffer and are not
  affected — by design (you control a one-off screenshot yourself anyway).
- **What the blur actually shows:** in full-screen video capture the blur samples
  the monitor's background layer in the window's area, so you see a *blurred
  smudge of the wallpaper* where the window is — not a blur of the window's own
  pixels. The sensitive content is still fully hidden; it just reads as blurred
  wallpaper rather than a blurred window. (Region/window-scoped screenshots blur
  the real window content instead — the two capture paths feed the blur different
  buffers; making video blur the window content reliably is an open problem.)
- Blur strength is whatever your global `decoration:blur:size` / `passes` are set
  to. There is currently no per-plugin strength knob.
- Hooks into Hyprland's **internal** API, so it is tied to the Hyprland version it
  was built against. Rebuild after every Hyprland update.

## Requirements

- Hyprland **0.55.x** (developed and tested on `0.55.4`).
- Hyprland development headers available to `pkg-config` (the `hyprland.pc`
  installed by `hyprpm` or your distro's `hyprland`/`hyprland-headers` package).
- A C++26-capable compiler (tested with `g++ 16`).
- `pkg-config`, `pixman`, `libdrm`.

## Build

```sh
git clone https://github.com/demoj1/hyprland-noscreen-blur-plugin.git
cd hyprland-noscreen-blur-plugin
make
```

This produces `blurcap.so`.

> The build uses `pkg-config --cflags hyprland pixman-1 libdrm`. If `pkg-config`
> can't find `hyprland`, make sure the headers for your **exact** Hyprland version
> are installed (e.g. via `hyprpm` once, or your distro's headers package).

## Install / load

### Quick (manual, good for trying it out)

```sh
hyprctl plugin load "$PWD/blurcap.so"
# ...
hyprctl plugin unload "$PWD/blurcap.so"
```

### Persistent (load on Hyprland start)

Add to your `hyprland.conf`:

```ini
exec-once = hyprctl plugin load /absolute/path/to/blurcap.so
```

(Or manage it with [`hyprpm`](https://wiki.hyprland.org/Plugins/Using-Plugins/)
if you prefer; this repo ships a plain `Makefile`.)

## Usage

Flag any window you want blurred in captures with the standard rule:

```ini
windowrule {
    name = telegram-privacy
    match {
        class = .*telegram.*
    }
    no_screen_share = true
}
```

That's it. On your monitor Telegram looks normal; in a screencast it's blurred.

You can verify with a quick recording:

```sh
wf-recorder -f /tmp/test.mp4   # share/record something with the flagged window visible
```

## License

MIT — see [LICENSE](LICENSE).
