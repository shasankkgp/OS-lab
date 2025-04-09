#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>

// Structure to store UID to login ID mapping
typedef struct {
    uid_t uid;
    char *login;
} UidMapping;

UidMapping *uid_map = NULL;
int uid_map_size = 0;

// Function to load user information from /etc/passwd
void load_user_mappings() {
    FILE *passwd_file = fopen("/etc/passwd", "r");
    if (!passwd_file) {
        perror("Error opening /etc/passwd");
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), passwd_file)) {
        char *username = strtok(line, ":");
        if (!username) continue;
        
        strtok(NULL, ":"); // Skip password field
        char *uid_str = strtok(NULL, ":");
        if (!uid_str) continue;
        
        uid_t uid = atoi(uid_str);
        
        uid_map = realloc(uid_map, (uid_map_size + 1) * sizeof(UidMapping));
        if (!uid_map) {
            perror("Memory allocation failed");
            fclose(passwd_file);
            return;
        }
        
        uid_map[uid_map_size].uid = uid;
        uid_map[uid_map_size].login = strdup(username);
        uid_map_size++;
    }
    
    fclose(passwd_file);
}

// Function to get login ID from UID
const char *get_login_from_uid(uid_t uid) {
    for (int i = 0; i < uid_map_size; i++) {
        if (uid_map[i].uid == uid) {
            return uid_map[i].login;
        }
    }
    
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return pw->pw_name;
    }
    
    static char uid_str[32];
    sprintf(uid_str, "%u", uid);
    return uid_str;
}

// Function to check if a file has the given extension
int has_extension(const char *filename, const char *ext) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return 0;
    return strcmp(dot + 1, ext) == 0;
}

// Function to recursively search directories
void search_directory(const char *dir_path, const char *ext, int *file_count) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s: %s\n", dir_path, strerror(errno));
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (lstat(path, &st) == -1) {
            fprintf(stderr, "Error accessing %s: %s\n", path, strerror(errno));
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            search_directory(path, ext, file_count);
        } 
        else if (S_ISREG(st.st_mode) && has_extension(entry->d_name, ext)) {
            (*file_count)++;
            printf("%-4d : %-10s %-12ld %s\n", 
                   *file_count, 
                   get_login_from_uid(st.st_uid),
                   (long)st.st_size, 
                   path);
        }
    }
    
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directory> <extension>\n", argv[0]);
        return 1;
    }
    
    const char *dir_name = argv[1];
    const char *extension = argv[2];
    
    load_user_mappings();
    
    printf("NO   : OWNER      SIZE         NAME\n");
    printf("--   : -----      ----         ----\n");
    
    int file_count = 0;
    search_directory(dir_name, extension, &file_count);
    
    printf("+++ %d files match the extension %s\n", file_count, extension);
    
    for (int i = 0; i < uid_map_size; i++) {
        free(uid_map[i].login);
    }
    free(uid_map);
    
    return 0;
}
