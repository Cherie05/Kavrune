# Contributing to Kavrune

Kavrune is an original C++20 Linux desktop and CLI project in development preview. Contributions should make USB workflows understandable and preserve the storage safety checks. Follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Issues and proposals

Use the published repository's issue tracker for ordinary bugs and feature requests. Describe the current behavior, expected behavior, Kavrune version or commit, distribution and relevant logs with private paths and device identifiers removed. Use synthetic files or a disposable VM for reproduction. Report vulnerabilities privately using [SECURITY.md](SECURITY.md).

Discuss broad design changes before implementation. Explain how storage changes handle cancellation, unplugging, identity changes, authorization errors and ambiguous device ancestry. A user interface change should keep destructive confirmation understandable and accessible.

## Development and validation

See the [README](README.md) for distribution packages and the full desktop build. For a core/CLI build without GTK development headers:

```sh
cmake -S . -B build -DKAVRUNE_BUILD_DESKTOP=OFF -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

This build still requires C++20, CMake, OpenSSL and GLib/GIO development packages. For changes to parsing, streaming or storage validation, also run the sanitizer configuration:

```sh
cmake -S . -B build-asan -DKAVRUNE_BUILD_DESKTOP=OFF \
  -DBUILD_TESTING=ON -DKAVRUNE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

For desktop changes, build with `./scripts/build.sh` and run `./build/kavrune --smoke-test` in a graphical session. Include a screenshot when it helps reviewers assess a visible change.

Automated write tests must use newly created temporary regular files. Do not point tests at `/dev` devices, format a developer's attached drives, require root or broaden authorization policies. Physical-media tests belong in the [disposable-device release matrix](docs/VALIDATION.md), with explicit control over the target and its contents. Test images must be synthetic or lawfully obtained; do not commit operating-system ISOs or personal drive backups.

## Pull requests

Create a branch from `main`, keep the change focused and open a pull request against `main`. Describe the problem, resulting behavior, validation performed and any remaining limits. Add meaningful regression coverage for storage or parser behavior, update affected documentation, and ensure required CI checks pass. Do not claim successful hardware validation based on regular-file tests.

Follow the existing C++ style, prefer bounded operations and explicit errors, and keep privileged work in the documented UDisks2/Polkit interfaces. Avoid adding shell execution, internal-disk overrides or silently relaxed safety checks.

## License and provenance

Submit only work you have the right to contribute. Contributions to original project files are provided under the [MIT License](LICENSE). By contributing, you confirm that you can license your submitted work under those terms. This project does not require a separate contributor license agreement.

Declare external code or assets, their upstream source, version and license in the pull request and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). Preserve required notices. Write new project code independently. Include external material only with verified licensing and an agreed reason to include it. Do not submit secrets, private logs, generated build outputs or downloaded dependencies.
