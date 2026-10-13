\# Operating Systems - Programming Assignment 02: Report



\*\*Roll No:\*\* BSDSF24M009

\*\*Project:\*\* Re-engineering the `ls` utility in C



\---



\## Feature 2: ls-v1.1.0 - Complete Long Listing Format



\### Q1. What is the crucial difference between `stat()` and `lstat()`? When is `lstat()` more appropriate in `ls`?



If the path is a symbolic link, `stat()` follows the link and returns information about the \*\*target file\*\*. `lstat()` does not follow it and returns information about the \*\*link itself\*\*. For regular files and directories they behave the same.



| | `stat()` | `lstat()` |

|---|---|---|

| Path is a symlink | Info about the target | Info about the link |

| `S\_ISLNK(st\_mode)` | Never true | True for links |

| `st\_size` of a link | Size of the target | Length of the path stored in the link |

| Broken link | Fails with `ENOENT` | Works |



`lstat()` is more appropriate in `ls` almost always. `ls -l` must show a link as a link: type `l`, its own permissions and size, and `name -> target`. With `stat()` the link would look like its target, and a broken link would produce an error. `lstat()` is also needed to color symlinks (v1.5.0) and to avoid following links into directories during `-R` (v1.6.0), which could cause infinite loops.



\### Q2. How can bitwise operators and macros extract file type and permissions from `st\_mode`?



`st\_mode` is a bit field. The upper bits store the \*\*file type\*\* and the lower bits store the \*\*permissions and special bits\*\*.



\*\*File type:\*\* mask with `S\_IFMT`, then compare with a type constant:



```c

if ((st.st\_mode \& S\_IFMT) == S\_IFDIR)

&#x20;   printf("directory");

```



The macros `S\_ISDIR()`, `S\_ISREG()` and `S\_ISLNK()` do the same in shorter form.



\*\*Permissions:\*\* AND `st\_mode` with a single-bit constant. A nonzero result means the bit is set:



```c

if (st.st\_mode \& S\_IRUSR)   /\* owner can read \*/

```



| Macro | Meaning |

|---|---|

| `S\_IRUSR`, `S\_IWUSR`, `S\_IXUSR` | Owner read, write, execute |

| `S\_IRGRP`, `S\_IWGRP`, `S\_IXGRP` | Group read, write, execute |

| `S\_IROTH`, `S\_IWOTH`, `S\_IXOTH` | Others read, write, execute |

| `S\_ISUID`, `S\_ISGID`, `S\_ISVTX` | setuid, setgid, sticky bit |



In my code, `mode\_to\_string()` tests each bit with `\&` and writes `r`, `w`, `x` or `-` into a 10-character string. The first character comes from the type macros. The special bits change `x` into `s` or `t` (or `S` or `T` when execute is not set).



\---



\## Feature 3: ls-v1.2.0 - Column Display (Down Then Across)



\### Q1. Explain the logic of "down then across" printing. Why is a single loop insufficient?



Steps:



1\. Read all filenames into an array and find the longest name length (`maxlen`).

2\. Get the terminal width.

3\. Column width = `maxlen + 2`.

4\. Number of columns = `terminal\_width / column\_width` (at least 1).

5\. Number of rows = `ceil(total\_files / columns)`.

6\. Print row by row. For row `r` and column `c`, the item index is `r + c \* rows`. Stop when the index is past the last file.



Example with 7 files and 3 columns (3 rows):



```

0  3  6

1  4

2  5

```



A single loop is insufficient because it would print indices 0, 1, 2, 3... in order, which gives "across" ordering. In "down then across", neighbours on the same row are `rows` positions apart in the array. Also, the first row already contains items from every column, so the number of rows and columns must be known before printing anything, and the program must jump through the array (`r`, `r + rows`, `r + 2\*rows`, ...) instead of walking it sequentially.



\### Q2. What is the purpose of `ioctl`? What are the limits of a fixed 80-column width?



`ioctl(STDOUT\_FILENO, TIOCGWINSZ, \&w)` asks the terminal for its current size and fills a `struct winsize`. The field `w.ws\_col` gives the number of columns, which is used to calculate how many filename columns fit on a line.



Limitations of using only a fixed width of 80:



\- On a wider terminal, space is wasted because fewer columns are used than could fit.

\- On a narrower terminal, lines exceed the width and wrap, which breaks the alignment.

\- If the window is resized, the layout does not adapt.

\- The output differs from the real `ls`, which always adapts to the terminal.



In my program, `ioctl` is used first. If it fails (for example when output is piped) or returns 0, the program falls back to 80.



\---



\## Feature 4: ls-v1.3.0 - Horizontal Column Display (-x)



\### Q1. Compare the complexity of vertical and horizontal printing. Which needs more pre-calculation and why?



The vertical ("down then across") layout is more complex and needs more pre-calculation.



| | Vertical (default) | Horizontal (`-x`) |

|---|---|---|

| Pre-calculation | Column width, number of columns, and number of rows | Column width only |

| Index logic | `idx = r + c \* rows`, jumping through the array | Sequential, `i = 0..n-1` |

| Loops | Nested (rows outside, columns inside) | One loop |

