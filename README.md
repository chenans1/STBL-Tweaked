Simple Timed Block with some tweaks and additional features, notably hit stop, random stagger (from simple timed block addons) and projectile timed block. 

## Quick start

```bash
git clone https://github.com/<you>/SKSE-Plugin-Template
cd SKSE-Plugin-Template
git submodule add -b ng https://github.com/alandtse/CommonLibVR.git extern/CommonLibVR-ng
git submodule update --init --recursive
cmake -B build -S .
cmake --build build --config Release
