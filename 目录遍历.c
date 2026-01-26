#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// 目录递归遍历实现
// 类似于tree命令的简化版
// 写ls的时候顺便写的这个
// 重点：处理符号链接，防止死循环！
// 这个坑我踩过好几次 结果程序卡死了

// 递归遍历目录
void BianLi(const char *LuJing, int ShenDu)
{
    // 先尝试打开目录
    DIR *MuLu = opendir(LuJing);
    if (!MuLu)
    {
        // 打不开可能是权限问题，跳过就行
        // perror(LuJing);  // 调试的时候打开这个 看看为什么失败
        return;
    }
    
    struct dirent *XiangMu;
    while ((XiangMu = readdir(MuLu)) != NULL)
    {
        // 跳过 . 和 ..
        // 防背刺：不跳过的话会无限循环！！
        // . 是当前目录，.. 是父目录
        // 这个坑我第一次写的时候就踩了 程序直接卡死
        if (strcmp(XiangMu->d_name, ".") == 0 || 
            strcmp(XiangMu->d_name, "..") == 0)
        {
            continue;
        }
        
        // 构建完整路径
        // 坑：路径拼接要注意加/，不然路径不对
        // 我之前忘了加/ 结果stat全失败了 调了好久
        char WanZhengLuJing[512];   // 512应该够了 不够再加
        snprintf(WanZhengLuJing, sizeof(WanZhengLuJing), "%s/%s", LuJing, XiangMu->d_name);
        
        // 打印缩进，让输出有层次感
        for (int i = 0; i < ShenDu; i++) printf("  ");
        
        struct stat WenJianXinXi;
        // 用lstat不用stat！！否则符号链接会被当成目标文件处理
        // 这个是重点！考试可能考
        if (lstat(WanZhengLuJing, &WenJianXinXi) == 0)
        {
            // 判断文件类型
            if (S_ISDIR(WenJianXinXi.st_mode))
            {
                // 是目录，递归进去
                printf("[目录] %s/\n", XiangMu->d_name);
                BianLi(WanZhengLuJing, ShenDu + 1);  // 递归进入子目录
            }
            else if (S_ISLNK(WenJianXinXi.st_mode))
            {
                // 防背刺：符号链接不递归！！
                // 不然遇到循环链接就死循环了，比如 a -> b -> a
                // 这个坑我也踩过 程序直接卡死
                printf("[链接] %s (跳过，避免循环)\n", XiangMu->d_name);
            }
            else
            {
                // 普通文件
                printf("[文件] %s\n", XiangMu->d_name);
            }
        }
        // else stat失败，可能是文件被删了或者权限问题，跳过
    }
    
    closedir(MuLu);  // 别忘了关闭！！不然文件描述符泄漏
    // 第一次写的时候忘了这个 结果打开太多文件就失败了
}

int main(int argc, char *argv[])
{
    // 没参数就遍历当前目录
    const char *LuJing = (argc > 1) ? argv[1] : ".";
    
    printf("遍历目录: %s\n", LuJing);
    printf("================\n");
    BianLi(LuJing, 0);
    printf("\n遍历完成!\n");  // 加个提示
    
    return 0;
}

// 用法: ./a.out [目录路径]
// 例如: ./a.out /home
//       ./a.out .
//       ./a.out     (不加参数就遍历当前目录)
//
// 踩坑记录：
// 1. 必须跳过 . 和 .. ，否则死循环
// 2. 必须用 lstat 而非 stat，否则无法识别  符号链接
// 3. 遇到符号链接不递归，否则循环链接会导致无限递归
// 4. closedir 别忘了
// 5. 路径拼接注意加 /，用snprintf比strcat安全
//
// 拓展思考：

// - 可以加个深度限制参数，防止递归太深栈溢出
// - 可以统计文件数量、目录数量等

