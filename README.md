从4.0开始，我们使用了新技术自动hook系统，由于该机制可以用于补丁其他游戏，故新架构闭源开发,不希望这些代码被用于其他游戏

而且对应的注入器，exe pattern补丁器从此停止更新

只会分发mcpatcher_core.dll（这个dll会被注入到游戏）

本项目目前作为MCUnlock++[https://www.bilibili.com/video/BV14CcweLExe]上游（推荐小白使用）


其他注入器推荐：

winmm_hijack winmm.x64.dll[https://github.com/fly-studio/winmm_hijack/releases/download/v1.1/winmm.x64.dll],把下载的winmm.x64.dll命名为winmm.dll mcpatcher_core.dll命名为winmm.mcpatcher_core.dll


Levilamina[https://github.com/LiteLDev/LeviLamina/] preload-native

1安装LeviLauncher[https://github.com/LiteLDev/LeviLauncher]

2安装支持Levilamina client的版本

3在mods下创建MCPatcher文件夹，创建manifest.json如下

{

  "name": "MCPatcher",
  
  "entry": "mcpatcher_core.dll",
  
  "type": "preload-native"

}

然后拷贝mcpatcher_core.dll到MCPatcher文件夹


本项目旧版注入器（只需替换旧版release里面的dll），不推荐，停更


89 少吃顿必胜客大批萨就行，不贵，何况还有java版账号（hypixel 2b2t 大量知名服务器进入权，和社区身份（正版id就是））

本项目仅供拿不出89的学生党（比如我当年（零花钱管得太死），如今2个正版号）
