#!/bin/bash

## This script is used by the Makefile to have the correct ambient variables when compiling `kcomp` on macOS.

export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
export LDFLAGS="-L/opt/homebrew/opt/llvm/lib"
export CPPFLAGS="-I/opt/homebrew/opt/llvm/include"
