#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* 
===================================================================
===============================MACROS==============================
===================================================================
*/
#define BUFFER_SIZE 1024
#define MAX_ARG_COUNT 64
#define INPUT_DELIMETERS " \t\r\n\a"

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define RESET "\x1b[0m"

/* 
===================================================================
=============================INTERFACE============================= 
===================================================================
*/
char *set_prompt(int argc, char **argv);
void shell_loop(char *prompt);

char *get_raw_input();
char **split_args(char *raw, int *bg);
char *get_arg(char *token);

int execute(int runInBackground, char **args);
void change_directory(char **args);
void print_current_directory();
int program_command(char **args, int bg);

void check_alloc(void *ptr);
void *remove_first_and_last_chars(char *chars);

/* 
===================================================================
================================MAIN===============================
===================================================================
*/

/* 
    Description: 
        This program is a simplified unix shell implemented in C. Designed to run in a unix environment.

    Note:
        The code does not correctly run processes in the background. When a process is run in the background, the output is not correctly presented and the process will become a 'zombie' after it completes it's execution. 
*/
int main(int argc, char **argv)
{
    char *prompt = set_prompt(argc, argv);

    shell_loop(prompt);

    free(prompt);
    return 0;
}

/* 
===================================================================
============================SHELL LOGIC============================
===================================================================
*/

/*
    Main loop where users enter and execute commands
*/
void shell_loop(char *prompt)
{
    char *raw = NULL;
    char **args = NULL;
    int status = 1;

    int *bg = (int *)malloc(sizeof(int));

    do
    {
        fputs(prompt, stdout);
        raw = get_raw_input();
        args = split_args(raw, bg);

        status = execute(*bg, args);

        free(raw);
        free(args);
    } while (status);

    free(bg);
}

/* 
===================================================================
==========================FORMAT ARGUMENTS=========================
===================================================================
*/

/*
    Reads unaltered user input
*/
char *get_raw_input()
{
    int i;
    int c;
    int done;
    char *buffer;
    size_t bufsize = BUFFER_SIZE;

    buffer = (char *)malloc(bufsize * sizeof(char));
    check_alloc(buffer);

    for (i = 0; c = getchar();)
    {
        done = c == '\n' || c == EOF;

        buffer[i++] = !done ? c : '\0';

        if (done)
            break;

        if (i >= bufsize)
        {
            bufsize += BUFFER_SIZE;
            buffer = realloc(buffer, bufsize * sizeof(char));
            check_alloc(buffer);
        }
    }

    return buffer;
}

/*
    Splits arguments entered by user apart
*/
char **split_args(char *raw, int *bg)
{
    int i;
    int maxArgCount = MAX_ARG_COUNT;

    char **args = malloc(maxArgCount * sizeof(char *));
    check_alloc(args);

    char *token = strtok(raw, INPUT_DELIMETERS);

    for (i = 0; token; token = strtok(NULL, INPUT_DELIMETERS))
    {
        if (strcmp(token, "&") == 0)
        {
            *bg = 1;
            continue;
        }

        args[i++] = get_arg(token);

        if (i >= maxArgCount)
        {
            maxArgCount += MAX_ARG_COUNT;
            args = realloc(args, maxArgCount * sizeof(char *));
            check_alloc(args);
        }
    }

    args[i] = NULL;

    free(token);
    return args;
}

/*
    Returns the argument whether it is the token or a string in qto
*/
char *get_arg(char *token)
{
    if (token[0] != '"' && token[0] != '\'')
        return token;

    char *stringArg;
    char delimeter = token[0];

    asprintf(&stringArg, "%s", token);
    while (token = strtok(NULL, INPUT_DELIMETERS))
    {
        asprintf(&stringArg, "%s %s", stringArg, token);
        if (token[strlen(token) - 1] == delimeter)
            break;
    }

    remove_first_and_last_chars(stringArg);

    return stringArg;
}

/* 
===================================================================
==========================EXECUTE COMMANDS=========================
===================================================================
*/

/*
    Execute builtins or program commands
*/
int execute(int runInBackground, char **args)
{
    int status = 1;

    if (args[0] == NULL)
        return status;
    else if (strcmp(args[0], "exit") == 0)
        exit(EXIT_SUCCESS);
    else if (strcmp(args[0], "pid") == 0)
        printf("pid: %d\n", getpid());
    else if (strcmp(args[0], "ppid") == 0)
        printf("ppid: %d\n", getppid());
    else if (strcmp(args[0], "cd") == 0)
        change_directory(args);
    else if (strcmp(args[0], "pwd") == 0)
        print_current_directory();
    else if (strcmp(args[0], "jobs") == 0)
        printf("Not implemented, extra credit if you do\n");
    else
        status = program_command(args, runInBackground);

    return status;
}

/*
    Execute the program command
*/
int program_command(char **args, int bg)
{
    int status;
    int rc = fork();

    if (rc < 0)
    {
        perror(RED "Error\n" RESET);
        exit(EXIT_FAILURE);
    }

    if (rc == 0)
    {
        execvp(args[0], args);
        fprintf(stderr, "Cannot exec %s: No such file or directory\n", args[0]);
        kill(getpid(), SIGTERM);
    }

    printf("[%d] %s\n", rc, args[0]);

    int guardian = bg ? fork() : 0;

    if (guardian != 0 && bg)
        return 1;

    waitpid(rc, &status, NULL);
    int exitStatus = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status);
    printf("[%d] %s Exit %d\n", rc, args[0], exitStatus);

    if (guardian == 0 && bg)
    {
        kill(rc, SIGTERM);
        kill(getpid(), SIGTERM);
    }

    return 1;
}

/*
    Prints current working directory
*/
void print_current_directory()
{
    size_t pathSize = (size_t)pathconf(".", _PC_PATH_MAX);
    char *cwd = (char *)malloc(pathSize * sizeof(char));
    check_alloc(cwd);

    printf("%s/\n", getcwd(cwd, pathSize));

    free(cwd);
}

/*
    Implementation of the cd command
*/
void change_directory(char **args)
{
    char *dir = args[1] == NULL ? getenv("HOME") : args[1];
    if (chdir(dir) != 0)
        fprintf(stderr, "cd: %s no such file or directory\n", dir);
}

/* 
===================================================================
===============================HELPERS=============================
===================================================================
*/

/* 
    Sets prompt to either the given prompt by the user, 
    or the default "308sh> " 
*/
char *set_prompt(int argc, char **argv)
{
    char *prompt = NULL;

    if (argc >= 2 && strcmp(argv[1], "-p") == 0)
    {
        prompt = (char *)malloc(strlen(argv[2]) * sizeof(char));
        check_alloc(prompt);
        strcpy(prompt, argv[2]);
    }
    else
    {
        prompt = (char *)malloc(8 * sizeof(char));
        check_alloc(prompt);
        strcpy(prompt, "308sh> ");
    }

    return prompt;
}
/*
    Removes first and last characters in string or char array.
    Designed for removing quote marks from strings.
*/
void *remove_first_and_last_chars(char *chars)
{
    int lastCharIndex = strlen(chars) - 2;
    strncpy(chars, &chars[1], lastCharIndex);
    *(chars + lastCharIndex) = '\0';
}

/*
    Checks that memory allocation does not fail.
    Program exits on failure. 
*/
void check_alloc(void *ptr)
{
    if (ptr == NULL)
    {
        fputs("Unable to allocate memory\n", stderr);
        exit(EXIT_FAILURE);
    }
}