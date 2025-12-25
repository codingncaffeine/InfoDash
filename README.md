# InfoDash

A modern GTK4 dashboard application for Linux that displays RSS feeds, weather, and stock data.

## Dependencies (Arch/EndeavourOS)

```bash
sudo pacman -S gtk4 libcurl libxml2 json-glib cmake base-devel
```

## Build

```bash
cd InfoDash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Run

```bash
./InfoDash
```

## Configuration

Config stored at `~/.config/infodash/config.json`