| Wrapping | Decided up front by the row count | Decided on the fly using a position counter |

| Edge cases | Last column may be shorter, must stop when `idx >= n` | None |



Vertical needs more pre-calculation because the first row already contains items from every column, so the program must know the total number of files, the column count and the row count before it prints anything. The horizontal version prints items in array order and only asks for each one whether it fits on the current line.



\### Q2. How were the display modes (-l, -x, default) managed? How did the program decide which function to call?



I used one `enum display\_mode` variable:



```c

enum display\_mode { MODE\_DEFAULT, MODE\_LONG, MODE\_HORIZONTAL };

```



`main()` starts with `MODE\_DEFAULT`. The `getopt(argc, argv, "lx")` loop sets `MODE\_LONG` for `-l` and `MODE\_HORIZONTAL` for `-x` (if both are given, the last one wins). The mode is passed to `do\_ls()`, which reads and sorts the names and then chooses the function:



| Mode | Function called |

|---|---|

| `MODE\_LONG` | `do\_ls\_long()` |

| `MODE\_HORIZONTAL` | `print\_horizontal()` |

| `MODE\_DEFAULT` | `print\_columns()` (down then across) |



One enum variable makes it impossible to be in two modes at once, the names keep the code readable, and a new mode only needs a new enum value and one extra branch in `do\_ls()`.



\---



\## Feature 5: ls-v1.4.0 - Alphabetical Sort



\### Q1. Why must all entries be read into memory before sorting? What are the drawbacks for directories with millions of files?



`readdir()` returns one entry at a time in an arbitrary filesystem order. Sorting needs to compare entries with each other, so the whole list must be available in memory before the first line can be printed.



Drawbacks for millions of files:



\- High memory use (each name string plus a pointer in the array).

\- No output appears until every entry has been read and sorted, so there is a long delay.

\- Sorting costs O(n log n) time.

\- Growing the array with `realloc` adds overhead.



An unsorted `ls` could print each entry immediately and use constant memory.



\### Q2. What is the purpose and signature of the `qsort()` comparison function? Why `const void \*`?



`qsort()` is generic and does not know the type of the elements, so the caller supplies a function that compares two elements:



```c

int compare\_names(const void \*a, const void \*b)

{

&#x20;   const char \*s1 = \*(const char \*\*)a;

&#x20;   const char \*s2 = \*(const char \*\*)b;

&#x20;   return strcmp(s1, s2);

}

```



It returns a negative value if `a` should come before `b`, zero if they are equal, and a positive value if `a` should come after `b`. `qsort` passes pointers \*\*to the array elements\*\*. Since each element is a `char \*`, the pointer is a `char \*\*`, so the function casts it and dereferences it to get the string. The parameters are `const void \*` so that one `qsort` can sort any type, and `const` promises that the comparison will not modify the data.



Because `strcmp` compares ASCII values, uppercase names sort before lowercase names (for example `Makefile` before `bin`).



\---



\## Feature 6: ls-v1.5.0 - Colorized Output



\### Q1. How do ANSI escape codes produce color? Show the sequence for green.



The terminal treats text that starts with the escape character (`\\033`) followed by `\[` as a control command instead of text to display. The format is `\\033\[<code>m`, where the code selects a color or style (31 red, 32 green, 34 blue, 0 reset). The terminal applies it to all text that follows until it is reset.



Green text:



```c

printf("\\033\[0;32m%s\\033\[0m", "hello");

```



`\\033\[0;32m` starts green, then comes the text, and `\\033\[0m` resets the color to default.



\### Q2. Which bits of `st\_mode` show that a file is executable?



The execute bits are `S\_IXUSR` (owner), `S\_IXGRP` (group) and `S\_IXOTH` (others). Testing `st\_mode \& S\_IXUSR` and so on gives a nonzero result when that class has execute permission. My code colors a file green if any of the three is set:



```c

if (m \& (S\_IXUSR | S\_IXGRP | S\_IXOTH))

```



\---



\## Feature 7: ls-v1.6.0 - Recursive Listing (-R)



\### Q1. What is a "base case"? What is it in the recursive `ls`?



The base case is the condition under which a recursive function stops calling itself, so the recursion terminates. In `ls -R`, the base case is a directory that contains \*\*no subdirectories\*\*: the loop finds no entry for which `S\_ISDIR()` is true, so no further `do\_ls()` call is made and the function returns. Skipping `.` and `..` is also essential, since otherwise every directory would recurse into itself and its parent forever. Using `lstat()` instead of `stat()` also prevents following symlinks that point back to a parent directory.



\### Q2. Why must a full path be built before the recursive call? What happens with `do\_ls("subdir")`?



`readdir()` returns only the entry name (`subdir`), not its location. `opendir()` and `lstat()` resolve a relative name against the \*\*current working directory\*\*, not against the directory being listed. Calling `do\_ls("subdir")` from inside `do\_ls("parent\_dir")` would look for `./subdir`, which usually does not exist (so it prints "Cannot open directory"), or it would list a different directory that happens to have the same name in the current directory. Building `parent\_dir/subdir` gives the correct path regardless of the working directory, and the header printed for each directory also shows the full path.

