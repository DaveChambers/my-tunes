# Assets

Drop branding assets here. Currently expected:

| File              | Purpose                                                                 |
|-------------------|-------------------------------------------------------------------------|
| `AppIcon.png`     | macOS bundle icon. PNG with alpha, ideally ≥ 512×512.                   |

The CMake setup detects `Assets/AppIcon.png` automatically: present →
JUCE bakes it into the .app bundle at configure time; absent → the
build still works, just without a custom icon. After dropping the
file, re-run `cmake -S . -B build && cmake --build build -j`.

For Projucer/Xcode users: open `MyTunes.jucer` and set the *Icon
(small)* / *Icon (large)* fields to the same `Assets/AppIcon.png`
path, then "Save Project and Open in IDE".
