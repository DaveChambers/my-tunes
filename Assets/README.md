# Assets

Branding assets used by the build:

| File                | Purpose                                                                 |
|---------------------|-------------------------------------------------------------------------|
| `TuneListIcon.png`  | macOS bundle icon. PNG with alpha, ideally ≥ 512×512.                   |

The CMake setup detects `Assets/TuneListIcon.png` automatically: present
→ JUCE bakes it into `TuneList.app/Contents/Resources/Icon.icns` and
wires `CFBundleIconFile` in Info.plist at configure time; absent → the
build still works, just without a custom icon. After replacing the
file, re-run `cmake -S . -B build && cmake --build build -j`.

For Projucer/Xcode users: `TuneList.jucer` already references this
file as a resource and points the *Icon (small)* / *Icon (large)*
fields at it, so just "Save Project and Open in IDE".
