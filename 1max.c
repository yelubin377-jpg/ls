#include <dirent.h>//opendir readdir closedir 函数（用来打开文件夹的工具）没有有的话打不开文件夹
#include <stdio.h>//有个叫做perror的函数
#include <stdlib.h>//malloc free 之类的一堆和开辟内存有关的函数
#include <string.h>//strcpy strcmp snprintf 之类的工具函数
#include <sys/stat.h>//stat 函数和 struct stat 结构体之类的
#include <sys/types.h>//文件大小 权限 类型（普通文件 / 目录）
#include <unistd.h>//Linux VIP专属（系统调用）
#include <pwd.h>//getpwuid 整用户名的
#include <grp.h>//getgrgid 整组名的
#include <time.h>//时间相关的 such as time_t localtime strftime等
#include <linux/limits.h>//系统限制  PATH_MAX：路径最长是多少

//跟51单片机作的矩形键盘差不多，都是0关1开
int a = 0;  // -a         显是所有文件，包括隐藏文件
int l = 0;  // -l        长格式显示（一堆碎碎念的什么大小、修改时间等等）
int R = 0;  // -R       递归显示子目录（世界数一样疯狂分叉，疯狂拆套娃）
int t = 0;  // -t         看谁update时间长，谁靠前面
int r = 0;  // -r        把顺序反过来
int i = 0;  // -i        inode
int s = 0;  // -s       显示文件大小

#define MAX_DEPTH 256           // 最大递归深度
#define MAX_VISITED 8192        // 【改进1】最大已访问路径数：4096 → 8192

// 已访问路径记录（用realpath检测循环）
static char *visited_paths[MAX_VISITED];
static int visited_count = 0;

// 检查路径是否已访问
int is_visited(const char *real_path) 
{
    for (int k = 0; k < visited_count; k++) 
    {
        if (visited_paths[k] && strcmp(visited_paths[k], real_path) == 0)
            return 1;
    }
    return 0;
}

// 添加已访问路径
void add_visited(const char *real_path) 
{
    if (visited_count < MAX_VISITED) 
    {
        char *dup = strdup(real_path);
        if (dup)  // 只有成功才添加
        {
            visited_paths[visited_count] = dup;
            visited_count++;
        }
    }
}

// 清理已访问路径
void clear_visited() 
{
    for (int k = 0; k < visited_count; k++) 
    {
        free(visited_paths[k]);
        visited_paths[k] = NULL;
    }
    visited_count = 0;
}

// 【改进2】防一手特殊伪文件系统（/proc /sys /dev）
int is_special_fs(const char *path) 
{
    // 精确匹配这三个目录及其子目录
    if (strncmp(path, "/proc", 5) == 0 && (path[5] == '/' || path[5] == '\0'))
        return 1;
    if (strncmp(path, "/sys", 4) == 0 && (path[4] == '/' || path[4] == '\0'))
        return 1;
    if (strncmp(path, "/dev", 4) == 0 && (path[4] == '/' || path[4] == '\0'))
        return 1;
    return 0;
}

int yincangwenjian(char *File)// 隐藏 or not
{
    if(File[0] == '.')
    {
        return 1;
    }
    else
    {
        return 0;
    }                  //如果第一个字符是 . 那就是隐藏文件，用来判断是否隐藏文件，十九返回1，否则返回0
}

