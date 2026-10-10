/*
 * Programming Assignment 02: ls-v1.3.0
 * Added: -l (long listing), column display (down then across),
 *        -x (horizontal display)
 * Usage:
 *       $ ls
 *       $ ls -l
 *       $ ls -x
 *       $ ls /home /etc
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern int errno;

/* Display modes */
enum display_mode
{
    MODE_DEFAULT,   /* down then across */
    MODE_LONG,      /* -l */
    MODE_HORIZONTAL /* -x */
};

void do_ls(const char *dir, enum display_mode mode);
void do_ls_long(const char *dir);
void mode_to_string(mode_t mode, char *str);
int  read_names(const char *dir, char ***names_out, int *maxlen);
void print_columns(char **names, int n, int maxlen);
void print_horizontal(char **names, int n, int maxlen);
void free_names(char **names, int n);
int  get_term_width(void);

int main(int argc, char *argv[])
{
    int opt;
    enum display_mode mode = MODE_DEFAULT;

    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
        case 'l':
            mode = MODE_LONG;
            break;
        case 'x':
            mode = MODE_HORIZONTAL;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l | -x] [dir...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], mode);
            puts("");
        }
    }
    return 0;
}

/* Decide which display function to call, based on the mode flag */
void do_ls(const char *dir, enum display_mode mode)
{
    if (mode == MODE_LONG)
    {
        do_ls_long(dir);
        return;
    }

    char **names = NULL;
    int maxlen = 0;
    int n = read_names(dir, &names, &maxlen);
    if (n < 0)
        return;

    if (mode == MODE_HORIZONTAL)
        print_horizontal(names, n, maxlen);
    else
        print_columns(names, n, maxlen);

    free_names(names, n);
}

/* Read all non-hidden names into a dynamic array; track the longest name */
int read_names(const char *dir, char ***names_out, int *maxlen)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return -1;
    }

    int capacity = 64, count = 0;
    char **names = malloc(capacity * sizeof(char *));
    if (names == NULL)
    {
        perror("malloc");
        closedir(dp);
        return -1;
    }

    *maxlen = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (count == capacity)
        {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc");
                free_names(names, count);
                closedir(dp);
                return -1;
            }
            names = tmp;
        }

        names[count] = strdup(entry->d_name);
        int len = (int)strlen(entry->d_name);
        if (len > *maxlen)
            *maxlen = len;
        count++;
    }
    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
    *names_out = names;
    return count;
}

/* Get terminal width using ioctl; fall back to 80 */
int get_term_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80;
    return w.ws_col;
}

/* "Down then across": fill each column top to bottom */
void print_columns(char **names, int n, int maxlen)
{
    if (n == 0)
        return;

    int term_width = get_term_width();
    int col_width = maxlen + 2;                 /* 2 spaces between columns */
    int cols = term_width / col_width;
    if (cols < 1)
        cols = 1;
    int rows = (n + cols - 1) / cols;           /* ceiling division */

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int idx = r + c * rows;
            if (idx >= n)
                break;
            /* pad every item except the last one on the line */
            if (idx + rows < n)
                printf("%-*s", col_width, names[idx]);
            else
                printf("%s", names[idx]);
        }
        printf("\n");
    }
}

/* "Across": fill left to right, wrap when the line is full (-x) */
void print_horizontal(char **names, int n, int maxlen)
{
    if (n == 0)
        return;

    int term_width = get_term_width();
    int col_width = maxlen + 2;
    int pos = 0;                                /* current horizontal position */

    for (int i = 0; i < n; i++)
    {
        /* would this item run past the end of the line? */
        if (pos > 0 && pos + col_width > term_width)
        {
            printf("\n");
            pos = 0;
        }
        printf("%-*s", col_width, names[i]);
        pos += col_width;
    }
    printf("\n");
}

void free_names(char **names, int n)
{
    for (int i = 0; i < n; i++)
        free(names[i]);
    free(names);
}

/* Convert st_mode into a string like "drwxr-xr-x" */
void mode_to_string(mode_t mode, char *str)
{
    /* file type */
    if (S_ISDIR(mode))       str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '-';

    /* owner */
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    /* group */
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    /* others */
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    /* special bits */
    if (mode & S_ISUID)
        str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
        str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX)
        str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

/* Long listing format (-l) */
void do_ls_long(const char *dir)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];
    long total = 0;
    int w_link = 0, w_user = 0, w_group = 0, w_size = 0;

    /* Pass 1: find the widest value in each column, and the total blocks */
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }
        total += st.st_blocks;

        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);

        int l = snprintf(NULL, 0, "%lu", (unsigned long)st.st_nlink);
        if (l > w_link) w_link = l;

        l = pw ? (int)strlen(pw->pw_name) : snprintf(NULL, 0, "%u", st.st_uid);
        if (l > w_user) w_user = l;

        l = gr ? (int)strlen(gr->gr_name) : snprintf(NULL, 0, "%u", st.st_gid);
        if (l > w_group) w_group = l;

        l = snprintf(NULL, 0, "%lld", (long long)st.st_size);
        if (l > w_size) w_size = l;
    }

    printf("total %ld\n", total / 2);   /* st_blocks counts 512-byte units; ls shows 1K */

    /* Pass 2: print each entry */
    rewinddir(dp);
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
            continue;

        char perms[11];
        mode_to_string(st.st_mode, perms);

        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);
        char user[32], group[32];
        if (pw) snprintf(user, sizeof(user), "%s", pw->pw_name);
        else    snprintf(user, sizeof(user), "%u", st.st_uid);
        if (gr) snprintf(group, sizeof(group), "%s", gr->gr_name);
        else    snprintf(group, sizeof(group), "%u", st.st_gid);

        /* ctime() gives "Wed Jun 30 21:49:08 1993\n"; we want "Jun 30 21:49" */
        char *t = ctime(&st.st_mtime);

        printf("%s %*lu %-*s %-*s %*lld %.12s %s",
               perms,
               w_link, (unsigned long)st.st_nlink,
               w_user, user,
               w_group, group,
               w_size, (long long)st.st_size,
               t + 4,
               entry->d_name);

        /* symbolic link: show where it points */
        if (S_ISLNK(st.st_mode))
        {
            char target[PATH_MAX];
            ssize_t n = readlink(path, target, sizeof(target) - 1);
            if (n != -1)
            {
                target[n] = '\0';
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }

    closedir(dp);
}