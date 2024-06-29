# BJIT

### Introduction

I recently had an empty day, with the only plans being relaxation. I quickly got bored. So I decided to do the next best thing.
Write a JIT compiler targeting ARM64.
I was able to get it into a working state in around ~24 hours. There might be some bugs, but I plan to clean this up later.

### Features

✔️ ARM64 JIT, for maximum performance
✔️ Support user input via ',' syntax
🚧 Support other architectures
🚧 Optimize compiled instructions

### JIT Status
This project might not fit the true definition of a Just-in-Time Compiler.
The code reads a BF file character by character, than compiles the program and executes the resulting instructions.
I think this might make BJIT fall under the category of a AOT (Ahead-Of-Time) Compiler.

### How does a JIT work?

Coming soon...
