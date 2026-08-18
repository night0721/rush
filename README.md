# 90s
Minimalist, customizable shell written in C99 with syntax highlighting and no dependency.

* Disclaimer: This project is for me to learn to write Unix syscalls, code might be inefficient, feel free to point out the mistakes and open a issue for that!

# Preview
[![Valid command](https://r2.e-z.host/3c62bb3a-a8a9-43f6-afd6-553646f51dc4/aqnpvvud.png)]()
[![Invalid command](https://r2.e-z.host/3c62bb3a-a8a9-43f6-afd6-553646f51dc4/xf80dq0b.png)]()

# Features
- Syntax highlighting on valid commands using ANSI colors
- History navigation using up and down keys with history command
- Support for environment variables
- Editing using left and right arrow keys
- !! to repeat last command
- Pipes
- autojump to directories
- stdin, stdout, stderr redirect
- Background jobs
- Underline file path if it exists in system

## Built in commands
- cd
- help
- exit
- history
- export
- source
- j
- bg

## Todo Features
- Tab completion
- Git integration
- Allow arguments with space in double quotes
- Aliases

# Usage
```sh
90s

# > to redirect stdout
# < to redirect stdin
# 2> to redirect stderr
# >> to append to file
# & to run command in background
# | to pipe
# !! to repeat last command
# >& to redirect both stdout and stderr
```

# Dependencies
None

# Building
You will need to run these with elevated privilages.
```
$ make
# make install
```

# Notes
- History is either saved in HOME or XDG_CONFIG_HOME if it is defined

# Contributions
Contributions are welcomed, feel free to open a pull request.

# License
This project is licensed under the GNU Public License v3.0. See [LICENSE](https://github.com/night0721/90s/blob/master/LICENSE) for more information.

# Credits
- [Tutorial - Write a shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/)
- [dash](https://github.com/danishprakash/dash)
- [Shell assignment](https://www.cs.cornell.edu/courses/cs414/2004su/homework/shell/shell.html)
- [khol](https://github.com/SanketDG/khol/)
