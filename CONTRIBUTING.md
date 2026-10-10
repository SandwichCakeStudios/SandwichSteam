# Contributing to Sandwich Steam

Thanks for helping! Bug reports, doc fixes and pull requests are all welcome.

## Reporting a bug
Open an issue with the **Bug report** template. The three things that make a bug fixable:
1. Your Unreal Engine version and operating system.
2. The output of `Steam.Core.Dump` (run it in a **Standalone** game, not Play In Editor).
3. The relevant part of the Output Log (filter by `LogSandwichSteam`).

## Improving the docs
The documentation site lives in [`docs/`](docs/). Every page is a plain Markdown file, so you can fix a typo straight from the GitHub web editor.
Screenshots are tracked by an ID such as `SHOT 07`. If you have a better screenshot, replace the PNG with the same file name.

## Code contributions
- Target: Unreal Engine 5.8.
- Follow [Epic's coding standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine).
- Every new `.h` / `.cpp` under `Source/`
- Each feature is its own runtime module (`SandwichSteam<Feature>`). The core module `SandwichSteam` must never depend on a feature module.
- Prefer composition over deep inheritance, no raw `new` / `delete`, gameplay tags where they make sense.
- Steam callbacks arrive on a non-game thread. Never touch a `UObject` from one directly; go through the plugin's callback dispatcher.
- Do not copy code from Epic's engine source or from other plugins.

By contributing you agree that your contribution is released under the [MIT License](LICENSE).
