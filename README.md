# rush

rush is a minimalistic shell for Unix systems written in C.

* Disclaimer: This project is for me to learn to write Unix syscalls, code might be inefficient, feel free to point out the mistakes and open a issue for that!

# Preview
[![Valid command](https://r2.e-z.host/3c62bb3a-a8a9-43f6-afd6-553646f51dc4/aqnpvvud.png)]()
[![Invalid command](https://r2.e-z.host/3c62bb3a-a8a9-43f6-afd6-553646f51dc4/xf80dq0b.png)]()

# Dependencies
- gcc

# Building
```sh
$ make
$ sudo make install
```

# Usage
```sh
$ ./rush
```

# Features
- No dependencies except a compiler
- Showing current time and directory with custom color
- Syntax highlighting on valid commands using ANSI colors
- History navigation using up and down keys with history command
- Support for environment variables
- Editing using left and right arrow keys
- !! to repeat last command
- Pipes
- autojump to directorys
- stdin, stdout, stderr redirect
- background jobs

# Built in commands
- cd
- help
- exit
- history
- export
- source
- j
- bg

# Todo Features
- tab completion

# Credits
- [Tutorial - Write a shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/)
- [dash](https://github.com/danishprakash/dash)
- [Shell assignment](https://www.cs.cornell.edu/courses/cs414/2004su/homework/shell/shell.html)
- [khol](https://github.com/SanketDG/khol/)
