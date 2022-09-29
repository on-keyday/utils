# minlc - minilang transpiler
minlc is a transpiler which converts minilang to c language

## What minilang
minilang is a programming language which I develop.

this is a toy language and would not support for practical purpose

# Impact from other language
- Go: if/for statement style, define operator
- Rust: fn keyword
- C/C++: operators, linkage

## Development status

- [x] minilang parser
- [ ] parse result to minilang object
- [ ] minilang object to c lang object
- [ ] c lang object to c lang source code
- [ ] compiler toolchain invocation

## Future
I want minlc to be transpiler which converts minilang to direct llvm. BUT cost of learning llvm is too high for me to make compiler, this plan will put off
