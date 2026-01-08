#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

/* SHELL SECTION  */

#define MAX_ARGS 64
#define MAX_LINE 1024

char *last_command = NULL;

char **parse_input(char *line)
{
    char **args = malloc(MAX_ARGS * sizeof(char *));
    int i = 0;

    char *token = strtok(line, " \t\n");
    while (token && i < MAX_ARGS - 1)
    {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return args;
}
void execute_command(char **args, int background)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        execvp(args[0], args);
        perror("exec failed");
        exit(1);
    }
    else if (pid > 0)
    {
        if (!background)
            waitpid(pid, NULL, 0);
    }
}

void execute_pipe(char **left, char **right)
{
    int fd[2];
    pipe(fd);

    if (fork() == 0)
    {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        execvp(left[0], left);
        exit(1);
    }

    if (fork() == 0)
    {
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        execvp(right[0], right);
        exit(1);
    }

    close(fd[0]);
    close(fd[1]);
    wait(NULL);
    wait(NULL);
}

void shell_loop()
{
    char line[MAX_LINE];

    while (1)
    {
        printf("uinxsh> ");
        fflush(stdout);

        if (!fgets(line, MAX_LINE, stdin))
            break;

        if (strcmp(line, "!!\n") == 0)
        {
            if (!last_command)
            {
                printf("No commands in history\n");
                continue;
            }
            strcpy(line, last_command);
            printf("%s", line);
        }
        else
        {
            free(last_command);
            last_command = strdup(line);
        }

        if (strncmp(line, "exit", 4) == 0)
            break;

        int background = strchr(line, '&') != NULL;
        char *pipe_pos = strchr(line, '|');

        if (pipe_pos)
        {
            *pipe_pos = '\0';
            char **left = parse_input(line);
            char **right = parse_input(pipe_pos + 1);
            execute_pipe(left, right);
            free(left);
            free(right);
            continue;
        }

        char **args = parse_input(line);
        if (!args[0])
        {
            free(args);
            continue;
        }

        if (strcmp(args[0], "cd") == 0)
        {
            chdir(args[1]);
        }
        else if (strcmp(args[0], "pwd") == 0)
        {
            char cwd[512];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
        }
        else if (strcmp(args[0], "sudoku") == 0)
        {
            extern void run_sudoku();
            run_sudoku();
        }
        else if (strcmp(args[0], "montecarlo") == 0)
        {
            extern void monte_carlo(int, long);
            monte_carlo(atoi(args[1]), atol(args[2]));
        }
        else
        {
            execute_command(args, background);
        }

        free(args);

        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;
    }
}

/* SUDOKU  */

int sudoku[9][9] = {
    {5, 3, 4, 6, 7, 8, 9, 1, 2},
    {6, 7, 2, 1, 9, 5, 3, 4, 8},
    {1, 9, 8, 3, 4, 2, 5, 6, 7},
    {8, 5, 9, 7, 6, 1, 4, 2, 3},
    {4, 2, 6, 8, 5, 3, 7, 9, 1},
    {7, 1, 3, 9, 2, 4, 8, 5, 6},
    {9, 6, 1, 5, 3, 7, 2, 8, 4},
    {2, 8, 7, 4, 1, 9, 6, 3, 5},
    {3, 4, 5, 2, 8, 6, 1, 7, 9}};

void *check_row(void *arg)
{
    int r = *(int *)arg;
    int seen[10] = {0};

    for (int c = 0; c < 9; c++)
    {
        int v = sudoku[r][c];
        if (seen[v])
            pthread_exit((void *)0);
        seen[v] = 1;
    }
    pthread_exit((void *)1);
}

void run_sudoku()
{
    pthread_t threads[9];
    int rows[9];
    int valid = 1;

    for (int i = 0; i < 9; i++)
    {
        rows[i] = i;
        pthread_create(&threads[i], NULL, check_row, &rows[i]);
    }

    for (int i = 0; i < 9; i++)
    {
        void *res;
        pthread_join(threads[i], &res);
        if (res == 0)
            valid = 0;
    }

    if (valid)
        printf("Sudoku is valid\n");
    else
        printf("Sudoku is invalid\n");
}

/* MONTE CARLO PI*/

void monte_carlo(int processes, long points)
{
    long *inside = mmap(NULL, sizeof(long),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    *inside = 0;

    for (int i = 0; i < processes; i++)
    {
        if (fork() == 0)
        {
            unsigned int seed = time(NULL) ^ getpid();
            long local = 0;

            for (long j = 0; j < points / processes; j++)
            {
                double x = rand_r(&seed) / (double)RAND_MAX;
                double y = rand_r(&seed) / (double)RAND_MAX;
                if (x * x + y * y <= 1)
                    local++;
            }

            __sync_fetch_and_add(inside, local);
            exit(0);
        }
    }

    for (int i = 0; i < processes; i++)
        wait(NULL);

    double pi = 4.0 * (*inside) / points;
    printf("Estimated Pi = %f\n", pi);
}

int main()
{
    shell_loop();
    free(last_command);
    return 0;
}