// 把目录列出来
void work(char *Folder, int depth)
{
    // 深度限制，防止栈溢出
    if (depth > MAX_DEPTH) 
    {
        fprintf(stderr, "XUPT WARNING:递归深度超过%d,跳过 %s\n", MAX_DEPTH, Folder);
        return;
    }

    // 【改进2】跳过特殊伪文件系统
    if (is_special_fs(Folder))
    {
        return;  // 直接跳过 /proc /sys /dev
    }

    DIR *dir;//指向一个 目录结构体
    struct dirent *ent;//设置一个ent结构体,存储文件夹的信息
    struct stat st;//存储文件的详细信息
    char buf[PATH_MAX];//创建一个字符数组，代谢是PATH_MAX大小的，（4096），用来存储文件路径

    dir = opendir(Folder);//打开文件夹，返回一个指向该文件夹的指针
    if (dir == NULL) 
    {
        perror("opendir");  //打印错误信息
        return;
    }

    // 使用固定大小的数组存储文件信息
    struct WenJianMessage
    {
        char name[256];//命名文件
        time_t mod_time;//修改时间
        ino_t inode_num;//inode编号，文件身份证号，一般是unsigned long类型
        off_t file_size;//文件大小，一般是long类型
    };

    struct WenJianMessage *entries = (struct WenJianMessage *)malloc(sizeof(struct WenJianMessage) * 1024);
    if (!entries) 
    {
        perror("malloc");
        closedir(dir);
        return;
    }
    int jishuqi = 0;//  文件技术器

    // embark on reading目录
    while ((ent = readdir(dir)) != NULL)// NULL是暗号，读完了，即没读完的时候疯狂循环
    {
        if (!a && yincangwenjian(ent->d_name))//如果不是隐藏文件，除了-a不会显示隐藏文件
        {
            continue;
        }
        strncpy(entries[jishuqi].name, ent->d_name, sizeof(entries[jishuqi].name) - 1);
        entries[jishuqi].name[sizeof(entries[jishuqi].name) - 1] = '\0';  // 确保以\0结尾

        snprintf(buf, PATH_MAX, "%s/%s", Folder, ent->d_name);//防一手溢出,massage都放到buf里面暂存1
        if (stat(buf, &st) == 0)
        {
            entries[jishuqi].mod_time = st.st_mtime;
            entries[jishuqi].inode_num = st.st_ino;
            entries[jishuqi].file_size = st.st_size;
        }
        jishuqi++;                      // stat(路径, &结构体) 的意思是：
        if (jishuqi >= 1024)            // "请内核根据这个路径，
        {
            break;                  // 把文件的所有详细信息，
        }                           // 填进我提供的这个结构体里，   // 并告诉我成没成功。"


                
    }

    closedir(dir);

    // 排个序
    for (int x = 0; x < jishuqi - 1; x++)//for循环，把每一个按顺序依次往最后丢，故cnt-1
    //The last one无需排序
    {
        for(int y = 0; y < jishuqi - 1 - x; y++)
        ///!!!!!!!!!!!这里一定一定一定注意！！！不要用int i = 0来开启for循环，否则会根前面的-i指令冲突，虽然不会导致程序出错，但导致-i失效！！！！！
        //变量名重复是万恶之源！！！
        {
            int Need_JiaoHuan = 0;//判断是否需要交换位置，依旧0否1是

            if (t) // 按时间排序
            {
                if (!r && entries[y].mod_time > entries[y + 1].mod_time)
                {
                    Need_JiaoHuan = 1;
                }
                if (r && entries[y].mod_time < entries[y + 1].mod_time)
                {
                    Need_JiaoHuan = 1;
                }
            }
            else    // 按名称排序
            {
                if (!r && strcmp(entries[y].name, entries[y + 1].name) > 0)
                {
                    Need_JiaoHuan = 1;
                }//倒叙
                if (r && strcmp(entries[y].name, entries[y + 1].name) < 0)
                {
                    Need_JiaoHuan = 1;
                }//正常情况下名称较长，往后靠
            }

            if (Need_JiaoHuan)//冒泡
            {
                struct WenJianMessage temp = entries[y];
                entries[y ] = entries[y + 1]; 
                entries[y + 1] = temp;
            }
        }
    }

    // 输出
    for (int shunxv = 0; shunxv < jishuqi; shunxv++)
    {
        snprintf(buf, PATH_MAX, "%s/%s", Folder, entries[shunxv].name);
        if (stat(buf, &st) != 0)
        {
            continue;
        }

        if (i) 
        {
            printf("%lu ", (unsigned long)entries[shunxv].inode_num);
        }

        if (s) 
        {
            printf("%ld ", (long)entries[shunxv].file_size);
        }

        // 用了-i或-s，在数字和文件名之间添加空格
        if ((i || s) && !l) //注意！！！l必须不能用，because l已经有了长格式，有了空格
        {
            printf(" ");
        }

        if (l)
        {
            // 文件类型
            if (S_ISDIR(st.st_mode))//如果是目录
            {
                printf("d");//目录
            }
            else
            {
                printf("-");//文件
            }

            // 权限，判断一个个的目录的权限
            printf("%c%c%c%c%c%c%c%c%c", st.st_mode & S_IRUSR ? 'r' : '-',
                st.st_mode & S_IWUSR ? 'w' : '-',
                st.st_mode & S_IXUSR ? 'x' : '-',
                st.st_mode & S_IRGRP ? 'r' : '-',
                st.st_mode & S_IWGRP ? 'w' : '-',
                st.st_mode & S_IXGRP ? 'x' : '-',
                st.st_mode & S_IROTH ? 'r' : '-',
                st.st_mode & S_IWOTH ? 'w' : '-',
                st.st_mode & S_IXOTH ? 'x' : '-');

            printf("%lu ", (unsigned long)st.st_nlink);
            struct passwd *pw = getpwuid(st.st_uid);  //整 用户ID 用的
            struct group *gr = getgrgid(st.st_gid);//       组ID
            printf("%s %s ", pw ? pw->pw_name : "", gr ? gr->gr_name : "");//输出 
            printf("%8ld ", (long)st.st_size);//文件大小
            //time 板块
            char tbuf[64]; //存储时间字符串（格式化后的）
            struct tm *tmx = localtime(&st.st_mtime); //转换为本地时间
            strftime(tbuf, sizeof(tbuf), "%b %d %H:%M", tmx);
            //strftime(存放结果的缓冲区，大小，时间格式模板，元时间数据来源)；
            printf("%s ", tbuf);
        }

        //颜色 目录蓝，可执行文件（点开就蛞蝓用）绿
        if (S_ISDIR(st.st_mode))// 判 断是不是个目目录
        {
            printf("\033[1;34m%s\033[0m", entries[shunxv].name);//ANSI，亮蓝色。
        }                   //一定要记得重置颜色，不然就丸辣
        else if (st.st_mode & S_IXUSR)//康康是不是自己有执行权限的文件，有的话直接标记上
        {
            printf("\033[1;32m%s\033[0m", entries[shunxv].name);
        }
        else
        {
            printf("%s", entries[shunxv].name);
        }
        if (!l && (i || s))
        {
            // 不需要额外的空格，因为已经在输出inode/filesize时添加了
        }
        
        printf("\n");
    }

    // 递归
    if (R)
    {
        for (int Shu = 0; Shu < jishuqi; Shu++)
        {
            // 跳过 . 和 .. 避免无限循环，防止程序宕机ing
            if (!strcmp(entries[Shu].name, ".") || !strcmp(entries[Shu].name, ".."))
                continue;
            
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "%s/%s", Folder, entries[Shu].name);
            
            struct stat lst;
            if (lstat(path, &lst) != 0)
                continue;  // 获取信息失败则跳过
            
            // 跳过符号链接，避免无限递归
            if (S_ISLNK(lst.st_mode))
                continue;
            
            // 如果是目录，检测循环后递归
            if (S_ISDIR(lst.st_mode))
            {
                // 用realpath获取真实路径，检测循环
                char real_path[PATH_MAX];
                if (realpath(path, real_path) && !is_visited(real_path))
                {
                    add_visited(real_path);
                    printf("\n%s:\n", path);
                    work(path, depth + 1);  // 深度+1
                }
            }
        }
    }
    
    free(entries);  // 释放动态内存
}

