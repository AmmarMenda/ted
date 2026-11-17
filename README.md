# TED

Minimal TUI text editor in C for Linux.

## Build

```sh
sudo apt install libtree-sitter-dev          # Debian/Ubuntu
# or: sudo pacman -S tree-sitter            # Arch
# or: brew install tree-sitter              # macOS

git clone --depth 1 https://github.com/tree-sitter/tree-sitter-c.git deps/tree-sitter-c
git clone --depth 1 https://github.com/tree-sitter/tree-sitter.git   deps/tree-sitter

make
sudo make install
