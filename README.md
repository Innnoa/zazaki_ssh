# zazaki_ssh

A customized SSH remote command execution tool with structured output, built on libssh.

## Features

- Remote command execution (`exec`) with `--json` structured output
- Interactive shell (`connect`)
- File transfer (`copy`) via SFTP
- YAML-based profile configuration
- Catppuccin Mocha terminal theme
- Stream-based backpressure handling
- Cross-platform design (Linux epoll/PTY, Windows IOCP/ConPTY)

## Quick Start

```bash
# build
cmake -S . -B build -DZSSH_BUILD_TESTS=ON
cmake --build build

# configure
cat > zssh.yaml <<EOF
defaults:
  stdout_high_watermark_bytes: 262144
  stderr_high_watermark_bytes: 262144
  stdin_low_watermark_bytes: 65536
profiles:
  myserver:
    host: your-host
    auth:
      method: publickey
      username: your-user
      use_agent: true
EOF

# use
./build/zssh exec myserver hostname
./build/zssh exec myserver "uname -a" --json
./build/zssh connect myserver
./build/zssh copy myserver ./local.txt /tmp/remote.txt
```

## Build Requirements

- C++20 compiler
- CMake >= 3.24
- libssh
- yaml-cpp
- nlohmann/json
- GoogleTest (fetched automatically)

## Commands

| Command   | Status | Description |
|-----------|--------|-------------|
| `exec`    | Done   | Remote command execution |
| `connect` | Done   | Interactive shell |
| `copy`    | Done   | File transfer via SFTP |
| `config`  | Done   | Profile inspection |
| `keygen`  | Stub   | Keypair generation |
| `proxy`   | Stub   | Port forwarding |
