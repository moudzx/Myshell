# Myshell
A minimal Unix shell written in C, supporting pipes, redirections, built-in commands, background jobs, and basic environment variable expansion.

## Features
 
- Interactive prompt with current working directory
- Command execution via `fork` + `execvp`
- Built-in commands: `cd`, `exit`, `help`, `pwd`, `export`
- Pipes: `cmd1 | cmd2 | cmd3`
- Redirection: `cmd > file`, `cmd >> file`, `cmd < file`
- Background execution: `cmd &`
- Basic `$VAR` environment variable expansion
- Ctrl+C is ignored by the shell itself (only affects foreground children)


## Usage examples
 
```
myshell:/home/user$ pwd
/home/user
myshell:/home/user$ echo hello | grep hell
hello
myshell:/home/user$ ls > files.txt
myshell:/home/user$ cat < files.txt
myshell:/home/user$ export NAME=world
myshell:/home/user$ echo $NAME
world
myshell:/home/user$ sleep 10 &
[background pid 12345]
myshell:/home/user$ exit
```
<img width="3070" height="1636" alt="output" src="https://github.com/user-attachments/assets/048fc5dc-69d9-4738-b7e2-72c991248a9b" />
