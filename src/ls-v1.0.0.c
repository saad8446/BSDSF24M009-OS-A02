/*
 * Programming Assignment 02: ls-v1.1.0
 * Added: -l (long listing format)
 * Usage:
 *       $ ls
 *       $ ls -l
 *       $ ls -l /home /etc
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
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern int errno;

void do_ls(const char *dir, int long_flag);
void do_ls_long(const char *dir);
void mode_to_string(mode_t mode, char *str);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [dir...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", long_flag);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], long_flag);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int long_flag)
{
    if (long_flag)
    {
        do_ls_long(dir);
        return;
    }

    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }
    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
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