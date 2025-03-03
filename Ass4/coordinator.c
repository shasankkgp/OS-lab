#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include "boardgen.c"

void print_() {
    printf("\t\t+---+---+---+\n");
}

int main() {
    int fds[9][2];
    for (int i = 0; i < 9; i++) {
        pipe(fds[i]);
    }

    int A[9][9], S[9][9];
    int saved_stdout = dup(STDOUT_FILENO);

    for (int i = 0; i < 9; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // xterm -T "Block 0" -fa Monospace -fs 15 -geometry "17x8+800+300" -bg "#331100" \-e ./block blockno bfdin bfdout rn1fdout rn2fdout cn1fdout cn2fdout
            char bfdin[10], bfdout[10], rn1fdout[10], rn2fdout[10], cn1fdout[10], cn2fdout[10], location[20];
            char block[10];
            char temp[10];

            strcpy(block, "\"Block ");
            sprintf(temp, "%d\"", i);
            strcat(block, temp);

            sprintf(bfdin, "%d", fds[i][0]);
            sprintf(bfdout, "%d", fds[i][1]);
            sprintf(rn1fdout, "%d", fds[((i + 1) % 3) + (i / 3) * 3][1]);
            sprintf(rn2fdout, "%d", fds[((i + 2) % 3) + (i / 3) * 3][1]);
            sprintf(cn1fdout, "%d", fds[((i + 3) % 9)][1]);
            sprintf(cn2fdout, "%d", fds[((i + 6) % 9)][1]);

            strcpy(location, "17x8+");
            sprintf(temp, "%d", 1150 + (i % 3) * 250);
            strcat(location, temp);
            strcat(location, "+");
            sprintf(temp, "%d", 150 + (i / 3) * 250);
            strcat(location, temp);

            char block_no[10];
            sprintf(block_no, "%d", i);

            char colour[10];
            strcpy(colour, "#331100");

            execlp("xterm", "xterm", "-T", block, "-fa", "Monospace", "-fs", "15", "-geometry", location, "-bg", colour, "-fg", "white",  "-e", "./block", block_no, bfdin, bfdout, rn1fdout, rn2fdout, cn1fdout, cn2fdout, NULL);
            perror("execlp failed");
            exit(EXIT_FAILURE);
        }
    }

    char command='h';
    do {
        if (command == 'h') {
            // Print help message
            printf("Commands supported\n");
            printf("\t\tn\t\t\tStart new game\n");
            printf("\t\tp b c d\t\t\tPut digit d [1-9] at cell c[0-8] of block b[0-8]\n");
            printf("\t\ts\t\t\tShow solution\n");
            printf("\t\th\t\t\tprint this help message\n");
            printf("\t\tq\t\t\tQuit\n");
            printf("\n");
            printf("Numbering scheme for blocks and cells\n");
            print_();
            for (int i = 0; i < 3; i++) {
                printf("\t\t|");
                for (int j = 0; j < 3; j++) {
                    printf(" %d |", i * 3 + j);
                }
                printf("\n");
                print_();
            }
        } else if (command == 'n') {
            // Launch new puzzle
            newboard(A, S);
            for (int i = 0; i < 9; i++) {
                // Send block data to child processes
                dup2(fds[i][1], STDOUT_FILENO);

                printf("n\n");
                int row = i / 3;
                int col = i % 3;
                for (int j = 0; j < 3; j++) {
                    for (int k = 0; k < 3; k++) {
                        printf("%d\n", A[row * 3 + j][col * 3 + k]);
                    }
                }
                fflush(stdout);
                dup2(saved_stdout, STDOUT_FILENO);
            }
        } else if (command == 'p') {
            // Handle place/replace digit command
            int b, c, d;
            scanf("%d %d %d", &b, &c, &d);
            if (b < 0 || b >= 9 || c < 0 || c >= 9 || d < 1 || d > 9) {
                printf("Invalid command\n");
                printf("Foodoko> ");
                continue;
            }
            dup2(fds[b][1], STDOUT_FILENO);
            printf("p %d %d\n", c, d);
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
        } else if (command == 's') {
            // Show solution
            for (int i = 0; i < 9; i++) {
                // Send show solution command to child processes

                close(1);
                dup(fds[i][1]);
                printf("s\n");
                int row = i / 3;
                int col = i % 3;
                for (int j = 0; j < 3; j++) {
                    for (int k = 0; k < 3; k++) {
                        printf("%d\n", S[row * 3 + j][col * 3 + k]);
                    }
                }
                fflush(stdout);
                dup2(saved_stdout, STDOUT_FILENO);
            }
        } else if (command == 'q') {
            // Quit
            for (int i = 0; i < 9; i++) {
                // Send quit commands to child processes
                dup2(fds[i][1], STDOUT_FILENO);
                printf("q\n");
                fflush(stdout);
                dup2(saved_stdout, STDOUT_FILENO);
            }
            for (int i = 0; i < 9; i++) {
                wait(NULL); // Waiting for all child processes to terminate
            }
            break;
        }
        printf("Foodoko> ");
    }while (scanf(" %c", &command) != EOF);

    return 0;
}