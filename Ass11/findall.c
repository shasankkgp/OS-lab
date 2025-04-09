#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    char *username;
    unsigned int user_id;
} UserRecord;

UserRecord *user_database = NULL;
int database_size = 0;

void initialize_user_database() {
    char buffer[2048];
    FILE *passwd = fopen("/etc/passwd", "r");
    
    if (!passwd) {
        fprintf(stderr, "Cannot access user database: %s\n", strerror(errno));
        return;
    }

    /*
        the structure of every line is 
            username:x:UID:GID:comment:home_directory:shell
    */
    
    while (fgets(buffer, sizeof(buffer), passwd)) {
        char *name_ptr = buffer;
        char *delimiter = strchr(name_ptr, ':');
        if (!delimiter) continue;
        
        *delimiter = '\0';
        char *id_start = delimiter + 1;
        
        delimiter = strchr(id_start, ':');
        if (!delimiter) continue;
        
        id_start = delimiter + 1;
        delimiter = strchr(id_start, ':');
        if (!delimiter) continue;
        
        *delimiter = '\0';
        unsigned int id_value = atoi(id_start);
        
        int new_size = database_size + 1;
        UserRecord *temp = realloc(user_database, new_size * sizeof(UserRecord));
        
        if (!temp) {
            fprintf(stderr, "Failed to allocate memory\n");
            fclose(passwd);
            return;
        }
        
        user_database = temp;
        user_database[database_size].username = strdup(name_ptr);
        user_database[database_size].user_id = id_value;
        database_size = new_size;
    }
    
    fclose(passwd);
}

char* find_username(unsigned int id) {
    static char id_buffer[64];
    
    for (int index = 0; index < database_size; index++) {
        if (user_database[index].user_id == id) {
            return user_database[index].username;
        }
    }
    
    struct passwd *pwd_entry = getpwuid(id);
    if (pwd_entry && pwd_entry->pw_name) {
        return pwd_entry->pw_name;
    }
    
    snprintf(id_buffer, sizeof(id_buffer), "%u", id);
    return id_buffer;
}

int check_file_type(const char *name, const char *suffix) {
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    
    if (name_len <= suffix_len) return 0;
    
    const char *file_ext = name + (name_len - suffix_len);
    if (*(file_ext - 1) != '.') return 0;
    
    return strcasecmp(file_ext, suffix) == 0;
}

void explore_files(const char *base_path, const char *file_type, int *matches) {
    struct dirent *item;
    struct stat file_info;
    char full_path[4096];
    
    DIR *folder = opendir(base_path);
    if (!folder) {
        fprintf(stderr, "Cannot access directory %s: %s\n", base_path, strerror(errno));
        return;
    }
    
    while ((item = readdir(folder))) {

        if (item->d_name[0] == '.' && 
            (item->d_name[1] == '\0' || 
             (item->d_name[1] == '.' && item->d_name[2] == '\0'))) {
            continue;
        }
        
        strcpy(full_path, base_path);
        size_t path_len = strlen(full_path);
        
        if (full_path[path_len-1] != '/') {
            full_path[path_len] = '/';
            full_path[path_len+1] = '\0';
            path_len++;
        }
        
        strcat(full_path, item->d_name);
        
        if (lstat(full_path, &file_info) != 0) {
            fprintf(stderr, "Cannot get info for %s: %s\n", full_path, strerror(errno));
            continue;
        }
        
        if (S_ISREG(file_info.st_mode)) {
            if (check_file_type(item->d_name, file_type)) {
                (*matches)++;
                printf("%-4d : %-10s %-12ld %s\n", 
                       *matches, 
                       find_username(file_info.st_uid),
                       (long)file_info.st_size, 
                       full_path);
            }
        } else if (S_ISDIR(file_info.st_mode)) {
            explore_files(full_path, file_type, matches);
        }
    }
    
    closedir(folder);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <directory> <file_extension>\n", argv[0]);
        return 1;
    }
    
    initialize_user_database();
    
    printf("%-4s : %-10s %-12s %s\n", "NO", "OWNER", "SIZE", "NAME");
    printf("%-4s : %-10s %-12s %s\n", "---", "----", "-----", "--------");
    
    int result_count = 0;
    explore_files(argv[1], argv[2], &result_count);
    
    printf("+++ %d files match the extension %s\n", result_count, argv[2]);
    
    for (int i = 0; i < database_size; i++) {
        free(user_database[i].username);
    }
    free(user_database);
    
    return 0;
}
