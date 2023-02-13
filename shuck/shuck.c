////////////////////////////////////////////////////////////////////////
// COMP1521 21t2 -- Assignment 2 -- shuck, A Simple Shell
// <https://www.cse.unsw.edu.au/~cs1521/21T2/assignments/ass2/index.html>
//
// Written by Leo Shi (z5364321) on July - August 2021
//
// 2021-07-12    v1.0    Team COMP1521 <cs1521@cse.unsw.edu.au>
// 2021-07-21    v1.1    Team COMP1521 <cs1521@cse.unsw.edu.au>
//     * Adjust qualifiers and attributes in provided code,
//       to make `dcc -Werror' happy.

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

// [[ TODO: put any extra `#include's here ]]
#include <spawn.h>
#include <sys/wait.h>
#include <ctype.h>
#include <glob.h>
// [[ TODO: put any `#define's here ]]

/* 
    Shuck is a program designed to emulate a terminal. We can pass in 
    commands and shuck will carry out the function and return the 
    necessary output back to us. The terminal will continue to take in
    commands until the exit command is run or the terminal meets an error.
    The terminal is able to comprehend numerous functions, such as cd, pwd,
    echo, and has some of our own made up commands, such as history and ! 
    (to print out a specific line in history). Our terminal can also handle
    input/output redirections, where the output of one command is used as
    the input to another.
*/


//
// Interactive prompt:
//     The default prompt displayed in `interactive' mode --- when both
//     standard input and standard output are connected to a TTY device.
//
static const char *const INTERACTIVE_PROMPT = "shuck& ";

//
// Default path:
//     If no `$PATH' variable is set in Shuck's environment, we fall
//     back to these directories as the `$PATH'.
//
static const char *const DEFAULT_PATH = "/bin:/usr/bin";

//
// Default history shown:
//     The number of history items shown by default; overridden by the
//     first argument to the `history' builtin command.
//     Remove the `unused' marker once you have implemented history.
//
static const int DEFAULT_HISTORY_SHOWN __attribute__((unused)) = 10;

//
// Input line length:
//     The length of the longest line of input we can read.
//
static const size_t MAX_LINE_CHARS = 1024;

//
// Special characters:
//     Characters that `tokenize' will return as words by themselves.
//
static const char *const SPECIAL_CHARS = "!><|";

//
// Word separators:
//     Characters that `tokenize' will use to delimit words.
//
static const char *const WORD_SEPARATORS = " \t\r\n";

// [[ TODO: put any extra constants here ]]


// [[ TODO: put any type definitions (i.e., `typedef', `struct', etc.) here ]]


static void execute_command(char **words, char **path, char **environment);
static void do_exit(char **words);
static int is_executable(char *pathname);
static char **tokenize(char *s, char *separators, char *special_chars);
static void free_tokens(char **tokens);

// [[ TODO: put any extra function prototypes here ]]

void running_the_command(char *command, char **words, char **environment);
void add_command_to_history(char **words);
int find_lines();
int locate_path(char *program, char **words, char **path, char **environment);
void print_history(char **words);
int global_character_check(char **words);
int is_number(char **words);
char **globbing_procedure(char **words, char **path, char **environment);
int input_output_character_check(char **words);
int input_output_redirection_procedure(char **words, char **path, char **environment);
void pipe_process(char **words, char **path, char **environment, int type);
char **remove_special_characters(char **words, int type);

