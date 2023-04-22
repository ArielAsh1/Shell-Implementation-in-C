//Ariel Ashkenazy 208465096

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MAX_PATH_LENGTH    (4096)
#define MAX_COMMAND_LENGTH (120)
#define MAX_TOKENS         (100)
#define MAX_HISTORY        (100)

#define TRUE               (1)
#define FALSE              (0)

#define ERROR              (-1)

typedef struct {
    pid_t pid;
    char command[MAX_COMMAND_LENGTH];
} HistoryEntry;

typedef struct {
    HistoryEntry history[MAX_HISTORY];
    char *tokens[MAX_TOKENS];
    int historySize;
    int running;
} State;

int readCommand(char buffer[], char *tokens[], char *historyEntryCommand);
void printPrompt(void);
pid_t handleCommand(State *state);
void updatePathEnv(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    State state;
    state.historySize = 0;
    state.running = TRUE;
    char buffer[MAX_COMMAND_LENGTH];
    
    updatePathEnv(argc, argv);

    // loop to read and execute commands
    while (state.running) {
        printPrompt();
        HistoryEntry *historyEntry = &state.history[state.historySize];
        int tokenCount = readCommand(buffer, state.tokens, historyEntry->command);
        if (tokenCount > 0) {
            pid_t pid = handleCommand(&state);
            if (pid > 0) {
                historyEntry->pid = pid;
                state.historySize++;
            }
        }
    }

    return 0;
}

pid_t handleCommand(State *state) {
    if (strcmp(state->tokens[0], "exit") == 0) {
        state->running = FALSE;
    } else if (strcmp(state->tokens[0], "history") == 0) {
        for (int i = 0; i < state->historySize; i++) {
            printf("%d %s", state->history[i].pid, state->history[i].command);
        }
        printf("%d history\n", getpid());
    } else if (strcmp(state->tokens[0], "cd") == 0) {
        // TODO: does cd always have an argument? or can the user send "cd"
        if (chdir(state->tokens[1]) != 0) {
            perror("chdir failed");
        }
    } else {
        // fork:
        //  < 0 -> error -> perror
        //  = 0 -> child -> execvp
        //  > 0 -> parent -> wait for child to finish
        pid_t forkRet = fork();
        if (forkRet < 0) {
            perror("fork failed");
        } else if (forkRet == 0) {
            // the command is stored in index 0, the arguments in 1-last
            const char *command = state->tokens[0];
            char **argv = state->tokens;
            int execRet = execvp(command, argv);
            if (execRet == ERROR) {
                perror("execvp failed");
                exit(ERROR);
            }
        } else {
            pid_t waitRet = waitpid(forkRet, NULL, 0);
            if (waitRet == ERROR) {
                perror("waitpid failed");
            }
        }

        return forkRet;
    }

    return getpid();
}

void printPrompt() {
    printf("$ ");
    fflush(stdout);
}

int readCommand(char buffer[], char *tokens[], char *historyEntryCommand) {
    char *bufferPtr = fgets(buffer, MAX_COMMAND_LENGTH, stdin);
    if (bufferPtr == NULL) {
        return -1;
    }

    strcpy(historyEntryCommand, bufferPtr);
    
    int tokenCount = 0;
    char *delimiter = " \n";
    char *token = NULL;
    token = strtok(buffer, delimiter);
    while (token != NULL) {
        tokens[tokenCount] = token;
        token = strtok(NULL, delimiter);
        tokenCount++;
    }
    tokens[tokenCount] = NULL;
    return tokenCount;
}

// overwrite the PATH environment variable with a new PATH value
// the new PATH value will contain the old value, with additional paths given to us in argv
// for example: When "./a.out x y z" and PATH="a:b:c", we will update PATH to "a:b:c:x:y:z"
void updatePathEnv(int argc, char *argv[]) {
    // will contain the updated PATH env (old+new)
    char pathBuffer[MAX_PATH_LENGTH] = { 0 };

    // take all the existing paths
    char *pathEnv = getenv("PATH");
    strcpy(pathBuffer, pathEnv);

    // add all the new paths (in argv)
    for (int i = 1; i < argc; i++) {
        char *dir = argv[i];
        char *p = pathBuffer + strlen(pathBuffer);
        strcpy(p, ":");
        strcpy(p + 1, dir);
    }

    int setEnvRet = setenv("PATH", pathBuffer, TRUE);
    if (setEnvRet == ERROR) {
        perror("setenv failed");
    }
}
