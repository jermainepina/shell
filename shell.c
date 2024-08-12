// Shell:
// 1. Initialize: read and execute configuration files
// 2. Interpret: read commands from stdin (input/file) and executes them
// 3. Terminate: after its done, executes all shutdown commands and frees memory
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Function delcarations for builtin shell commands:
//
int lsh_cd(char** args);
int lsh_help(char** args);
int lsh_exit(char** args);

// List of built in commands followed by their corresponding function
char *builtin_str[] = {
    "cd",
    "help",
    "exit"
};

int (*builtin_func[]) (char**) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit
};

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char*);
}

// Implementations of builtin functions
int lsh_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) perror("lsh");
    }
    return 1;
}

int lsh_help(char **args) {
    printf("Jermaine's LSH\n");
    printf("Type program names and arguments, and hit enter\n");

    // function list
    printf("The following are built in:\n");
    for (int i = 0; i < lsh_num_builtins(); i++) {
        printf(" %s\n", builtin_str[i]);
    }
    printf("Use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit(char** args) {
    return 0;
}

// Reads line of input
// Reads in characters and null terminates when newline or EOF
// If input is larger than buffsize, realloc memory
// Can be simplified with getline()
// #define LSH_RL_BUFSIZE 1024
// char* sh_read_line() {
//     int buffsize = LSH_RL_BUFSIZE;
//     int position = 0;
//     char* buffer = malloc(sizeof(char) * buffsize);
//     int c;

//     if (!buffer) {
//         fprintf(stderr, "lsh: allocation error\n");
//         exit(EXIT_FAILURE);
//     }

//     while(1) {
//         c = getchar();
//         if (c == EOF || c == '\n') {
//             buffer[position] = '\0';
//             return buffer;
//         } else {
//             buffer[position] = c;
//         }
//         position++;

//         if (position >= buffsize) {
//             buffsize += LSH_RL_BUFSIZE;
//             buffer = realloc(buffer, buffsize);
//             if (!buffer) {
//                 fprintf(stderr, "lsh: allocation error\n");
//                 exit(EXIT_FAILURE);
//             }
//         }
//     }
// }

// Uses getline to read lines of input
char* lsh_read_line(void) {
    char *line = NULL;
    size_t buffsize = 0;
    if (getline(&line, &buffsize, stdin) == -1) {
        if (feof(stdin)) exit(EXIT_SUCCESS);
        else {
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

// Doesnt allow quoting and backslash escaping
// Whitespace is used to seperate arguments
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char** lsh_split_line(char* line) {
    int buffsize = LSH_TOK_BUFSIZE, pos = 0;
    char **tokens = malloc(buffsize * sizeof(char*));
    char* token;

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    // Uses strtok to create tokens with whitespace as delimiter
    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[pos] = token;
        pos++;

        // Adjust size of tokens if it exceeds length of buffer
        if (pos >= buffsize) {
            buffsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, buffsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        // Move on to the next token (NULL tells strtok to continue
        // where it left off)
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[pos] = NULL;
    return tokens;
}

// INFO:
//
// Since most programs aren’t Init, that leaves only one practical way for
// processes to get started: the fork() system call. When this function is
// called, the operating system makes a duplicate of the process and starts
// them both running. The original process is called the “parent”, and the
// new one is called the “child”. fork() returns 0 to the child process,
// and it returns to the parent the process ID number (PID) of its child.
// In essence, this means that the only way for new processes is to start
// is by an existing one duplicating itself.

// This might sound like a problem. Typically, when you want to run a new
// process, you don’t just want another copy of the same program – you want
// to run a different program. That’s what the exec() system call is all
// about. It replaces the current running program with an entirely new one.
// This means that when you call exec, the operating system stops your
// process, loads up the new program, and starts that one in its place. A
// process never returns from an exec() call (unless there’s an error).

// With these two system calls, we have the building blocks for how most
// programs are run on Unix. First, an existing process forks itself into
// two separate ones. Then, the child uses exec() to replace itself with a
// new program. The parent process can continue doing other things, and it
// can even keep tabs on its children, using the system call wait().


// Launches the process by forking existing process and
// replacing its child with the new process
int lsh_launch(char **args) {
    // Process ID
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid == 0) {
        // child process
        if (execvp(args[0], args) == -1) {
            // error executing
            perror("lsh");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // error forking
        perror("lsh");
    } else {
        // parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int lsh_execute(char** args) {
    if (args[0] == NULL) return 1;
    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0)
            return (*builtin_func[i])(args);
    }
    return lsh_launch(args);
}

void lsh_loop(void) {
    char* line;
    char** args;
    int status;

    do {
        printf("> ");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(args);
        free(line);
    } while (status);
}

int main(int argc, char** argv) {
    // Load config files, if any

    // Run command loop
    lsh_loop();

    // Perform shutdown/cleanup
    return EXIT_SUCCESS;
}
