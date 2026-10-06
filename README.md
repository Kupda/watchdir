# watchdir

Linux directory watcher built on inotify (C++17).

Prints a timestamped log of what happens in a directory:

```
[06.10.2026 12:54:57] created: n.txt
[06.10.2026 12:55:01] modified: n.txt
[06.10.2026 12:55:10] deleted: n.txt
```

## Build

Requires Linux, g++ and CMake.

```
cmake -S . -B build
cmake --build build
```

## Usage

```
./build/watchdir [path]
```

Without an argument it watches the current directory.

## Features

- `created` / `deleted` events
- `modified` is reported only if the file content really changed (FNV-1a hash comparison)
- editor swap files (`*.swp`) are ignored
- builds with AddressSanitizer and UndefinedBehaviorSanitizer

## Development

The project has a Dev Container (`.devcontainer`), so it can be opened in VS Code on any OS.

## What I learned

- One `read()` on an inotify descriptor can return several events of variable size, so the buffer has to be walked event by event.
- `IN_MODIFY` fires on every write, and editors like nano write a file in several steps. `IN_CLOSE_WRITE` fires once when writing is finished.
- `IN_CLOSE_WRITE` also fires for `touch` and for saving without changes, so a content hash is used to detect real modifications.
- On a folder shared from macOS into Docker, events can be duplicated. Testing in a native Linux folder (`/tmp`) gives correct results.