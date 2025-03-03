/*
    NAME : G . SAI SHASANK 
    ROLL NO : 22CS10025
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_NUMBERS 100

void dfs(int node) {
    FILE *fp;
    char line[256];
    int target_line, current_line = 2;
    int count = 0;
    target_line = node + 1;

    fp = fopen("foodep.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open data file\n");
        exit(2);
    }

    int dependencies[MAX_NUMBERS];
    // int n;

    fgets(line, 256, fp);
    // n = atoi(line);

    while (fgets(line, 256, fp) != NULL) {
        if (current_line == target_line) {
            char *ptr = strchr(line, ':');
            if (ptr != NULL) {
                ptr++;
                char *token = strtok(ptr, " ");
                while (token != NULL && count < MAX_NUMBERS) {
                    dependencies[count++] = atoi(token);
                    token = strtok(NULL, " ");
                }
            }
            break;
        }
        current_line++;
    }

    fclose(fp);

    for (int i = 0; i < count; i++) {
        FILE *file;
        file = fopen("done.txt", "r");
        if (file == NULL) {
            fprintf(stderr, "Unable to open file\n");
            exit(1);
        }
        int index = 0;
        int val;
        while (index < dependencies[i]) {
            val = fgetc(file);
            index++;
        }
        fclose(file);

        if (val == '0') {
            int id = fork();
            if (id == 0) {
                char dep_str[10];
                sprintf(dep_str, "%d", dependencies[i]);
                execlp("./rebuild", "./rebuild", dep_str,"-Wall", NULL);
                exit(0);
            } else {
                wait(NULL);
            }
        }
    }

    if( !dependencies[0] ){
        printf("foo%d rebuild\n", node);
    }else{
        printf("foo%d rebuild from ", node);
        for( int i=0 ; i<count ; i++ ){
            printf("foo%d ", dependencies[i]);
        }
        printf("\n");
    }

    FILE *fle;
    fle = fopen("done.txt", "r+");
    if (fle == NULL) {
        fprintf(stderr, "Unable to open file\n");
        exit(1);
    }
    char val[100];
    fgets(val, 100, fle);
    fseek(fle, 0, SEEK_SET);
    val[node - 1] = '1';
    fputs(val, fle);
    fclose(fle);

    return;
}

int main(int argc, char *argv[]) {
    FILE *fp;
    int n;
    if (argc == 1) {
        fprintf(stderr, "Run with a node name\n");
        exit(1);
    }

    n = atoi(argv[1]);

    if (argc == 2) {
        fp = fopen("done.txt", "w");
        if (fp == NULL) {
            fprintf(stderr, "Unable to open file\n");
            exit(1);
        }
        FILE *fle;
        fle = fopen("foodep.txt", "r");
        char line[100];
        fgets(line, 100, fle);
        fclose(fle);
        int nodes=atoi(line);
        for (int i = 0; i < nodes ; i++) {
            fputc('0', fp);
        }

        fclose(fp);
    }

    dfs(n);

    exit(0);
}