int main (void)
{
    // Ensure `stdout' is line-buffered for autotesting.
    setlinebuf(stdout);

    // Environment variables are pointed to by `environ', an array of
    // strings terminated by a NULL value -- something like:
    //     { "VAR1=value", "VAR2=value", NULL }
    extern char **environ;

    // Grab the `PATH' environment variable for our path.
    // If it isn't set, use the default path defined above.
    char *pathp;
    if ((pathp = getenv("PATH")) == NULL) {
        pathp = (char *) DEFAULT_PATH;
    }
    char **path = tokenize(pathp, ":", "");

    // Should this shell be interactive?
    bool interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    // Main loop: print prompt, read line, execute command
    while (1) {
        // If `stdout' is a terminal (i.e., we're an interactive shell),
        // print a prompt before reading a line of input.
        if (interactive) {
            fputs(INTERACTIVE_PROMPT, stdout);
            fflush(stdout);
        }

        char line[MAX_LINE_CHARS];
        if (fgets(line, MAX_LINE_CHARS, stdin) == NULL)
            break;

        // Tokenise and execute the input line.
        char **command_words =
            tokenize(line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
        execute_command(command_words, path, environ);
        free_tokens(command_words);
    }

    free_tokens(path);
    return 0;
}

// Execute a command, and wait until it finishes.
//  * `words': a NULL-terminated array of words from the input command line
//  * `path': a NULL-terminated array of directories to search in;
//  * `environment': a NULL-terminated array of environment variables.

static void execute_command(char **words, char **path, char **environment) {
    assert(words != NULL);
    assert(path != NULL);
    assert(environment != NULL);
    //have a command_counter to keep track if any command runs. If one does
    //this counter will no longer be 0. If 0, the program will run the command
    //not found lines.
    int command_counter = 0;

    if (words[0] == NULL) {
        // nothing to do
        return;
    }
    char program[PATH_MAX];
    strcpy(program, words[0]);

    if (strcmp("exit", program) == 0) {
        do_exit(words);
        // `do_exit' will only return if there was an error.
        return;
    }
    if (global_character_check(words) != 0) {
        //global character code
        //to be a global character, the input must contain either
        // *, [], ?, ~
        command_counter++;
        char **new_words = globbing_procedure(words, path, environment);
        locate_path(program, new_words, path, environment);
        free_tokens(new_words);

    } else if (input_output_character_check(words) != 0) {
        //input input_output code
        //run a command which contains the '<' or '>' characters
        command_counter++;
        input_output_redirection_procedure(words, path, environment);

    } else if (strcmp("cd", program) == 0) {
        //implement the change directory command
        command_counter++;
        if (words[1] != NULL && words[2] != NULL) {
            fprintf(stderr, "cd: too many arguments\n");
        } else {
            if (words[1] != NULL) {
                //change the directory to the specified 
                int valid = chdir(words[1]);
                if (valid == -1) {
                    fprintf(stderr, "cd: %s: No such file or directory\n", words[1]);
                }
            } else if (words[1] == NULL) {
                //return back to the home directory
                char *value = getenv("HOME");
                chdir(value);
            }
        }
    } else if (strcmp("pwd", program) == 0) {
        //implement the print working directory command
        //check for errors
        if (words[1] != NULL) {
            fprintf(stderr, "pwd: too many arguments\n");   
        } else {
            command_counter++;
            //correct input: find the current directory and print it as output
            char current_directory[PATH_MAX];
            if (getcwd(current_directory, sizeof(current_directory)) != NULL) {
                printf("current directory is '%s'\n", current_directory);
            } else {
                //no current directory: print error
                perror("getcwd() error");
            } 
        }
    } else if (strcmp("history", program) == 0) {
        //implement the history command
        command_counter++;
        //check for errors
        //2 types of input: history || history [number]
        if (words[1] != NULL && words[2] != NULL) {
            //too many input numbers: print error
            fprintf(stderr, "history: too many arguments\n");
        } else if (words[1] != NULL && is_number(words) == 0) {
            //second input is not a number: print error
            fprintf(stderr, "history: %s: numeric argument required\n", words[1]);
        } else {
            //no errors with input: print history
            print_history(words);
        }
    } else if (strcmp(program, "!") == 0) {
        //replicate a command
        command_counter++;
        int needed_command = 0;
        if (words[1] != NULL) {
            //specified: we want to call the command as stated
            needed_command = atoi(words[1]);
        } else {
            //if not specified: we want to call the latest command
            needed_command = (find_lines() - 1);
        }
        //get the directory of the wanted command
        char file[MAX_LINE_CHARS];
        strcat(strcpy(file, getenv("HOME")), "/.shuck_history");
        FILE * history_file = fopen(file, "r+");
        int line_counter = 0;
        int i = 0;
        int character;
        char command[MAX_LINE_CHARS];
        while ((character = getc(history_file)) != EOF) {
            if (character == '\n') {
                line_counter++;
            }
            if (line_counter == needed_command && character != '\n') {
                char charvalue = character;
                command[i] = charvalue;
                i++;
            }
        }
        command[i] = '\0';
        //print out the command and then execute the command
        printf("%s\n", command);
        char **tokenized_words = tokenize(command, " ", "");
        char *tokenized_program = tokenized_words[0];
        //add the command to history; not the ! command
        add_command_to_history(tokenized_words);
        //check if passed in command has a global variable
        int global_check = global_character_check(tokenized_words);
        if (global_check == 0) {
            //passed in command does not include a global variable
            locate_path(tokenized_program, tokenized_words, path, environment);
        } else {
            //passed in command includes a global variable
            char **new_words = globbing_procedure(tokenized_words, path, environment);
            locate_path(tokenized_program, new_words, path, environment);
        }
        free_tokens(tokenized_words);
    } else {
        //execute the command using posix_spawn
        command_counter = locate_path(program, words, path, environment);
    }
    if (strcmp(program, "!") != 0) {
        //add command to the history file
        //make sure the command is not a ! command
        //for ! commands, we want to add the executed command,
        //rather than the ! command.
        add_command_to_history(words);
    }
    if (command_counter == 0) {
        fprintf(stderr, "%s: command not found\n", program);
    }
}

// Implement the `exit' shell built-in, which exits the shell.
//
// Synopsis: exit [exit-status]
// Examples:
//     % exit
//     % exit 1
static void do_exit(char **words) {
    assert(words != NULL);
    assert(strcmp(words[0], "exit") == 0);

    int exit_status = 0;

    if (words[1] != NULL && words[2] != NULL) {
        // { "exit", "word", "word", ... }
        fprintf(stderr, "exit: too many arguments\n");

    } else if (words[1] != NULL) {
        // { "exit", something, NULL }
        char *endptr;
        exit_status = (int) strtol(words[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "exit: %s: numeric argument required\n", words[1]);
        }
    }

    exit(exit_status);
}

// Check whether this process can execute a file.  This function will be
// useful while searching through the list of directories in the path to
// find an executable file.
static int is_executable(char *pathname) {
    struct stat s;
    return
    // does the file exist?
    stat(pathname, &s) == 0 &&
    // is the file a regular file?
    S_ISREG(s.st_mode) &&
    // can we execute it?
    faccessat(AT_FDCWD, pathname, X_OK, AT_EACCESS) == 0;
}

// Split a string 's' into pieces by any one of a set of separators.
// Returns an array of strings, with the last element being `NULL'.
// The array itself, and the strings, are allocated with `malloc(3)';
// the provided `free_token' function can deallocate this.
static char **tokenize(char *s, char *separators, char *special_chars) {
    size_t n_tokens = 0;
    // Allocate space for tokens.  We don't know how many tokens there
    // are yet --- pessimistically assume that every single character
    // will turn into a token.  (We fix this later.)
    char **tokens = calloc((strlen(s) + 1), sizeof *tokens);
    assert(tokens != NULL);
    while (*s != '\0') {
        // We are pointing at zero or more of any of the separators.
        // Skip all leading instances of the separators.
        s += strspn(s, separators);
        // Trailing separators after the last token mean that, at this
        // point, we are looking at the end of the string, so:
        if (*s == '\0') {
            break;
        }
        // Now, `s' points at one or more characters we want to keep.
        // The number of non-separator characters is the token length.
        size_t length = strcspn(s, separators);
        size_t length_without_specials = strcspn(s, special_chars);
        if (length_without_specials == 0) {
            length_without_specials = 1;
        }
        if (length_without_specials < length) {
            length = length_without_specials;
        }
        // Allocate a copy of the token.
        char *token = strndup(s, length);
        assert(token != NULL);
        s += length;
        // Add this token.
        tokens[n_tokens] = token;
        n_tokens++;
    }
    // Add the final `NULL'.
    tokens[n_tokens] = NULL;
    // Finally, shrink our array back down to the correct size.
    tokens = realloc(tokens, (n_tokens + 1) * sizeof *tokens);
    assert(tokens != NULL);
    return tokens;
}
// Free an array of strings as returned by `tokenize'.
static void free_tokens(char **tokens) {
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

// program is passed into this command. we want to run the command
// by using posix_spawn
void running_the_command(char *command, char **words, char **environment) {
    pid_t pid = 0;
    if (posix_spawn(&pid, command, NULL, NULL, words, environment) != 0) {
        perror("spawn");
        exit(1);
    }
    // wait for spawned processes to finish
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        printf("%s exit status = %d\n", command, exit_status);
    }    
}

// program is passed into this command. we want to add this program
// into shuck_history file
void add_command_to_history(char **words) {
    //open up the shuck_history file
    char file[MAX_LINE_CHARS];
    strcat(strcpy(file, getenv("HOME")), "/.shuck_history");
    FILE * history_file;
    history_file = fopen(file, "a+");
    //add the command into the shuck_history file
    int word_counter = 0;
    while (words[word_counter] != NULL) {
        fprintf(history_file, "%s ", words[word_counter]);
        word_counter++;
    }
    fprintf(history_file, "\n");
    //close the shuck_history file
    fclose(history_file);
}

// find the total number of lines (which is the same as commands)
// in the shuck_history file
int find_lines() {
    //find how many lines are in the shuck_history file
    //the number of lines is equal to the number of commands run
    char file[MAX_LINE_CHARS];
    //open the shuck_history file
    strcat(strcpy(file, getenv("HOME")), "/.shuck_history");
    FILE * history_file;
    history_file = fopen(file, "r+");
    int character; 
    int lines = 0;
    while ((character = getc(history_file)) != EOF) {
        // everytime we detect a newline character, it indicates
        // a new command in the file
        if (character == '\n') { 
            lines++;
        }
    }
    //close the shuck_history file
    fclose(history_file);
    return lines;
}

// a relative path is passed into this command. We need to get the
// absolute path before it can be executed using posix_spawn
int locate_path(char *program, char **words, char **path, char **environment) {
    //the passed in command is a relative path
    //We want to find its absolute path , which is then passed
    //into posix_spawn
    int i = 0;
    int command_successfully_ran = 0;
    while (path[i] != NULL) {
        char command[PATH_MAX];
        if (strrchr(program, '/') == NULL) {
            strcpy(command, path[i]);
            strcat(command, "/");   
            strcat(command, program);
        } else {
            strcpy(command, program);
        }
        if (is_executable(command)) {
            command_successfully_ran++;
            running_the_command(command, words, environment);
            break;
        }
        i++;
    }
    return command_successfully_ran;
}

// a program to print out the whole shuck_history file
void print_history(char **words) {
    //
    int print_how_many = 0;
    if (words[1] != NULL) {
        print_how_many = atoi(words[1]);
    }
    int lines = find_lines();
    char file[MAX_LINE_CHARS];
    //open the shuck_history file
    strcat(strcpy(file, getenv("HOME")), "/.shuck_history");
    FILE * history_file = fopen(file, "r+");
    int print_lines = 0;
    //we only want to print the last 10 inputs
    //if number of inputs is more than 10, we only want the last 10
    //below are checkers for all of the different cases 
    if (lines > 9) {
        print_lines = (lines - 10);
    }
    if (print_how_many != 0 && print_how_many > lines) {
        print_lines = 0;
    } else if (print_how_many != 0 && lines < 10) {
        print_lines = print_lines + (lines - print_how_many);
    } else if (print_how_many != 0) {
        print_lines = print_lines + (10 - print_how_many);
    }
    printf("%d: ", (print_lines));
    int cycle_lines = 0;
    int character;
    //loop through the history file to print out each line appropriately
    while ((character = getc(history_file)) != EOF) {
        if (feof(history_file)) {
            //if history_file doesnt have any more lines, break
            break;
        }
        if (cycle_lines >= print_lines) {
            putchar(character);
            //everytime we have new line, we need to print out the new command
            //number before the actual command
            if (character == '\n' && (cycle_lines + 1) != lines) {
                printf("%d: ", (cycle_lines + 1));
            }
        }
        if (character == '\n') {
            cycle_lines++;
        }
    }
    //close the shuck_history file
    fclose(history_file);
}

// check if **words has a global character in it
int global_character_check(char **words) {
    //check if the input has any of the global characters
    int i = 0;
    int counter = 0;
    while (words[i] != NULL) {
        int j = 0;
        while (words[i][j] != '\0') {
            char c = words[i][j];
            if (c == '*' ||
                c == '?' ||
                c == '[' ||
                c == '~') {
                counter++;
            }
            j++;
        }
        i++;
    }
    return counter;
}

// check if **words has an input_output character in it
int input_output_character_check(char **words) {
    //check if the input has any of the global characters
    int i = 0;
    int counter = 0;
    while (words[i] != NULL) {
        int j = 0;
        while (words[i][j] != '\0') {
            char c = words[i][j];
            if (c == '<' || c == '>') {
                counter++;
            }
            j++;
        }
        i++;
    }
    return counter;
}

// condition checker for the input/output redirection 
// process. If conditions are met, values are passed to pipe process
int input_output_redirection_procedure(char **words, char **path, char **environment) {
    int number_of_commands = 0;
    for (int i = 0; words[i] != NULL; i++) {
        number_of_commands++;
    }
    // get an understanding of how many '>' or '<' characters
    // there are in the input. if too many, output error.
    int character_counter = input_output_character_check(words);
    number_of_commands--;
    int type;
    // type = 0: <  (input)
    // type = 1: >> (create new file, override previous file if found)
    // type = 2: >  (append to file, create file if not found)

    if (strcmp(words[0], "<") == 0 && character_counter == 1) {
        type = 0;
        pipe_process(words, path, environment, type);

    } else if (number_of_commands > 2 &&
               strcmp(words[number_of_commands - 2], ">") == 0 &&
               strcmp(words[number_of_commands - 1], ">") == 0 &&
               character_counter == 2) {
        type = 1;
        pipe_process(words, path, environment, type);

    } else if (strcmp(words[number_of_commands - 1], ">") == 0 &&
        character_counter == 1) {
        type = 2;
        pipe_process(words, path, environment, type);

    } else {
        int counter = 0;
        for (int i = 0; words[i] != NULL; i++) {
            if (strcmp(words[i], "<") == 0) {
                counter++;
            }
        }
        if (counter == 0) {
            printf("invalid output redirection\n");
        } else {
            printf("invalid input redirection\n");
        }
    }
    return 0;
}

// carries out the pipe process using multiple
// posix_spawn actions.
void pipe_process(char **words, char **path, char **environment, int type) {
    int number_of_commands = 0;
    for (int i = 0; words[i] != NULL; i++) {
        number_of_commands++;
    }
    number_of_commands--;

    char *file;
    if (type == 0) {
        file = words[1];
    } else {
        file = words[number_of_commands];
    }

    posix_spawn_file_actions_t actions;
    FILE * output_file;
    if (type == 0) {
        output_file = fopen(file, "r");
    } else if (type == 1) {
        output_file = fopen(file, "a");
    } else {
        output_file = fopen(file, "w");
    }

    if (output_file == NULL) {
        fprintf(stderr, "%s: No such file or directory\n", words[1]);
        return;
    }

    int file_descriptor = fileno(output_file);
    if (file_descriptor == -1) {
        perror("pipe");
        return;
    }

    if (posix_spawn_file_actions_init(&actions) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }

    if (type == 0) {
        if (posix_spawn_file_actions_adddup2(&actions, file_descriptor, 0) != 0) {
            perror("posix_spawn_file_actions_adddup2");
            return;
        }
    } else if (type == 1 || type == 2) {
        if (posix_spawn_file_actions_adddup2(&actions, file_descriptor, 1) != 0) {
            perror("posix_spawn_file_actions_adddup2");
            return;
        }       
    }

    if (posix_spawn_file_actions_addclose(&actions, file_descriptor) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }
    char *program;
    if (type == 0) {
        program = words[2];
    } else {
        program = words[0];
    }

    if (strcmp(program, "cd") == 0 || strcmp(program, "pwd") == 0 ||
        strcmp(program, "history") == 0 || strcmp(program, "!") == 0) {
        fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", 
                program);
        return;        
    }

    int i = 0;
    char command[PATH_MAX];
    while (path[i] != NULL) {
        strcpy(command, path[i]);
        strcat(command, "/");   
        strcat(command, program);
        if (is_executable(command)) {
            break;
        }
        i++;
    }

    char **new_words = remove_special_characters(words, type);
    pid_t pid;
    if (posix_spawn(&pid, command, &actions, NULL, new_words, environment) != 0) {
        perror("spawn");
        return;
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        printf("%s exit status = %d\n", command, exit_status);
    }  
    posix_spawn_file_actions_destroy(&actions);
    return;
}

