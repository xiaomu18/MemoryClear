// MemoryClear.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment (lib,"psapi.lib")
#pragma comment (lib,"Advapi32.lib")
#define SE_INCREASE_QUOTA_NAME  TEXT("SeIncreaseQuotaPrivilege")

bool EnableSpecificPrivilege(LPCTSTR lpPrivilegeName)
{
    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES Token_Privilege;
    BOOL bRet = TRUE;
    do
    {
        if (0 == OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
            //OutputDebugView(_T("OpenProcessToken Error"));
            bRet = FALSE;
            break;
        }
        if (0 == LookupPrivilegeValue(NULL, lpPrivilegeName, &Token_Privilege.Privileges[0].Luid))
        {
            //OutputDebugView(_T("LookupPrivilegeValue Error"));
            bRet = FALSE;
            break;
        }
        Token_Privilege.PrivilegeCount = 1;
        Token_Privilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        //Token_Privilege.Privileges[0].Luid.LowPart=17;//SE_BACKUP_PRIVILEGE  
        //Token_Privilege.Privileges[0].Luid.HighPart=0;  
        if (0 == AdjustTokenPrivileges(hToken, FALSE, &Token_Privilege, sizeof(Token_Privilege), NULL, NULL))
        {
            //OutputDebugView(_T("AdjustTokenPrivileges Error"));
            bRet = FALSE;
            break;
        }
    } while (false);
    if (NULL != hToken)
    {
        CloseHandle(hToken);
    }
    return bRet;
}

bool AdjustTokenPrivilegesForNT()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // Get a token for this process.

    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);

    // Get the LUID for the EmptyWorkingSet privilege.

    LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);

    tkp.PrivilegeCount = 1; // one privilege to set    
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Get the EmptyWorkingSet privilege for this process.

    return AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
}

int EmptyAllSet()
{
    HANDLE SnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (SnapShot == NULL)
    {
        return 0;
    }

    PROCESSENTRY32 ProcessInfo;//声明进程信息变量
    ProcessInfo.dwSize = sizeof(ProcessInfo);//设置ProcessInfo的大小

    //返回系统中第一个进程的信息
    BOOL Status = Process32First(SnapShot, &ProcessInfo);

    int i = 0;

    while (Status)
    {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, ProcessInfo.th32ProcessID);

        if (hProcess)
        {
            //printf("正在清理进程ID：%d 的内存\n",ProcessInfo.th32ProcessID);
            SetProcessWorkingSetSize(hProcess, -1, -1);
            //内存整理
            EmptyWorkingSet(hProcess);
            CloseHandle(hProcess);

            i++; // 记录已清理的进程数
        }

        //获取下一个进程的信息
        Status = Process32Next(SnapShot, &ProcessInfo);
    }

    return i;
}

// 获取当前进程是否运行在管理员权限下
BOOL IsProcessElevated()
{
    BOOL fIsElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);

        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize))
        {
            fIsElevated = Elevation.TokenIsElevated;
        }
    }
    if (hToken)
    {
        CloseHandle(hToken);
    }
    return fIsElevated;
}

int main(int argc, char* argv[])
{
    printf("\n [*] MemoryClear v0.2 For Windows NT\n");

    if (argc < 2)
    {
        printf("\n [!] 用法：");
        printf("\n [!] EXE clean 立即释放内存 (清理工作集 + 缓存在内存的文件)");
        printf("\n [!] EXE clean [int] 每隔 int 秒清理一次内存 (持续运行，按 Ctrl^C 停止)");
        printf("\n [!] 每个步骤提示后的 [1] 表示执行成功，[0] 则表示执行失败");
        printf("\n");

        return 0;
    }

    bool IsDelayClean = FALSE;
    int delayTime;

    std::string firstArg = argv[1];

    if (firstArg != "clean")
    {
        printf("\n [!] 未知的选项参数");
        printf("\n");

        return -1;
    }

    if (argc >= 3)
    {
        IsDelayClean = TRUE;

        std::string intArg = argv[2];

        try 
        {
            // 尝试将参数转换为整数
            delayTime = std::stoi(intArg);
        }
        catch (std::invalid_argument&)
        {
            printf("\n [!] 请在 [int] 处输入正确的数字");
            printf("\n");

            return -1;
        }
    }

    if (!IsProcessElevated())
    {
        printf("\n [!] 本工具需要管理员权限，请使用管理员权限运行\n");

        return -1;
    }

    // 先进行系统提权
    printf("\n [!] AdjustTokenPrivilegesForNT 提权 [%d]", AdjustTokenPrivilegesForNT());
    printf("\n [!] SE_INCREASE_QUOTA_NAME 提权 [%d]", EnableSpecificPrivilege(SE_INCREASE_QUOTA_NAME));

    printf("\n");

    printf("\n [!] 开始清理工作集");
    
    printf("\n [!] 成功清理 %d 个进程的工作集", EmptyAllSet());

    printf("\n [!] 清理内存文件缓存 [%d]", SetSystemFileCacheSize(-1, -1, 0));

    while (IsDelayClean)
    {
        printf("\n\n [!] 延迟 %d 秒执行下一次清理\n", delayTime);

        Sleep(delayTime * 1000);

        printf("\n [!] 开始清理工作集");

        printf("\n [!] 成功清理 %d 个进程的工作集", EmptyAllSet());

        printf("\n [!] 清理内存文件缓存 [%d]", SetSystemFileCacheSize(-1, -1, 0));
    }

    printf("\n [!] 清理成功, 自动退出！");
    
    printf("\n");

    //getchar();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