int main(int argc, char *argv[])
{
    printf("(检测)这是我写的yls(max版本)而非系统自带\n");
    int ok = 0;
    
    //变量名一个个审判
    for (int shuzi = 1; shuzi < argc; shuzi++)
    ///!!!!!!!!!!!这里一定一定一定注意！！！不要用int i = 0来开启for循环，否则会根前面的-i指令冲突，虽然不会导致程序出错，但导致-i失效！！！！！
    {
        if (argv[shuzi][0] == '-')//-a -l，-是应该算一个开始的标志，准备诸葛读取后面的命令行 
        {
            for (int j = 1; argv[shuzi][j]; j++)
            {
                if (argv[shuzi][j] == 'a')
                {
                    a = 1;
                }
                else if (argv[shuzi][j] == 'l')
                {
                    l = 1;
                }
                else if (argv[shuzi][j] == 'R')
                {
                    R = 1;
                }
                else if (argv[shuzi][j] == 't')
                {
                    t = 1;
                }
                else if (argv[shuzi][j] == 'r')
                {
                    r = 1;
                }
                else if (argv[shuzi][j] == 'i')
                {
                    i = 1;
                }
                else if (argv[shuzi][j] == 's')
                {
                    s = 1;
                }//一个个看是不是，根单片机矩阵键盘一样，0关1开
            }
        }
        else
        {
            printf("%s:\n", argv[shuzi]);
            work(argv[shuzi], 0);
            ok = 1;
        }
    }

    // 如果没有指定目录，则处理当前目录（主要看有没有带参数）

    while (!ok)
    {
        work((char *)".", 0);//一定要换类型，不然以一直报错！～
        ok = 1; // 避免无限循环
    }
    //for example就是：
    //./ls2 （不带参数）→ 显示当前目录内容
    //./ls2 /home （带参数）→ 显示 /home 目录内容
    clear_visited();  // 清理已访问路径
    return 0;
}
