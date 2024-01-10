#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

pid_t pid_global;
int serial_foreground = 0;
int background = 0;
int parallel_foreground = 0;
int foreground = 0;
int stopped = 0;

typedef struct
{
    char **argu;
} thread_args;

char **tokenize(char *line)
{
    char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
    char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
    int i, tokenIndex = 0, tokenNo = 0;

    for (i = 0; i < strlen(line); i++)
    {

        char readChar = line[i];

        if (readChar == ' ' || readChar == '\n' || readChar == '\t')
        {
            token[tokenIndex] = '\0';
            if (tokenIndex != 0)
            {
                tokens[tokenNo] = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
                strcpy(tokens[tokenNo++], token);
                tokenIndex = 0;
            }
        }
        else
        {
            token[tokenIndex++] = readChar;
        }
    }
    free(token);
    tokens[tokenNo] = NULL;
    return tokens;
}
void run_command(char **tokens)
{

    pid_global = fork();
    if (pid_global == 0)
    {
        execvp(*tokens, tokens);
        printf("\n");
        exit(1);
    }
    else if (pid_global > 0)
    {
        if (foreground)
        {
            wait(NULL);
            serial_foreground = 0;
            background = 0;
            parallel_foreground = 0;
            foreground = 0;
        }
        else
        {
            serial_foreground = 0;
            background = 0;
            parallel_foreground = 0;
            foreground = 0;
        }
    }
}
void *thread_run(void *args)
{
    thread_args *thread_arguments = (thread_args *)args;
    char **argv = thread_arguments->argu;
    run_command(argv);
    return NULL;
}
void serial_foreground_run(char **tokens, int size)
{
    char **token_temp;
    for (int i = 0; i < size;)
    {
        int y = 0;
        while (tokens[i] != NULL && strcmp(tokens[i], "&&"))
        {
            token_temp[y] = tokens[i];
            y++;
            i++;
        }
        token_temp[y] = NULL;
        i++;
        foreground = 1;
        serial_foreground = 1;
        background = 0;
        parallel_foreground = 0;
        if (stopped == 1)
        {
            kill(pid_global, SIGINT);
        }
        run_command(token_temp);
        wait(NULL);
    }
}
void parallel_foreground_run(char **tokens, int size)
{
    char **token_temp;
    pthread_t thread_id[parallel_foreground];
    int ind = 0;
    for (int i = 0; i < size;)
    {
        int y = 0;
        while (tokens[i] != NULL && strcmp(tokens[i], "&&&"))
        {
            token_temp[y] = tokens[i];
            y++;
            i++;
        }
        token_temp[y] = NULL;
        i++;
        foreground = 1;
        serial_foreground = 0;
        background = 0;
        parallel_foreground = 1;
        pthread_create(&thread_id[ind], NULL, run_command, token_temp);
        pthread_join(thread_id[ind], NULL);
        ind++;
    }
}
void running_commads_using_execvp(char **tokens)
{
    int n = 0;
    for (int i = 0; tokens[i] != NULL; i++)
    {
        n++;
        if (strcmp(tokens[i], "&&") == 0)
        {
            serial_foreground = 1;
            foreground = 1;
            background = 0;
            parallel_foreground = 0;
        }
        if (strcmp(tokens[i], "&&&") == 0)
        {
            parallel_foreground = 1;
            foreground = 1;
            background = 0;
            serial_foreground = 0;
        }
    }
    if (strcmp(tokens[0], "exit") == 0)
    {
        /* Exiting the shell */
        kill(pid_global, SIGINT);
        exit(0);
    }
    if (serial_foreground == 1)
    {
        serial_foreground_run(tokens, n);
    }
    else if (parallel_foreground == 1)
    {
        parallel_foreground_run(tokens, n);
        printf("HIT HERE\n");
    }
    else
    {
        if (!strcmp(tokens[n - 1], "&"))
        {
            printf("HELLo\n");
            tokens[n - 1] = NULL;
            background = 1;
            foreground = 0;
            run_command(tokens);
        }
        else
        {
            foreground = 1;
            background = 0;
            run_command(tokens);
        }
    }
    return;
}
void INThandler(int sig)
{
    if (serial_foreground == 1)
    {
        stopped = 1;
        serial_foreground = 0;
        foreground = 0;
        background = 0;
        kill(pid_global, SIGINT);
    }
    if (pid_global)
    {
        kill(pid_global, SIGINT);
        printf("\n");
    }
}
int main(int argc, char *argv[])
{
    char line[MAX_INPUT_SIZE];
    char **tokens;
    int i;
    signal(SIGINT, INThandler);
    char cwd[1024];
    while (1)
    {
        /* Getting working directory of the code */
        getcwd(cwd, sizeof(cwd));

        /* BEGIN: TAKING INPUT */
        printf("\x1B[1m");
        printf("\033[0;32m");
        bzero(line, sizeof(line));
        printf("%s:$ ", cwd);
        printf("\033[0m");
        printf("\x1B[0m");
        scanf("%[^\n]", line);
        getchar();
        /* END: TAKING INPUT */
        line[strlen(line)] = '\n'; // terminate with new line
        tokens = tokenize(line);

        /* Implementing cd command */
        if (strcmp(tokens[0], "cd") == 0)
        {
            if (strcmp(tokens[1], "..") == 0)
            {
                /* Going back if 'cd ..' is entered*/
                chdir("..");
            }
            else if (tokens[1] != NULL)
            {
                /* Going to the directory entered */
                if (chdir(tokens[1]) == -1)
                {
                    printf("\033[0;31m");
                    printf("bash: cd: %s: No such file or directory\n", tokens[1]);
                    printf("\033[0m");
                }
                else
                {
                    chdir(tokens[1]);
                }
            }
            else
            {
                printf("\033[0;31m");
                printf("bash: Please enter a directory name");
                printf("\033[0m");
            }
        }
        else
        {
            running_commads_using_execvp(tokens);
        }

        // Catch Ctrl-C command

        // Freeing the allocated memory
        for (i = 0; tokens[i] != NULL; i++)
        {
            free(tokens[i]);
        }
        free(tokens);
    }
    return 0;
}