// create a new 2d string which includes all the values 
// of words except for '<', '>' characters and filenames
char **remove_special_characters(char **words, int type) {
    char temp_words[512] = {0};
    // printf("%s\n", temp_words);
    if (type == 0) {
        for (int i = 2; words[i] != NULL; i++) {
            strcat(temp_words, words[i]);
            strcat(temp_words, ":");
        }

    } else if (type == 1) {
        for (int i = 0; words[i + 3] != NULL; i++) {
            strcat(temp_words, words[i]);
            strcat(temp_words, ":");
        }

    } else if (type == 2) {
        for (int i = 0; words[i + 2] != NULL; i++) {
            strcat(temp_words, words[i]);
            strcat(temp_words, ":");
        }
    }

    char **new_words = tokenize(temp_words, ":", "");
    return new_words;
}

// check if **words is completely number or contains a letter
int is_number(char **words) {
    //check if the input contains only numbers or
    //also has some alphabetical letter
    char * supposed_number = words[1];
    int i = 0;
    int counter = 0;
    //if it has an alphabetical letter, then counter will increase
    //if the input is purely numbers, then counter will be 0
    while (supposed_number[i] != '\0') {
        counter = isdigit(supposed_number[i]);
        i++;
    }
    return counter;
}

// carry out the globbing procedure. return a 2d string with the
// new globalised words.
char **globbing_procedure(char **words, char **path, char **environment) {
    glob_t matches; 
    // holds pattern expansion
    char temp_words[MAX_LINE_CHARS];
    strcat(temp_words, words[0]);
    strcat(temp_words, " ");
    int j = 1;

    while (words[j] != NULL) {
        glob(words[j], GLOB_NOCHECK|GLOB_TILDE, NULL, &matches);   
        for (int i = 0; i < matches.gl_pathc; i++) {
            strcat(temp_words, matches.gl_pathv[i]);
            strcat(temp_words, " ");
        }
        j++;
    }

    char **new_words = tokenize(temp_words, " ", "");
    return new_words;
}