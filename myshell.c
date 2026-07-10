#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#define TOKEN_BUFSIZE 64
#define TOKEN_DELIM " \t\r\n\a"
#define LINE_BUFSIZE 1024
#define MAX_COMMANDS 32
#define MAX_ARGS 64
int sh_cd(char **args);
int sh_exit(char **args);
int sh_help(char **args);
int sh_pwd(char **args);
int sh_export(char **args);
char *builtin_str[] = {
"cd",
"exit",
"help",
"pwd",
"export"
};
int (*builtin_func[]) (char **) = {
&sh_cd,
&sh_exit,
&sh_help,
&sh_pwd,
&sh_export
};
int sh_num_builtins(void) {
return sizeof(builtin_str) / sizeof(char *);
}
int sh_cd(char **args) {
if (args[1] == NULL) {
char *home = getenv("HOME");
if (home == NULL) {
fprintf(stderr, "myshell: cd: HOME not set\n");
return 1;
}
if (chdir(home) != 0) perror("myshell: cd");
} else {
if (chdir(args[1]) != 0) {
perror("myshell: cd");
}
}
return 1;
}
int sh_exit(char **args) {
(void)args;
return 0;
}
int sh_help(char **args) {
(void)args;
printf("myshell - a simple C shell\n");
printf("Built-in commands:\n");
for (int i = 0; i < sh_num_builtins(); i++) {
printf("  %s\n", builtin_str[i]);
}
printf("Use pipes (|), redirection (< > >>), and background (&) as usual.\n");
printf("Type program names and arguments, then hit enter.\n");
return 1;
}
int sh_pwd(char **args) {
(void)args;
char cwd[1024];
if (getcwd(cwd, sizeof(cwd)) != NULL) {
printf("%s\n", cwd);
} else {
perror("myshell: pwd");
}
return 1;
}
int sh_export(char **args) {
if (args[1] == NULL) {
fprintf(stderr, "myshell: export: usage: export NAME=VALUE\n");
return 1;
}
char *eq = strchr(args[1], '=');
if (eq == NULL) {
fprintf(stderr, "myshell: export: invalid format, use NAME=VALUE\n");
return 1;
}
*eq = '\0';
if (setenv(args[1], eq + 1, 1) != 0) {
perror("myshell: export");
}
*eq = '=';
return 1;
}
char *sh_read_line(void) {
size_t bufsize = LINE_BUFSIZE;
char *line = malloc(bufsize);
if (!line) {
fprintf(stderr, "myshell: allocation error\n");
exit(EXIT_FAILURE);
}
if (getline(&line, &bufsize, stdin) == -1) {
if (feof(stdin)) {
free(line);
return NULL;
} else {
perror("myshell: getline");
exit(EXIT_FAILURE);
}
}
return line;
}
char *expand_variables(const char *token) {
if (token[0] != '$' || token[1] == '\0') {
return strdup(token);
}
char *val = getenv(token + 1);
return strdup(val ? val : "");
}
char **sh_split_args(char *line) {
int bufsize = TOKEN_BUFSIZE;
int position = 0;
char **tokens = malloc(bufsize * sizeof(char *));
char *token;
if (!tokens) {
fprintf(stderr, "myshell: allocation error\n");
exit(EXIT_FAILURE);
}
token = strtok(line, TOKEN_DELIM);
while (token != NULL) {
tokens[position] = expand_variables(token);
position++;
if (position >= bufsize) {
bufsize += TOKEN_BUFSIZE;
char **new_tokens = realloc(tokens, bufsize * sizeof(char *));
if (!new_tokens) {
fprintf(stderr, "myshell: allocation error\n");
exit(EXIT_FAILURE);
}
tokens = new_tokens;
}
token = strtok(NULL, TOKEN_DELIM);
}
tokens[position] = NULL;
return tokens;
}
typedef struct {
char **args;
char *infile;
char *outfile;
int append;
} Command;
void parse_redirection(Command *cmd) {
char **args = cmd->args;
int read_idx = 0, write_idx = 0;
while (args[read_idx] != NULL) {
if (strcmp(args[read_idx], "<") == 0) {
read_idx++;
if (args[read_idx] != NULL) {
cmd->infile = args[read_idx];
read_idx++;
}
} else if (strcmp(args[read_idx], ">") == 0) {
read_idx++;
if (args[read_idx] != NULL) {
cmd->outfile = args[read_idx];
cmd->append = 0;
read_idx++;
}
} else if (strcmp(args[read_idx], ">>") == 0) {
read_idx++;
if (args[read_idx] != NULL) {
cmd->outfile = args[read_idx];
cmd->append = 1;
read_idx++;
}
} else {
args[write_idx++] = args[read_idx++];
}
}
args[write_idx] = NULL;
}
int sh_split_pipeline(char *line, Command commands[], int *background) {
int num_commands = 0;
*background = 0;
char *amp = strrchr(line, '&');
if (amp != NULL) {
char *rest = amp + 1;
int only_whitespace = 1;
for (; *rest; rest++) {
if (*rest != ' ' && *rest != '\t' && *rest != '\n') {
only_whitespace = 0;
break;
}
}
if (only_whitespace) {
*amp = '\0';
*background = 1;
}
}
char *saveptr;
char *segment = strtok_r(line, "|", &saveptr);
while (segment != NULL && num_commands < MAX_COMMANDS) {
char *seg_copy = strdup(segment);
commands[num_commands].args = sh_split_args(seg_copy);
commands[num_commands].infile = NULL;
commands[num_commands].outfile = NULL;
commands[num_commands].append = 0;
parse_redirection(&commands[num_commands]);
num_commands++;
segment = strtok_r(NULL, "|", &saveptr);
}
return num_commands;
}
int sh_launch_pipeline(Command commands[], int num_commands, int background) {
int i;
int in_fd = 0;
pid_t pids[MAX_COMMANDS];
int pipefd[2];
for (i = 0; i < num_commands; i++) {
int has_next = (i < num_commands - 1);
if (has_next) {
if (pipe(pipefd) < 0) {
perror("myshell: pipe");
return 1;
}
}
pid_t pid = fork();
if (pid == 0) {
signal(SIGINT, SIG_DFL);
if (in_fd != 0) {
dup2(in_fd, STDIN_FILENO);
close(in_fd);
}
if (commands[i].infile != NULL) {
int fd = open(commands[i].infile, O_RDONLY);
if (fd < 0) {
perror("myshell: open (infile)");
exit(EXIT_FAILURE);
}
dup2(fd, STDIN_FILENO);
close(fd);
}
if (has_next) {
dup2(pipefd[1], STDOUT_FILENO);
close(pipefd[1]);
close(pipefd[0]);
}
if (commands[i].outfile != NULL) {
int flags = O_WRONLY | O_CREAT | (commands[i].append ? O_APPEND : O_TRUNC);
int fd = open(commands[i].outfile, flags, 0644);
if (fd < 0) {
perror("myshell: open (outfile)");
exit(EXIT_FAILURE);
}
dup2(fd, STDOUT_FILENO);
close(fd);
}
if (commands[i].args[0] == NULL) {
exit(EXIT_SUCCESS);
}
if (execvp(commands[i].args[0], commands[i].args) == -1) {
fprintf(stderr, "myshell: %s: command not found\n", commands[i].args[0]);
exit(EXIT_FAILURE);
}
} else if (pid < 0) {
perror("myshell: fork");
return 1;
} else {
pids[i] = pid;
}
if (in_fd != 0) close(in_fd);
if (has_next) {
close(pipefd[1]);
in_fd = pipefd[0];
}
}
if (background) {
printf("[background pid %d]\n", pids[num_commands - 1]);
return 1;
}
int status;
for (i = 0; i < num_commands; i++) {
waitpid(pids[i], &status, 0);
}
return 1;
}
int sh_execute(char *line) {
Command commands[MAX_COMMANDS];
int background = 0;
char *p = line;
while (*p == ' ' || *p == '\t') p++;
if (*p == '\0' || *p == '\n') return 1;
int num_commands = sh_split_pipeline(line, commands, &background);
if (num_commands == 0) return 1;
if (num_commands == 1 && commands[0].args[0] != NULL) {
for (int i = 0; i < sh_num_builtins(); i++) {
if (strcmp(commands[0].args[0], builtin_str[i]) == 0) {
int result = (*builtin_func[i])(commands[0].args);
free(commands[0].args);
return result;
}
}
}
int result = sh_launch_pipeline(commands, num_commands, background);
for (int i = 0; i < num_commands; i++) {
free(commands[i].args);
}
return result;
}
int main(void) {
char *line;
int status = 1;
signal(SIGINT, SIG_IGN);
printf("myshell v1.0 - type 'help' for info, 'exit' to quit\n");
do {
char cwd[1024];
if (getcwd(cwd, sizeof(cwd)) != NULL) {
printf("myshell:%s$ ", cwd);
} else {
printf("myshell$ ");
}
fflush(stdout);
line = sh_read_line();
if (line == NULL) {
printf("\n");
break;
}
char *trimmed = line;
while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
if (*trimmed == '#' || *trimmed == '\n' || *trimmed == '\0') {
free(line);
continue;
}
char *line_copy = strdup(line);
char *first_word = strtok(line_copy, TOKEN_DELIM);
if (first_word != NULL && strcmp(first_word, "exit") == 0) {
free(line_copy);
free(line);
break;
}
free(line_copy);
status = sh_execute(line);
free(line);
} while (status);
return EXIT_SUCCESS;
}
