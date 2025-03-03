#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void print_() {
    printf("  +---+---+---+\n");
}

int A[3][3], B[3][3];

void print(){      // function to print the block on screen
    printf("\033[H\033[J");
        print_();
        for (int i = 0; i < 3; i++) {
            printf("  |");
            for (int j = 0; j < 3; j++) {
                if (B[i][j]) printf(" %d |", B[i][j]);
                else printf("   |");
            }
            printf("  \n");
            print_();
        }
        fflush(stdout);
}

int main(int argc, char* argv[]) {

    int block_no = atoi(argv[1]);
    int bfdin = atoi(argv[2]);
    int bfdout = atoi(argv[3]);
    int rn1fdout = atoi(argv[4]);
    int rn2fdout = atoi(argv[5]);
    int cn1fdout = atoi(argv[6]);
    int cn2fdout = atoi(argv[7]);

    dup2(bfdin, STDIN_FILENO); // Reading from the input pipe

    int saved_stdout = dup(STDOUT_FILENO);

    char command;
    printf("Block %d ready\n", block_no);
    while (scanf(" %c", &command) != EOF) {
        if (command == 'n') {    // new game 
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    scanf("%d", &A[i][j]);
                    B[i][j] = A[i][j];
                }
            }
        } else if (command == 'p') {     
            int c, d;
            scanf("%d %d", &c, &d);
            int row = c / 3;
            int col = c % 3;
            if (A[row][col] != 0) {   // checking if the block is read only or not
                printf("Readonly cell");
                fflush(stdout);
                sleep(2);
            } else {
                // Check if the element in the block is already present
                int flag = 0;
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        if (B[i][j] == d) {
                            flag = 1;
                            break;
                        }
                    }
                }
                if (flag == 1) {
                    printf("Block conflict");
                    fflush(stdout);
                    sleep(2);
                } else {
                    // Check row and column for the digit d
                    dup2(rn1fdout, STDOUT_FILENO);
                    printf("r %d %d %d\n", row, d, bfdout);
                    fflush(stdout);
                    dup2(saved_stdout, STDOUT_FILENO);
                    scanf("%d", &flag);
                    if (flag == 1) {
                        printf("Row conflict");
                        fflush(stdout);
                        sleep(2);
                        print();
                        continue;
                    }
                    dup2(rn2fdout, STDOUT_FILENO);
                    printf("r %d %d %d\n", row, d, bfdout);
                    fflush(stdout);
                    dup2(saved_stdout, STDOUT_FILENO);
                    scanf("%d", &flag);
                    if (flag == 1) {
                        printf("Row conflict");
                        fflush(stdout);
                        sleep(2);
                        print();
                        continue;
                    }
                    dup2(cn1fdout, STDOUT_FILENO);
                    printf("c %d %d %d\n", col, d, bfdout);
                    fflush(stdout);
                    dup2(saved_stdout, STDOUT_FILENO);
                    scanf("%d", &flag);
                    if (flag == 1) {
                        printf("Column conflict");
                        fflush(stdout);
                        sleep(2);
                        print();
                        continue;
                    }
                    dup2(cn2fdout, STDOUT_FILENO);
                    printf("c %d %d %d\n", col, d, bfdout);
                    fflush(stdout);
                    dup2(saved_stdout, STDOUT_FILENO);
                    scanf("%d", &flag);
                    if (flag == 1) {
                        printf("Column conflict");
                        fflush(stdout);
                        sleep(2);
                        print();
                        continue;
                    }
                    B[row][col] = d;     // placing the element in the block
                }
            }
        } else if (command == 'r') {
            // Handle row check requests
            int row1,d1,bfdout1;
            scanf("%d %d %d", &row1, &d1, &bfdout1);
            int flag = 0;
            for (int i = 0; i < 3; i++) {
                if (B[row1][i] == d1) {
                    flag = 1;
                    break;
                }
            }
            dup2(bfdout1, STDOUT_FILENO);
            printf("%d\n", flag);
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
        } else if (command == 'c') {
            // Handle column check requests
            int col1, d1, bfdout1;
            scanf("%d %d %d", &col1, &d1, &bfdout1);
            int flag = 0;
            for (int i = 0; i < 3; i++) {
                if (B[i][col1] == d1) {
                    flag = 1;
                    break;
                }
            }
            dup2(bfdout1, STDOUT_FILENO);
            printf("%d\n", flag);
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
        } else if (command == 's') {
            // Show solution
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    scanf("%d", &B[i][j]);
                }
            }
        } else if (command == 'q') {
            // Handle quit command
            printf("Bye...");
            fflush(stdout);
            sleep(2);
            break;
        }
        print();
    }

    return 0;
}