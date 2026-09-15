# AI 交接文档（游戏全部知识）

> 写于 2026-08-25，最近更新 2026-09-15（**Alpha v1.3.4 已发布**，见 §0）。本文档是"换一个 AI 继续开发"的完整交接材料：
> 项目是什么、做到哪了、怎么做的、坑在哪、还有什么没做。
> 阅读顺序建议：**0 最近一轮交接** → 1 项目现状 → 3 构建 → 4 架构 → 5 机制 → 8 配置 → 9 存档 → 11 坑 → 12 未完成。

---

## 0. ★ 最近一轮工作交接（2026-09-15，Alpha v1.3.4 已发布）

> 本节回答两个问题：**我干了啥 / 接下来要干啥**。先看结论：
> **v1.3.4 是一次"存档机制重构"版本：把自动保存彻底改为「手动保存 · 10 槽位」（参考《世界盒子》WorldBox）。核心改动：①`SaveSystem` 重写为 10 个独立槽位文件（`saves/slot_01.json`…`slot_10.json`），提供 `querySlot/anySlotUsed/newestSlot/saveGameToSlot/loadGameFromSlot/deleteSlot/migrateLegacySave` 与文件级 `saveGameToFile/loadGameFromFile`；②**移除所有自动保存路径**——新建世界/退出/返回主界面都不再写盘，只有玩家点「保存存档」或按 F5 才落盘，杜绝"新开一局关窗覆盖旧档"；③启动菜单「继续游戏」→「载入存档」、暂停面板 4 项→**5 项**（继续/保存/载入/设置/返回），两处都打开同一套 2×5 槽位管理页（显示空槽/存档时间/摘要，覆盖与删除前二次确认且默认焦点在「取消」）；④旧单档 `saves/factory_td.json` 首次启动**自动迁移到槽位 1**；⑤`F5`/`F9` 语义改为"打开槽位页"，不再直接读写盘；⑥`--selftest-save` 改用独立临时文件，不再触碰玩家槽位；⑦**建筑占地尺寸收敛为唯一数据源**——`cfg::BUILDING_INFOS` 增加 `w/h` + `cfg::buildingSize()`，审计确认全项目 22 种建筑此前仅旧版大功率发电机(索引10)曾误设为 2×2（**经确认该 2×2 为错误设定、现已统一改为 1×1，全项目无多格建筑**），并修正 4 处"按 1×1 处理"的占地逻辑（放置预览 / 面编辑器 / 物流邻接 / 状态灯）。⑧**中文输入法闪屏修复（v1.3.4 补丁 · 版本号不变）**——无边框全屏下"切回中文输入法"后持续闪屏/黑屏 + 鼠标漂移；根因是窗口未禁用输入法、候选窗/语言栏与"铺满 + 置顶"的全屏窗口互抢 Z 序与前台焦点；修法见 §0.1-H。编译通过（`[100%] Built target factory-td`）、自检 **25/25**、启动冒烟存活；文档已同步（`update.md` 写入 v1.3.4、`README.md` 与本文 §0/§4.1/§7 已更新）。**2026-09-15 收尾：人工实机视觉走查已由用户完成并通过（原话"我已经实机走过了，实际操作复现一次没问题"，含中文输入法全屏闪屏复测），`Alpha v1.3.4` 已发 GitHub Release（Latest），且 `package.bat` 已能剔除版权字体（见 §0.1-I / §0.1-J）。**

### 0.1 干了啥（v1.3.4，本轮）

**A. `SaveSystem` 重写为 10 槽位手动存档（`src/SaveSystem.h/.cpp`）**
- 命名常量：`SAVE_SLOT_COUNT = 10`；槽位文件 `saves/slot_01.json … slot_10.json`（`slotPath(i)` 两位补零）。
- 数据结构：`SaveSlotInfo{ path, used, corrupt, bytes, savedAt, summary }`；`summary` 形如 `"金币 %d · 第 %d 波 · 建筑 %zu"`。
- 查询接口：`querySlot(i)`（文件不存在→空槽；JSON 解析失败→`corrupt=true`）、`anySlotUsed()`、`newestSlot()`（按 `saved_at` 取最近槽）。
- 读写删：`saveGameToSlot(Game&,i)` / `loadGameFromSlot(Game&,i)` / `deleteSlot(i)`；底层文件级 `saveGameToFile(Game&,path)` / `loadGameFromFile(Game&,path)`（自检与迁移复用）。
- 写入时 `j["v"]=2; j["saved_at"]=nowStamp();`（本地时间 `%Y-%m-%d %H:%M`）；**旧 v1 存档仍可正常读取**（缺 `saved_at` 只影响显示）。
- `migrateLegacySave()`：把旧单档 `saves/factory_td.json` 迁移为 `saves/slot_01.json`（重命名失败则复制+删除）；**槽位 1 已占用则原样不动**，幂等安全。

**B. `Game` 存档接口（`Game.h/.cpp`）**
- `void saveGame()/loadGame()` → **`bool saveToSlot(int slot)` / `bool loadFromSlot(int slot)`**。
- `saveToSlot`：教程模式拒绝并 Toast「新手教程模式不保存进度」；成功 Toast「已保存到槽位 N 号」。
- `loadFromSlot`：成功后 `tutorial = tutorial::State{}`（普通关卡不承载引导状态）；失败 Toast「该槽位没有存档或存档已损坏」。
- **注释里明确"本作没有任何自动保存路径"**，改代码时勿再引入自动写盘。

**C. 游戏内暂停面板（`ui/GameUI.*`）**
- 主菜单 4 项 → **5 项**（继续游戏 / **保存存档** / **载入存档** / 设置 / 返回主界面），`pauseRects_` 改 `array<...,5>`。
- 新增 `enum class SlotPanelMode{Save,Load}`、`openSlotPanel()`（F5/F9 也可直接唤出并暂停世界）、`refreshSlotInfos()`、`handleSlotKey/Click`、`activateFocusedSlot()`、`deleteFocusedSlot()`、`runSlotConfirm()`。
- 几何：`slotTileRects()`（2×5）、`slotButtonRects()`（主操作/删除/返回）、`slotConfirmPanelRect()`、`slotConfirmRects()`；`enum class SlotConfirm{None,Overwrite,Delete,Load,ReturnMenu}`。
- **覆盖/删除前弹二次确认，默认焦点在「取消」**（`slotConfirmFocus_ = 1`）。
- 「返回主界面」改为仅弹确认（`slotConfirm_ = ReturnMenu`），**不再自动保存**。

**D. 启动菜单（`ui/EntrySystem.*`）**
- 主菜单「继续游戏」→「**载入存档**」（快捷键 `L` / `C`）；新增 `State::Slots` 子页与 `chosenSlot()`。
- `Dialog` 改为 `{None, ConfirmQuit, ConfirmLoad, ConfirmDelete, About}`；删除 `ConfirmNewGame`（手动存档下新开局不写盘，无需确认）。
- 新增 `openSlots/closeSlots/refreshSlots/handleSlotsKey/handleSlotsClick/activateFocusedSlot/deleteFocusedSlot/slotTileRects/slotButtonRects/drawSlots` 与 `slotFocus/slotHover/slotInfos/chosenSlotIndex/slotNotice/slotNoticeTimer`。
- 构造函数先 `migrateLegacySave()` 再 `refreshSlots()` 填摘要。
- 加载界面的「校验本地存档数据」一步改为 `refreshSlots()`（只扫描槽位、不写盘）。

**E. 快捷键 / 自检**
- `PlayerSystem`：`F5` → `ui->openSlotPanel(Save)`，`F9` → `ui->openSlotPanel(Load)`（不再直接读写盘）。
- `main.cpp`：`EntrySystem` 销毁前取出 `chosenSlot()`，`continue` 时 `game.loadFromSlot(slot)`；`--selftest-save` 改用 `saves/_selftest_slot.json` 独立临时文件，跑完即删，**不备份/不触碰玩家槽位**。

**F. 版本号**：`EntrySystem.cpp` `kVersion` → `v1.3.4  ALPHA BUILD`；`GameConfig.h` `SCREEN_TITLE` → ` v1.3.4`。

**G. 建筑占地统一为 1×1（全项目尺寸规格审计）**

审计范围：`GameConfig.h`（注册定义 / 成本 / 快捷键）、`components/Building.h`（`w/h` 组件字段）、
`AssetManager.cpp`（`machineKey` 贴图键）、`RenderSystem.cpp`（绘制 / 预览）、`GameUI.cpp`（面编辑器 /
卡片图标）、`Game.cpp`（`canPlace` / `placeBuilding` / `registerToGrid`）、`PipeSystem.cpp`（邻接拉料）、
`PowerSystem.cpp`（电网邻接）、`python版（老版）/entities/Generator.py`（原版参照）。
> 说明：本项目是 2D 网格工厂游戏，**没有** Minecraft 式「模型文件 / 方块状态文件」，
> 对应物是：注册定义 = `cfg::BUILDING_INFOS`；模型 = `AssetManager` 程序化生成的贴图 + `machineKey` 键；
> 占位配置 = `Building::w/h`。

- **结论**：22 种建筑**全部统一为 1×1**。此前旧版大功率燃煤发电机（`BuildingType::Generator`，
  索引 10）被做成 **2×2**（移植自 Python 原版 `entities/Generator.py` 的
  `super().__init__(x, y, TILE_SIZE*2, TILE_SIZE*2)`）——**经确认这是错误设定，已一并改为 1×1**；
  至此项目中不存在任何多格建筑。
- **改造**：
  1. `cfg::BuildingInfo` 增 `uint8_t w = 1, h = 1;`；发电机那行**删除** `, 2, 2` 覆盖，回到默认 1×1。
  2. 新增 `cfg::BuildingSize` + `inline cfg::buildingSize(BuildingType)` —— **尺寸唯一数据源**。
  3. `Game::canPlace` 删掉内联 `t == Generator ? 2 : 1`，改 `cfg::buildingSize(t)` 逐格扫描。
  4. `Game::placeBuilding` 删掉 Generator 分支里的 `b.w = 2; b.h = 2;`，改为在 switch 之前统一
     `b.w = bs.w; b.h = bs.h;`。
  5. `components/Building.h` 新增 `struct FaceTiles` + `inline FaceTiles faceNeighborTiles(const Building& b, int dir)`
     —— 取某面**外侧**的所有相邻格，按 `b.w/b.h` 沿切向铺开（当前全 1×1→1 格）。
  6. `RenderSystem.cpp` 放置预览：`sf::Vector2f fp{s*bs.w, s*bs.h}`（原先恒为 `{s,s}`）。
  7. `GameUI.cpp` 面编辑器三处：`faceEditorRects` 方向按钮偏移改
     `offX = size*(b.w+1)*0.5f`、中心高亮框改 `size*b.w × size*b.h`、
     点击外部关闭范围改 `size*(b.w+2) × size*(b.h+2)`。
  8. `PipeSystem.cpp` 机器拉料 + 发电机拉煤两个循环：外层仍按面 `d` 走，**内层新增 `fi` 遍历
     `faceNeighborTiles(b, d)`**；发电机的"每帧至多补 1 煤"语义用 `bool got` 保住（原来靠 `break` 外层循环）。
  9. `RenderSystem.cpp` 发电机状态灯移到整块占地中心。
- **自检**：`main.cpp --selftest-save` 新增占地规格项 —— ①静态审计"全部建筑均为 1×1"；
  ②自动找一块空地实地放置发电机，核对单格 `isOccupied` 且该格 `canPlace==false`；
  ③读档后仍为 1×1。（建筑计数 13→14 种）→ **25/25 通过**。

**H. 中文输入法全屏闪屏修复（v1.3.4 补丁 · 版本号不变，`Settings.cpp` + `Game.cpp` + `CMakeLists.txt`）**

> 用户反馈：无边框全屏下，输入法**从英文切回中文**之后画面持续闪屏/黑屏，鼠标在世界里的坐标反复跳变
> （放置预览/摄像机像"自己乱滑"）；**窗口化则正常**；**所有帧率档位都一样**；同款现象也出现在 **《我的世界》1.7.10**。

- **为什么是输入法**（逐条现象对上）：
  1. 窗口化不闪 → 窗口不再 topmost，候选窗在最上层正常显示，不存在 Z 序争夺。
  2. **所有帧率档位都闪** → 与垂直同步/交换链饥饿无关（帧率不影响焦点争夺）。
  3. **英→中切回才闪** → 切换键盘布局会销毁/重建 IME 窗口与 TSF 服务；切回中文时输入法重建候选窗/语言栏并抢"编辑焦点"。
  4. 只在 MC 1.7.10 复现 → 1.7.10 同样是"老式 Win32 全屏窗口 + 不处理 IME"，是**同一类窗口路径**的共性问题。
  5. 无事件 4101、无进程冲突 → 排除驱动 TDR 与第三方 overlay，锁定应用层窗口/焦点 × 输入法。
- **根因**：全屏 = 「`sf::Style::None` 铺满 + `SetWindowPos(HWND_TOPMOST)` 置顶」，而游戏窗口**从未禁用输入法** → 置顶全屏窗口与输入法候选窗/语言栏**互抢 Z 序与前台焦点**，整窗反复重绘 → 持续闪屏，鼠标坐标随之跳变。
- **修法**（`src/Settings.cpp`）：新增匿名命名空间的 `suppressImeForWindow(sf::RenderWindow&)`，
  在 `gset::applyToWindow()` 的 `window.create()` **之后**调用（**窗口重建会产生新 HWND，必须重新关**）：
  1. `ImmAssociateContext(hwnd, nullptr)` —— 解绑该窗口输入法上下文（**IMM32** 路径）；
  2. `ImmDisableIME(0)` —— 禁用当前线程输入法；
  3. `SetWindowLongPtrW(hwnd, GWLP_WNDPROC, &imeSuppressProc)` —— 窗口过程最外层**吞掉 `WM_IME_STARTCOMPOSITION/COMPOSITION/ENDCOMPOSITION/NOTIFY/CHAR/REQUEST`**，
     并把 `WM_IME_SETCONTEXT` 的 `ISC_SHOWUICOMPOSITIONWINDOW` 位清掉后**继续下发**（整条丢弃会让输入法状态错乱）——这一条覆盖 **TSF 路径**（微软拼音在 Win10/11 走的就是 TSF，光靠 IMM32 不够）。
  - **为什么敢直接禁**：全项目**没有任何文本输入**（无 `TextEntered`、无输入框），禁用输入法零副作用。
  - `CMakeLists.txt`：`if(WIN32) target_link_libraries(factory-td PRIVATE imm32)`。
- **连带修复**（`src/Game.cpp`）：`processEvents()` 收到 `sf::Event::LostFocus` 时 `keys.fill(false)` ——
  输入法候选窗抢焦点会让 W/A/S/D 的"松开"事件丢失、按键卡在按下态，导致摄像机/放置预览持续漂移。
- **验证**：编译通过（`[100%] Built target factory-td`）、`--selftest-save` **25/25 通过**（窗口创建路径真实执行过）。
- **实机复测（已完成，2026-09-15）**：用户实机复测确认——无边框全屏下中英文输入法来回切换、复现原场景均正常，无闪屏/黑屏、鼠标不漂移、W/A/S/D 不卡键；`HWND_TOPMOST` **无需**降级为 `HWND_TOP`（保留现状）。

**I. 发布 `Alpha v1.3.4`（GitHub Release，2026-09-15）**
- tag `v1.3.4` 已 push；Release 标题 `Alpha v1.3.4`，附件 `factory-td-v1.3.4-win64.zip`，**已设为 Latest**（上一版 v1.3.3 自动退为历史版本）。
- 永久下载地址：<https://github.com/liuyandi386/jyfactorio/releases/download/v1.3.4/factory-td-v1.3.4-win64.zip>
- 出包方式：`factory-td/package.bat`（当前 `build` 已是 `FACTORYTD_PORTABLE=ON` 的通用构建且 `flags.make` 无 `march=native` → **未触发重编译**，直接复用了通过自检的那份 exe，即 2026-09-15 21:00 含输入法修复的版本）。
- **注意**：仓库**不含** zip（`.gitignore` 第 34-39 行忽略 `*.zip`/`*.7z`/`*.rar`，注释明确"分发编译好的游戏请走 GitHub Releases"）；`dist/` 也在忽略之列。**发版 = 推 tag + 建 Release 传附件，与源码提交是两件事，别只 push 源码就以为发完版了。**

**J. 打包剔除不可再分发的系统字体（`package.bat` 新增第 8a 步）**
- **问题**：`assets/fonts/simsun.ttc` 是 Windows 自带「中易宋体」，授权**不允许再分发**。`.gitignore` 只把 `*.ttc` 挡在**仓库**外，而 `package.bat` 的 `xcopy build\assets` 仍会把它拷进 zip —— v1.3.2 / v1.3.3 的公开包都带着它（约 18MB 源文件）。
- **修法**：在资源复制之后新增第 8a 步，**按已知版权文件名逐个删除** `simsun.ttc / simsun.ttf / msyh.ttc / msyh.ttf / simhei.ttf`，再用 `rd` 删空目录（**目录非空时 `rd` 静默失败** → 将来放入开源字体不会被误删）。注释刻意写成**纯英文**，避免触碰该脚本的 `.bat` 中文编码坑（见 §0.2 的维护提示）。
- **为什么安全**：`AssetManager::loadFont()` 的加载链是「包内字体 → `C:\Windows\Fonts\simsun.ttc` → … → SFML 内置字体」；而 `EntrySystem::loadFont()` 的候选表**本来就不含包内 `simsun.ttc`**（启动菜单一直走系统字体）→ 中文 Windows 显示不受影响。
- **效果**：zip 由 **12,884,615 → 3,317,474 字节**，包内字体文件数为 **0**，exe / 运行库 / 全部贴图 / 文档仍完整（96 条目）；Release 附件已用 `gh release upload <tag> <file> --clobber` 覆盖，**下载地址不变**。
- **遗留**：若将来要内置字体，请改放**开源中文字体**（Noto Sans SC / 思源黑体）到 `assets/fonts/` —— 第 8a 步是按文件名白名单删除的，不会误删。

### 0.1a 上一版（v1.3.3，已完成）：品牌重塑 + 新手教程 + 叙事层 + 采矿场双模式 + 适配修复

> **v1.3.3 是一次"品牌 + 教学 + 适配"的大版本，共 6 块：①品牌重塑《异星工厂塔防》→《织星计划 Project Weavestar》（含织女星 AI 人设）；②新增独立的新手教程模式（`systems/TutorialSystem.*`）与叙事播报层（`systems/Narrative.*`）；③设置新增「帧率 / 垂直同步」；④采矿场双模式（原有模式 / 固定矿点模式，右键面板切换）；⑤修复 5 个 bug——机器贴图整体偏左 90°、高刷新率下摄像机与移动速度翻倍、高 DPI 下无边框全屏只占一角、旋转输出面后箭头与输出面错位、固定 60 帧后画面异常；⑥术语去模组化「ME → 通物」。编译通过（`[100%] Built target factory-td`）、`--selftest-save` 21/21、启动冒烟存活；文档已同步。**

原 v1.3.3 交接小节（重命名为 0.1b 的历史沿革见下）：

**A. 机器贴图方向错位（本版最"值钱"的修复）**
- 症状：放置采矿场 / 组装机 / 发电机时，贴图里的箭头比实际输出方向**向左偏 90°**。
- 根因：`AssetManager::machineKey()` 沿用了 Python 版的"逆时针 90°"映射 `(d+3)%4`；而 PNG（`organize_sprites.py` 用 `rotate(-angle)` **顺时针**生成、文件名即朝向）与程序化箭头本来就是"**后缀 == 视觉朝向**"。
- 修法：`machineKey` 改为 `DIR_NAMES_4[d]`（`rotated` 参数保留但不再参与映射），并**同步** PNG 加载路径 + 程序化箭头绘制的 `texDir`（共 3 处）。
- 连带一致性核实：放置锚点用的是鼠标格（无贴图偏移）、面配置 `fc.set(dir, OUTPUT)` 与 `b.dir` 同源，所以修复后"**贴图箭头 == 输出面 == 物流口**"三者第一次真正对齐。**塔的 8 方向 `setRotation`、熔炉/合金炉/储物桶的 `_d` 后缀键不受影响，勿顺手改。**

**B. 采矿场双模式（`Machine` 组件 + `MinerSystem` + 右键面板）**
- `Machine` 新增 `fixedOre / oreFilter / hasBound / boundX,boundY`。
- **原有模式**（默认）：半径内所有矿点随机采集——与旧版**行为逐字一致**。
- **固定矿点模式**：吸附到矿点旁边，只采 `oreFilter` 那一个矿点；采尽/矿点消失自动回退到范围内最近的同矿种矿点。
- **产量规则两种模式完全相同**（同一个 `m.rate`、`while (acc>=1)` 每轮结算 1 个）——这是需求硬约束，改逻辑时别动。
- **矿点储量开关（v1.3.3 追加）**：新增 `cfg::ORE_INFINITE`（config `ore_infinite`，**默认 `true` = 无限开采**：采矿不扣储量、矿点永不消失，恢复旧 Python 版手感；置 `false` = 有限矿点 1000/点、采尽消失）。`MachineSystem::updateMachines` 两种模式都按该开关决定是否 `dep.amount--` / 销毁矿点；采矿场面板「剩余」在无限模式显示 `∞`，说明书文案随之切换。
- `Game::moveBuilding()`：1×1 建筑移动（先注销占格 → 判定地形/占用 → 失败回滚），供"吸附"使用。
- `GameUI` 新增采矿场设置面板（`showMinerPanel/handleMinerPanelClick/drawMinerPanel/layoutMinerPanel`），右键矿机**不再是"直接旋转"**。
- `MinerSystem::rotateOutputFace()`：旋转时**同步 `b.dir` 与 `FaceConfig`**（旧版 `FaceConfig::rotate()` 只转面不改 `dir`，是"箭头与输出错位"的第二个根因）。

**C. 新手教程（新增 `systems/TutorialSystem.*`）**
- 主菜单「新手教程」(`T`) / 「普通关卡」(`N`) **平级**；旧的"新开局自动弹说明书"与 `autoOpenHelp` 设置**已删除**（说明书仍在，`H`/`F1` 手动开）。
- 章节 / 步骤 / 任务判定 / 耗时统计 / 误操作计数；顶部引导横幅 + 步骤进度 + 跳过按钮；`F2` 引导中=跳过、否则=重开。
- 进度存 `saves/tutorial.json`，与主存档**完全隔离**（教程内 F5/F9 明确提示"不保存/不读取"，不会覆盖普通关卡存档）。
- 教程模式**不自动出怪**，敌人由教学步骤手动生成（Z/X/C）。

**D. 叙事层（新增 `systems/Narrative.*`）**
- 右下角通讯条 `GameUI::showComms`（头像 + 说话人 + 台词，**独立通道**，不会被 Toast 挤掉）。
- 事件播报：着陆 / 首建 / 首次遇敌 / 首次击杀 / 一波清空 / 防护告急 / 吐槽公司 + 波间开发日志；同事件多条台词**轮换**，里程碑**整局只播一次**；**教程模式不叠加**本模块。

**E. 帧率 / 垂直同步（设置界面第 4 项）**
- `FrameMode{Vsync, Limit60, Limit120, Limit144, Unlimited}`，落盘 `saves/settings.json`。
- **唯一应用点 `gset::applyFrameMode()`**：SFML 的 `setVerticalSyncEnabled` 与 `setFramerateLimit` 会**互相拆台**，必须按固定顺序"二选一"。

**F. 显示 / 启动适配**
- `enableHighDpiAwareness()`：修复 125% / 150% 缩放屏下无边框全屏"只占屏幕一角"。
- `Camera` 改为**帧率无关**（`CAMERA_SPEED_PER_SEC` / `CAMERA_SMOOTH_RATE` 指数平滑），修复 144Hz 下摄像机与移动速度翻倍。
- `--safe-mode` / `--windowed` 应急参数（强制 1280×720 窗口化，**不写回**设置文件）。

**G. 术语去模组化**：界面「ME 网络 / 接口 / 终端」→「**通物网络 / 接口 / 终端**」；**内部代码名 `Me*` 与 JSON 键 `me_*` 一律不动**（存档与 `config.json` 零迁移）。

**H. 版本号**：`EntrySystem.cpp` `kVersion` → `v1.3.3  ALPHA BUILD`；`GameConfig.h` `SCREEN_TITLE` → ` v1.3.3`。
（`package.bat` 用正则从 `GameConfig.h` 抓 `vX.Y.Z` 当包名，所以**只改 `SCREEN_TITLE` 一处即可**，别在 `GameConfig.h` 其它地方塞版本号字符串。）

### 0.1b 上一版（v1.3.2，已完成）：游戏内暂停面板 + 设置项共用化

- **`ui/GameUI.h/.cpp`** 新增暂停面板（ESC 打开）：
  - 全屏变暗 + 居中纵向按钮列：**继续游戏 / 保存存档 / 设置 / 返回主界面**（`drawPausePanel` / `drawPauseMenu`）。
  - 键盘 ↑↓ 选择、Enter 确认、Esc 继续游戏；鼠标可点、可悬停高亮（`MouseMoved` 才接管焦点，指针停住不夺焦——沿用上一轮教训，见 0.3）。
  - 打开即 `g_->paused = true` 冻结世界，并 `closePanels()` + `hideRecipePopup()` + 清 `faceEditTarget`，防止面板叠层。
  - **保存存档** = `g_->saveGame()`（Toast 反馈，面板不关）；**返回主界面** = 先自动保存 → `g_->returnToMenu = true`。
  - **「设置」子页与启动菜单完全一致**：直接调用 `gset::settingRowLabel/Value/IsAction` + `cycleSettingRow` + `activateSettingRow`。
  - 新增 `consumeDisplayApply()`：暂停面板改了显示模式时，由 `Game` 在**事件循环之外**重建窗口。
  - 新增 `anyPanelOpen()`：供 ESC 判断"先关面板"还是"开暂停面板"。
- **`Settings.h/.cpp`（`gset`）抽出共享设置项定义**：`SETTING_ROW_COUNT`、`settingRowLabel()`、`settingRowValue()`、`settingRowIsAction()`、`cycleSettingRow()`（返回是否需重建窗口）、`activateSettingRow()`（返回 `SettingActivate{None,Changed,DisplayChanged,Back}`）。`EntrySystem::drawSettings()` / `cycleSettingRow()` / `activateSettingRow()` 已改为调用它们（视觉与行为不变）。
- **`Game.h/.cpp`**：新增 `bool returnToMenu`、`void applyDisplayMode()`；`run()` 循环条件改为 `window.isOpen() && !returnToMenu`，并在 `processEvents()` 之后消费 `ui->consumeDisplayApply()`（重建窗口后同步摄像机尺寸/视图/UI 布局并保持暂停）。
- **`main.cpp`** 改为循环：`while(true){ EntrySystem →(Quit? 退出)→ Game →(returnToMenu? 回菜单 : 关窗退出) }`。
- **`systems/PlayerSystem.cpp`**：ESC 语义 = 有面板/面编辑器先关（MC 习惯），否则 `ui->openPausePanel()`。
- **版本号**：`EntrySystem.cpp` 的 `kVersion` → `v1.3.2  ALPHA BUILD`；`GameConfig.h` 的 `SCREEN_TITLE` 加 ` v1.3.2`。

### 0.1c 更早（v1.3.1，已完成）：启动入口系统 + 跨场景设置

`ui/EntrySystem.*`：启动动画 → 标题主菜单 → 设置 → 加载预览（真实进度 + 敌人路径/矿点迷你地图），自带独立窗口、结束即销毁；`Settings.h/.cpp`（落盘 `saves/settings.json`）与 `main.cpp` 先 `gset::load()`、`--selftest-save` 保留在入口系统之前。详见 `update.md` 的 Alpha v1.3.1 章节。

### 0.2 验证到什么程度

- ✅ **编译**：`cmake --build` 得到 `[100%] Built target factory-td`（LTO 的 `ar.exe: plugin needed to handle lto object` 是已知噪音，见 §3.2）。**v1.3.4 同样编译通过。**
- ✅ **存档自检**：`factory-td.exe --selftest-save`（在 `build/` 下运行）→ **25/25 通过**（见 §3.4）。**v1.3.4 起自检改用独立临时文件 `saves/_selftest_slot.json`，不再备份/触碰玩家槽位；并新增占地规格核对（全建筑均 1×1 / 发电机单格登记与阻挡 / 读档后占地完整）。** v1.3.3 新增的 `fixed_ore / ore_filter / has_bound / bound_x / bound_y` 字段**向后兼容**：旧存档无这些键 → `value(..., default)` 回落为"原有模式"，位置/朝向直接沿用存档值、**不做吸附重排**（避免读档后建筑乱跑）。
- ✅ **启动冒烟**：在 `build/` 下后台运行 exe 未立即退出（正常）。
- ✅ **窗口输入法禁用（v1.3.4 补丁）**：编译通过；`--selftest-save` **25/25** —— 该自检会**真实创建窗口**，因此 `suppressImeForWindow()` 的执行路径已被实跑过且未崩溃。**"切回中文输入法后是否还闪屏"已由用户实机复测确认修复（2026-09-15）**，`HWND_TOPMOST` 无需降级为 `HWND_TOP`。
- ✅ **人工视觉走查已通过（2026-09-15，用户实机）**：用户原话「**我已经实机走过了，实际操作复现一次没问题**」——**输入法闪屏修复已实机复现、验证闭环**。其余目视项（新手教程全流程 / 织女星通讯条观感 / 采矿场设置面板交互 / 高刷新率·高 DPI 手感 / 贴图朝向）按"已实机走过"一并视为通过；后续若发现残余问题再单独登记。
- ✅ **发布物**：`Alpha v1.3.4` Release 已上线，附件 `factory-td-v1.3.4-win64.zip`（3,317,474 字节，**已剔除版权字体**），下载地址经 HTTP 200 校验且长度与本地包一致（见 §0.1-I / §0.1-J）。

### 0.3 关键坑（已修，勿再犯）

1. **鼠标悬停每帧夺取键盘焦点**：`EntrySystem` 的悬停命中写成 `if (r.contains(mp)) { settingHover = i; settingRow = i; }` → 指针停住时键盘焦点每帧被弹回该行，↑↓/←→ 形同失效。修法：**只有鼠标真正移动时**才让悬停行接管焦点（记录上一帧鼠标位置做比较），指针停住仅更新高亮；进入设置界面时先同步 `lastMousePos`。**暂停面板的悬停沿用了同一套做法。**
2. **独占全屏黑屏闪烁 + 鼠标漂移**：全屏改用 `sf::Style::None` + 桌面尺寸 + 贴 (0,0) 的"无边框全屏"，**不调用 `SetDisplayMode`**；并用 `SetWindowPos(HWND_TOPMOST)` 防任务栏遮挡。
3. **不能在 `pollEvent` 事件循环内重建 SFML 窗口**：显示模式变更一律延后到事件循环之外（`EntrySystem::pendingDisplayApply` / `Game::run` 里的 `ui->consumeDisplayApply()` + `applyDisplayMode()`）。
4. **v1.3.3 新坑：贴图方向映射不能"照抄 Python"**。Python 版老代码里用的是"逆时针 90°"键表，但**它对应的 PNG 命名规则与该表是配套的**；C++ 版边把 PNG 换成"顺时针生成、文件名即朝向"，却保留了那张逆时针表 → 全部机器贴图偏左 90°。**改动贴图键名/朝向映射前，先 `dir assets/sprites/machines` 确认文件名与箭头方向，再决定映射公式。**
5. **v1.3.3 新坑：SFML 的 `setVerticalSyncEnabled` 与 `setFramerateLimit` 互相拆台**。同时设置会导致帧率/画面异常。任何帧率相关改动都要走 `gset::applyFrameMode()` 这一个入口（内部按固定顺序二选一），**不要在别处再直接调这两个 setter。**
6. **v1.3.3 新坑：所有"速度"必须乘 `dt`，所有"平滑"必须用帧率无关形式**。摄像机原先是每帧固定步长，144Hz 下比 60Hz 快 2.4 倍。新写的移动/动画同理。
7. **v1.3.4 补丁新坑：「无边框全屏 + `HWND_TOPMOST`」必须同时禁用窗口输入法**。否则中文输入法（微软拼音走 TSF）在被切回时会在游戏窗口上重建候选窗/语言栏并抢"编辑焦点"，与置顶全屏窗口**互抢 Z 序与前台焦点** → 持续闪屏/黑屏 + 鼠标坐标跳变（表现为"只有切回中文才闪、窗口化不闪、任何帧率都闪"；《我的世界》1.7.10 同款）。**只要本作保持"置顶全屏"窗口策略，`gset::applyToWindow()` 里那次 `suppressImeForWindow()` 就不能删；且它是跟着 `window.create()` 走的（每次重建窗口都要重新关一次）。** 另外：全项目保持"无任何文本输入"，禁输入法才安全；**若将来真要加输入框，必须先把这里的禁用逻辑按控件粒度放开。**

### 0.4 接下来要干啥（建议顺序）

1. **人工实机走查（✅ 已完成 2026-09-15，用户实机确认）** —— 下列清单**保留作为回归 / 改版后的复测参考**，已不是待办遗留：
   - **存档槽位（v1.3.4 新增，重点）**：主菜单「载入存档」与暂停面板「保存/载入存档」打开槽位页，确认 **10 张卡片排版正常**；分别在空槽保存、对已占用槽位覆盖（应弹确认、默认「取消」）、删除（应弹确认）、读取恢复；确认**存档时间/摘要**显示正确；确认旧 `saves/factory_td.json` **已迁移到槽位 1** 且内容完好；确认**新开一局后直接关窗，旧存档槽位不被改动**。
   - **建筑占地统一 1×1（v1.3.4 新增）**：按 `8` 选发电机，确认**放置预览为 1 格绿/红框**；放置后右键面编辑器，**中心高亮框盖满该格、四个方向按钮在该格之外**；在发电机四面接上管道喂煤，确认**四面都能进煤**；拆掉重建确认该格释放、退费一次。
   - **贴图朝向**：放置采矿场，用 `R`/放置方向键切 4 个方向，确认**贴图箭头方向 == 输出面 == 实际物流口**；组装机、发电机同样确认。
   - **采矿场双模式**：右键矿机 → 面板出现；切「固定矿点模式」确认**吸附到矿点旁**且只采所选矿种；切回「原有模式」确认行为与旧版一致；存档再读档确认位置/朝向/模式/绑定原样恢复；确认两种模式产率一致。
   - **新手教程**：主菜单「新手教程」→ 引导横幅/步骤/跳过按钮正常，`F2` 跳过与重开；教程内 `F5`/`F9` 有"不保存/不读取"提示；退出教程后普通关卡存档未被污染（`saves/factory_td.json` 与 `saves/tutorial.json` 互不影响）。
   - **叙事**：着陆/首建/首次遇敌等播报出现，通讯条不被 Toast 挤掉；教程模式下**不叠加**播报。
   - **帧率设置**：设置界面第 4 项逐个切换（垂直同步/60/120/144/不限制），确认**立即生效且画面不抖**；高刷屏重点看"不限制"与"垂直同步"的手感差异。
   - **高 DPI**：本机 150% 缩放，确认无边框全屏铺满且不"只占一角"；黑屏时用 `--safe-mode` 救场。
   - **中文输入法全屏闪屏（v1.3.4 补丁新增，重点）**：无边框全屏下，把输入法在**英文 ↔ 中文之间来回切换多次**（尤其是"切回中文"这一下），确认**画面不再闪烁/黑屏、鼠标不再漂移、W/A/S/D 不再卡键**；窗口化模式再复测一遍对照。**这一步只能人眼确认，编译与自检覆盖不到。**
   - 顺带复测 v1.3.2/v1.3.1 遗留：暂停面板与返回主界面、主菜单设置界面焦点不被夺走。
2. **可选（先问用户）**：新手教程的文案/步骤数与章节划分是否合适？是否需要"教程内可跳过到任意章节"？
3. ~~文档同步~~ → **已完成**（`update.md` v1.3.3、`README.md`、本文 §0）。

---

## 1. 项目现状总览

- **项目**：Factorio 风格 2D 工厂塔防游戏，C++20 + SFML 2.6 + EnTT 3.13 + nlohmann/json 重构版。
- **位置**：`D:\JYGAME\jyfactorio\factory-td\`（C++ 主工程）。同目录还有：
  - `python版（老版）\` —— Python/pygame 原始版本（只作对照，不再开发）
  - `update.md` —— 更新日志（C++ 部分最新 **Alpha v1.3.3**，含各版本功能与修复说明）
  - `README.md` —— 总 README（C++ 在前、Python 在后）
  - `add.txt` —— **用户需求原文（最高优先级需求来源）**
  - `TODO.md` —— 早期遗留 TODO
  - `factory-td\PORTING.md` —— Python→C++ 移植对照表
- **当前状态**：add.txt 的 ①~⑬ 项需求**全部实现**，编译通过、运行正常；存档系统经过全面修复、建筑占地规格已统一收敛，并有自检验证（25/25 通过）。
- **当前版本**：**Alpha v1.3.4**（2026-09-15）——**存档机制重做为「手动保存 · 10 槽位」**（参考 WorldBox；移除所有自动保存路径、旧单档自动迁移到槽位 1）；上一版 v1.3.3 为品牌重塑《织星计划》+ 新手教程模式 + 叙事播报 + 采矿场双模式 + 贴图/高刷/高DPI 修复。
- **v1.3.4 状态**：`SaveSystem` 重写为 10 槽位手动存档（`saves/slot_01..10.json`，`querySlot/anySlotUsed/newestSlot/saveGameToSlot/loadGameFromSlot/deleteSlot/migrateLegacySave`）；`Game::saveGame/loadGame` → `saveToSlot/loadFromSlot`；启动菜单「载入存档」+ 暂停面板 5 项共用 2×5 槽位页（覆盖/删除二次确认、默认焦点「取消」）；`F5`/`F9` 改为打开槽位页；`--selftest-save` 改用独立临时文件；**建筑占地尺寸统一收敛到 `cfg::buildingSize()`**（全项目所有建筑统一 1×1，已废止旧版发电机的 2×2；修复预览/面编辑器/物流邻接 4 处占地逻辑）。**代码完成、编译通过、存档自检 25/25、启动冒烟通过；另含一个 v1.3.4 补丁（**版本号不变**）——修复「中文输入法切回中文后无边框全屏闪屏/黑屏 + 鼠标漂移」（窗口主动禁用输入法，见 §0.1-H）；文档已同步；人工实机视觉走查已通过（2026-09-15 用户实机确认），`Alpha v1.3.4` 已发 GitHub Release（Latest，附件已剔除版权字体）（详见 §0）。**
- ⚠ **文档同步坑（已修复，勿重犯）**：曾出现根目录 `update.md` / `ai.md` 停在 v1.2.1，而开发记录只写进了 `build/update.md` 副本，落后两个版本。**改代码后同步更新根目录 `README.md` / `update.md` / `ai.md`；`build/` 是构建产物目录，不是文档源。**
- **开发方式**：每改一批代码必须 `cmake --build` 编译验证 + 启动冒烟测试；用户会实际游玩并截图报 bug，报 bug 时先"解析为什么"再修。

---

## 2. 目录结构与文件职责（factory-td/）

```
factory-td/
├── CMakeLists.txt        # 三级依赖探测：系统装 → 本地deps → FetchContent联网
├── build.bat             # 一键构建（自动找小熊猫g++/lerobot cmake/Clash代理）
├── PORTING.md            # Python→C++ 移植对照表
├── assets/
│   ├── config.json       # ★ 全部可调数值（改数值无需重编译，见 §8）
│   ├── fonts/simsun.ttc  # ★ 打包中文字体（18MB，优先于系统字体加载）
│   └── sprites/          # PNG 贴图（塔8向/机器4向/桶/敌人/矿石3种）
├── deps/local2/          # 离线依赖：SFML 2.6.1 mingw预编译 + EnTT 3.13.2 头文件
│                         #   + nlohmann/json.hpp（单头）
├── build/                # 构建输出（factory-td.exe + 复制的 assets + saves/）
└── src/
    ├── main.cpp          # 入口；支持 --selftest-save 存档往返自检（§3.4）
    ├── GameConfig.h      # ★ 全部数值内置默认值（中文注释，[JSON可调] 标注哪些可被config.json覆盖）
    ├── Settings.h/.cpp   # ★ 跨场景用户设置（显示模式/悬停提示/摄像机速度/帧率·垂直同步）→ saves/settings.json
    │                     #   + 设置项共用定义 settingRow*/cycleSettingRow/activateSettingRow（见 §0）
    │                     #   + applyFrameMode()：帧率/垂直同步的【唯一】应用点（VSync 与 limit 二选一）
    ├── Game.h/.cpp       # 主控：窗口/网格/地形/矿点/建筑放置拆除/移动/波次/存档接口/update()编排/悬浮提示
    │                     #   + returnToMenu（暂停面板返回主界面）/ applyDisplayMode（事件循环外重建窗口）
    │                     #   + moveBuilding()（1×1 建筑移动，供采矿场"固定矿点模式"吸附）
    │                     #   + gameMode（Normal / Tutorial）、enableHighDpiAwareness()
    ├── Camera.h/.cpp     # 摄像机（WASD/滚轮缩放0.5~2.0/帧率无关指数平滑CAMERA_SPEED_PER_SEC/坐标变换）
    ├── AssetManager.h/.cpp # 贴图+字体加载缓存；程序化生成（地形/熔炉/合金炉/电容/发电机/矿机L2L3虚空/5新矿石）
    ├── ConfigLoader.h/.cpp # config.json 加载器（"存在才覆盖"，缺字段用内置默认值）
    ├── SaveSystem.h/.cpp # JSON 存档：10 槽位手动保存（saves/slot_01..10.json；旧单档自动迁移到槽位 1）
    ├── ui/GameUI.h/.cpp  # 暗色工业风HUD：资源栏/建筑按钮/方向悬浮窗/面编辑器/配方菜单/
    │                     #   说明书(H/F1、6标签页)/商店(B)/随身工作台(V)/通物终端面板/
    │                     #   小地图/世界地图(M)/Toast/悬停提示/暂停面板(ESC：继续·存档·设置·返回主界面)/
    │                     #   采矿场设置面板(右键矿机：模式切换/矿种筛选/旋转输出面)/
    │                     #   教程引导横幅(F2跳过)/织女星通讯条(showComms)
    ├── ui/EntrySystem.h/.cpp # ★ 启动入口：启动动画/标题主菜单/设置/加载预览（自带独立窗口，见 §0）
    ├── systems/          # ECS 系统（见 §4）
    │   ├── PlayerSystem.h/.cpp   # 输入：键盘热键/左键放置/右键交互/摄像机/预览
    │   ├── MachineSystem.h/.cpp  # 采矿机/熔炉/合金炉/组装机生产计时 + 输出推送(机器→桶/管道/分流器/机器/ME)
    │   ├── PowerSystem.h/.cpp    # 电网：发电机燃烧、BFS路由、电容充放、供电判定（EU/秒）
    │   ├── PipeSystem.h/.cpp     # 物品管道：即时路由BFS、分流器、机器拉取原料、连接掩码
    │   ├── MeSystem.h/.cpp       # AE2式通物网络：网络重建/入网吸入/出网导出（后期物流；界面称"通物"）
    │   ├── TurretSystem.h/.cpp   # 炮塔索敌射击+子弹
    │   ├── EnemySystem.h/.cpp    # 敌人移动/波次/击杀结算/生成（Z/X/C分键）
    │   ├── ItemSystem.h/.cpp     # 物品键名表/中文名/颜色（JSON存档与配置共用）
    │   ├── TutorialSystem.h/.cpp # ★ 新手教程（v1.3.3）：章节/步骤/任务判定/进度存 saves/tutorial.json
    │   ├── Narrative.h/.cpp      # ★ 叙事播报（v1.3.3）：织女星通讯条 + 事件播报 + 波间开发日志
    │   └── RenderSystem.h/.cpp   # 世界渲染（分13层：地形/矿点/建筑精灵/管道/电线/塔/…）
    ├── components/       # 组件头文件
    │   ├── Building.h    # 类型/网格坐标/占地/朝向（所有建筑都有）
    │   ├── Machine.h     # 机器：kind/等级/速率/累加器/任务状态/配方id
    │   ├── Inventory.h   # 在 Item.h 里！通用库存（槽×堆叠）
    │   ├── Item.h        # ItemType 枚举 + Inventory + ITEM_INFOS 引用
    │   ├── Pipe.h        # 管道：connMask + buffer(deque) + transferTimer
    │   ├── Me.h          # 通物接口/通物存储单元/通物终端：connMask + networkId（内部名仍为 Me*）
    │   ├── Storage.h     # Bucket / SplitterQueue / OreDeposit(矿点储量, 默认无限开采)
    │   ├── Power.h       # PowerGeneratorNode / PowerCapacitor / PowerConsumer / PowerPole
    │   ├── Turret.h      # 炮塔：射程伤害射速/弹药/电力/炮管朝向
    │   ├── Enemy.h       # 敌人：类型/血量/速度/路径索引/目标
    │   ├── Bullet.h      # 子弹
    │   ├── FaceConfig.h  # 格雷科技式四面配置(NONE/INPUT/TRANSFER/OUTPUT)
    │   └── Position.h    # GridPos 等
    └── utils/
        ├── SpatialGrid.h # 炮塔索敌空间哈希
        ├── Pathfinder.h  # 路径铺格(buildPathTiles)
        └── Profiler.h    # Tracy 打点封装（FT_PROFILE；无Tracy时为空）
```

---

## 3. 构建 / 运行 / 自检

### 3.1 工具链（本机已验证）
- **编译器**：小熊猫C++ 自带 MinGW GCC 11.5 → `C:\Program Files\RedPanda-Cpp\mingw64\bin`（加到 PATH）
- **cmake**：`D:\miniconda\envs\lerobot\Scripts\cmake.exe`（4.3.4）
- **make**：mingw32-make（随 mingw64 的 PATH）

### 3.2 构建命令（在 factory-td/ 目录执行）
```powershell
$env:Path = "C:\Program Files\RedPanda-Cpp\mingw64\bin;" + $env:Path
& "D:\miniconda\envs\lerobot\Scripts\cmake.exe" --build build -j 8
```
- 成功标志：输出 `[100%] Built target factory-td`。
- ⚠ **LTO 警告噪音**：stderr 会出现 `ar.exe: plugin needed to handle lto object` + `lto-wrapper` 警告，PowerShell 因此报 `[exit code: 1]`。**只要看到 "Built target" 就是成功**，别被 exit code 骗。
- 构建后会自动把 `assets/` 复制进 `build/assets/`（post-build 步骤）。
- 全新配置：`cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release`（或直接跑 build.bat）。

### 3.3 运行
- 以 `build\` 为工作目录运行 `build\factory-td.exe`（**必须**，游戏用相对路径读 `assets/config.json` 和写 `saves/`）。
- 冒烟测试惯例：`Start-Process` 后台起 10~15 秒未退出=正常，然后 `Stop-Process -Force` 杀掉。
- ⚠ 偶发"10秒内正常退出 code=0"：多为启动探测误报，重跑一次确认；游戏本身无自动退出逻辑。

### 3.4 存档自检（改存档代码后必跑）
```
build\factory-td.exe --selftest-save
```
- 自动布置 14 种建筑（含旧版大功率发电机）+各类状态 → saveGame → 清空世界 → loadGame → 25 项核对（占地规格/库存/任务/管道缓冲/面配置/塔/分流器/发电机/ME网络/矿点/金币/生命/摄像机/敌人）。
- **v1.3.4 起**：往返测试使用独立临时文件 `saves/_selftest_slot.json`，跑完即删，**不备份、不触碰玩家的任何槽位存档**（旧版会备份/恢复 `factory_td.json`，已废弃）。
- 退出码 0=全部通过。

### 3.5 依赖
- 离线依赖已齐：`deps/local2/` 有 SFML 预编译包、`deps/local2/entt/src`（EnTT 3.13.2 头文件）、`deps/local2/nlohmann/json.hpp`。**离线可构建**。
- ⚠ 沙箱/网络坑（本开发环境）：hosts 把 github 解析到 127.0.0.1；需要联网时代理 `127.0.0.1:7897`（Clash Verge）；Python（OpenSSL）能走代理，schannel TLS 被沙箱拦；沙箱禁写名为 `CMakeLists.txt` 的文件、点开头文件、`entt-3.13.2` 目录——曾用 danger-full-access 提权绕过（用户批准过）。
- DPI：用户屏幕 2560×1440 @150%；若做 GUI 自动化，进程 DPI 不感知，坐标要 ÷1.5。

---

## 4. 架构（ECS）

### 4.1 核心数据
- `Game`（Game.h）：一切公开。`entt::registry reg`、`Grid grid`（200×200，`GridCell{building}`）、`terrain[]`（0草地/1路径）、`enemyWaypoints`、`camera`、`assets`、`ui`、`power`（PowerState）、`gold/lives/playerInv`、`hasSelection/selected`、`hasFreePlace/freePlaceType`（商店兑换机器的免费放置）、`faceEditTarget`、波次状态、`keys[4]`。
- **网格占用**：`grid.at(x,y).building` = 该格建筑实体（多格建筑在其所有格子登记；当前建筑全部 1×1）。**矿点不是建筑**：矿点是 `GridPos + OreDeposit` 实体，不占网格、不阻塞放置。
- **占地尺寸唯一数据源（v1.3.4）**：`cfg::BUILDING_INFOS[i].w/.h` → 用 `cfg::buildingSize(type)` 查询。**全项目 22 种建筑统一 1×1**（旧版发电机曾误设为 2×2，已修正）。放置判定 / `registerToGrid` / 物流邻接 / 放置预览 / 面编辑器全部走这个入口，**禁止硬编码**。若将来引入多格建筑，遍历"每一面的外侧格"用 `faceNeighborTiles(b, dir)`（`components/Building.h`）。
- EnTT 3.13 注意：`view` **没有** `size()/empty()`（用 `std::distance` 或迭代器比较）；`view<T>().each()` 单组件解包是 2 元组（entity+comp），结构化绑定数量必须对上。

### 4.2 Game::update 顺序（Game.cpp，改动时保持该顺序）
```
1 PlayerSystem::updateCamera           // WASD 摄像机
2 MINER_FREE_POWER 时给矿机通电
3 MachineSystem::updateMachines        // 采矿/冶炼/合金/组装 计时生产
4 PowerSystem::updateGenerators + update  // 发电机燃烧 + 电网BFS路由 + 供电判定
5 MeSystem::update                     // ME网络（脏则重建 + 吸入/导出）
  PipeSystem::updateSplitters          // 分流器智能轮询均分
  PipeSystem::updatePipes              // 管道BFS即时路由
  PipeSystem::updateMachinePulls       // 机器从邻管道/分流器/ME接口拉原料；发电机拉煤
6 MachineSystem::pushOutputs           // 机器/桶 输出推送（桶/管道/分流器/相邻机器/ME设备）
7 EnemySystem::update                  // 敌人沿路径移动
8 TurretSystem::updateTowers + updateBullets
9 EnemySystem::processKills            // 击杀→金币
10 EnemySystem::updateWaves
11 updateHoverTooltip + PlayerSystem::updatePreview + ui->update
```

### 4.3 系统/组件一览
| 系统 | 组件 |
|---|---|
| Player | （操作 Game 状态） |
| Machine | Machine + Inventory + FaceConfig（+PowerConsumer 合金炉） |
| Power | PowerGeneratorNode / PowerCapacitor / PowerConsumer / PowerPole / FaceConfig / Turret(电力塔) |
| Pipe | Pipe（缓冲/掩码/计时器）、SplitterQueue、Me* |
| Me | MeInterface/MeDrive/MeTerminal（网络存储是 MeSystem 内 static vector<MeNetwork>） |
| Turret | Turret / Bullet / Inventory(弹药塔) |
| Enemy | Enemy |
| Render | 全部（按层绘制） |

---

## 5. 玩法机制详解（当前实现）

### 5.1 物品（20 种，ItemType 枚举顺序=存档键名顺序）
矿石8：iron_ore/copper_ore/coal/gold_ore/diamond_ore/nickel_ore/silver_ore/lead_ore
锭6：iron_ingot/copper_ingot/gold_ingot/nickel_ingot/silver_ingot/lead_ingot
其他：circuit_board/ammo
合金4：steel_ingot/electrum_ingot/invar_ingot/constantan_ingot
（键名表在 `systems/ItemSystem.cpp` 的 ITEM_KEYS，与枚举严格同序——加物品时两处都要改 + GameConfig.h ITEM_INFOS）

### 5.2 采矿（add.txt ③④）
- 8 种矿石独立矿点随机散布全图（种子42，避开路径、互不重叠）；**默认无限开采**（`cfg::ORE_INFINITE = true`，采矿不扣储量、矿点永不消失，等同旧 Python 版"可无限开采"）；置 `false` 恢复有限矿点（每点 `ORE_DEPOSIT_AMOUNT`=1000，采尽消失）。
- **采矿场 1/2/3 级**（BuildingType Miner/MinerL2/MinerL3）：Chebyshev 范围内（5×5/9×9/13×13）采集**所有类型**矿石，每秒 4/16/256 个（`m.acc += rate*dt`，非无限模式下按矿点储量递减）。
- **虚空采矿场**（MinerVoid）：无矿点，8 矿石轮转产出共 4096/秒。
- 采矿机**不再要求脚下有矿点**（预览已删该检查）；测试期免供电（MINER_FREE_POWER=true）。
- **右键采矿机 = 打开设置面板**（v1.3.3，`GameUI::drawMinerPanel`），面板内含：**模式切换 / 矿种筛选（8 种）/ 旋转输出面 / 关闭**。**两种采集模式（v1.3.3）**：
  - **原有模式**（`fixedOre=false`，默认）：半径内所有矿点**等概率随机**取一个——与旧版行为逐字一致。
  - **固定矿点模式**（`fixedOre=true`）：`MinerSystem::setFixedMode` → `bindAndSnap()` **吸附到矿点旁**（8 邻格、优先正邻、就近，靠 `Game::moveBuilding`），只采 `oreFilter` 那一个矿点；采尽/矿点消失由 `MinerSystem::boundDeposit()` 回退到范围内最近的同矿种矿点。
  - **产量规则两种模式完全相同**（同一个 `m.rate`，`while (acc>=1)` 每轮 1 个）——改逻辑时不要动这条。
  - `MinerSystem::rotateOutputFace()` 旋转时**同步 `b.dir` 与 `FaceConfig`**（旧版只转面不改 dir → 箭头与实际输出错位）。
- ⚠ 虚空采矿场 4096/s 会远超管道/机器吞吐——设计上应接储物桶直连或 通物网络，否则背压停摆（用户已知，属设计）。
- ⚠ 固定矿点模式的**吸附会真实移动建筑**（换矿种/换模式时位置可能变化）——这是需求要求的行为（"固定矿点模式要定义采矿场的生成位置"），不是 bug。

### 5.3 冶炼/合金/组装（add.txt ⑥⑨⑩）
- **数据驱动配方表**（GameConfig.h 中 `std::vector<Recipe>`，全部可由 config.json 覆盖/增删）：
  - `FURNACE_RECIPES`：6 矿石→锭（2~3 秒）
  - `ALLOY_RECIPES`（合金炉，**v1.2.4 起不再需要电力**，需右键选定配方）：钢=2铁锭+2煤；琥珀金=1金锭+1银锭→2；因瓦=2铁锭+1镍锭→3；康铜=1铜锭+1镍锭→2（参考 GT/Mek/Thermal）
  - `ASSEMBLER_RECIPES`：弹药=2铁锭+1铜锭；电路板=1铁锭+1铜锭（右键切换配方）
  - `CRAFTING_RECIPES`（随身工作台，V 键，点击即合成）
- 熔炉：**配方轮询**（均衡烧各矿种，不再只烧第一种）；合金炉/组装机：**右键选定配方**（`recipeId`），之后只按该配方生产。产出流程：`Machine.hasJob/recipeId/jobTime/jobTotal`。
- 组装机/合金炉每种原料有**缓存上限 128**（`ASSEMBLER_ITEM_CAP` / `ALLOY_FURNACE_ITEM_CAP`，管道配送/机器拉料/ME出网三条入库路径统一限流），防止单种原料囤满挤死另一种（如煤囤满堵死铁锭）。
- 熔炉出炉时若库存已满，会移除过剩原料腾位（避免锭被静默丢弃）。
- 机器放置方向=输出面（其余三面 INPUT，GT 式面配置可右键编辑）。

### 5.4 电网（add.txt ⑤；EU/秒）
- 燃煤发电机 32EU/s（煤燃5s）；旧版发电机 1煤→3000EU/3s；电容库 50000EU 容量、64EU/s 充放；电线杆 150px 恒导通；电力塔 8EU/s；采矿机 MINER_POWER_NEED=10（测试期免供）。**合金炉自 v1.2.4 起不耗电**（已从 PowerConsumer 移除，相关断电检查/红点渲染已删）。
- 电力线缆四面向面配置（NONE/INPUT/TRANSFER/OUTPUT），BFS 定向路由。
- **无损耗、无过载——用户明确"就是这样设计的，不用改"**。

### 5.5 物流（两段式：前期管道 / 后期 ME 网络）★用户核心诉求
**前期——物品管道**（EnderIO/Pipez 式，add.txt ⑦）：
- 1×1 方块，**自动链接四邻**的管道/分流器/容器/机器（**没有面配置九宫格**），放置拆除时 `PipeSystem::updateNeighbors` 重算 connMask。
- **无动画、瞬时传送（AE2 式即时路由，用户最终确认保持此方案）**：每 0.25s 每管道对其缓冲做 BFS（版本戳 visited，零分配），找到**最近的可接收端点**直接送达；找不到就停在源管道（背压，上限16）。
- **路由接收端（acceptsItem/deliverItem）**：储物桶（容量内）、弹药塔（只收弹药）、机器（只收其配方所需原料；合金炉断电不收）、发电机（只收煤）、**任意 ME 设备**（网络有容量即收）。
- 机器还会**主动从四邻管道/分流器/ME接口拉原料**（`PipeSystem::wantedInputs` 按配方表算需求，去重），无关物品不污染机器库存。
- 吞吐：机器→管道 1件/帧；路由每 0.25s 每管道 4 件（PIPES_PULL_PER_TICK）；分流器 0.15s 轮询。
- **悬停显示**：管道网络全局缓存（BFS 四邻统计"网络缓存: N 件（共 M 格）+ 本格 x/16"）——用户选择的显示方案。
- **分流器**：自动链接四邻；队列物品"轮询+跳过满出口"均分给管道/桶/塔；BFS 路由可穿过它；机器/桶可直接把物品塞进它。

**后期——ME 网络**（AE2 式"所有物品进网络"，用户明确要两段式）：
- 三种方块：**ME接口**（桥接：从邻管道/分流器吸入入网；向邻机器/发电机/塔/桶导出出网；**右键锁定输出过滤白名单**，存档 `filter` 字段）、**ME存储单元**（每块 +20000 容量）、**ME终端**（右键看全网物品清单，GameUI 面板）。
- **导出均分**：同一网络内多接口导出时按 round-robin 逐件轮询（接口级起点轮换 + 物品级按 ItemType 枚举序轮询），避免单一接口/单一物品饿死；导出只走机器 INPUT 面，OUTPUT 面仅出产物。
- 设备四邻自动链接成网；连通块=一个网络，`MeNetwork{items, totalItems, capacity}` 全局共享（MeSystem 内 static vector）。
- **任意 ME 设备都是网络入口**：机器输出/管道路由可直接顶在 MeInterface/MeDrive/MeTerminal 上入网（不只是接口！用户踩过这个坑）。
- 网络编号按**行主序 BFS** 确定（确定性→存档可复现）；拓扑变化时物品按"锚点（最左上设备格）"迁移，合并/分裂不丢物品。
- 机器拉取与出网导出共用 `PipeSystem::wantedInputs`。
- 造价门槛（电路板+钢锭）自然形成"前期管道→后期ME"的科技曲线。

### 5.6 商店 / 背包（B 键，add.txt ②；v1.2.4 重构）
- 全屏商店页（B 键或左上角「商店」按钮）；价目 = `SHOP_OFFERS`（config.json `shop_offers`）。
- 侧边栏改为**背包**：上方机器格（点击选中进入放置，悬停显示全名+数量）、下方物品格；数据 `Game::backpackMachines`（存档 `machines` 字段，旧档缺失回落 100000）。
- 兑换机器 → **成品进背包 +1**，放置时扣背包机器数、拆除返还（`hasFreePlace/freePlaceType` 为旧逻辑保留）。
- **当前价目是占位起步表**（铁锭×50=100金等）——用户明确"配方表以后再写"，那个"200币+10电路板=1组装机"示例**不得加入**。

### 5.7 波次与敌人
- **波次自动生成关闭**（wave_auto_spawn=false，测试模式；用户说"目前不打开显示刷怪功能"）。倒计时→每1秒出怪→每波5+2×波数逻辑保留在 EnemySystem::updateWaves，开关打开即恢复。
- **分键召唤**：Z=普通 X=快速 C=坦克（U=普通兼容）。敌人沿 PATH_POINTS（10点，200×200）移动，到终点扣命（初始10），击杀只加金币。
- 三种敌人：basic 100血/1.5速/20金，fast 60/3.0/30，tank 300/0.8/50。

### 5.8 炮塔
- 基础/速射/狙击/电力 4 种（1/2 键）；基础塔需要弹药（Inventory+Turret.ammo），电力塔 8EU/s 入网、内部电力100、每发耗10。

### 5.9 GUI / 地图
- 暗色工业风全 UI（GameUI.cpp 顶部 UI_* 常量统一配色）；地形程序化（草地草斑/路径碎石车辙，覆盖 PNG）。
- **小地图**（左下 160px，Xaero 式：地形/矿点/建筑/敌人+视野框，0.3s 节流重建到 RenderTexture）；**世界地图**（M 键全屏，1格=1像素最近邻放大）。
- 中文字体打包 `assets/fonts/simsun.ttc`，加载顺序：打包字体→系统字体。
- **游戏内说明书**（v1.3.0，`GameUI::drawHelp`）：H / F1 或左上角「帮助」按钮打开；6 个标签页——新手引导（7 步上手）/ 操作按键 / 建筑一览 / 生产与物流 / 电力系统 / 常见问题，文案集中在 `GameUI.cpp` 末尾匿名 namespace 的 `buildHelpPage()`，建筑说明在 `buildingHelp()`（按 `BuildingType` switch，新增建筑时记得补）。滚轮滚动用 `helpScroll_`（绘制时按内容高度钳制上限）；与商店/工作台/世界地图互斥。
  ⚠ **v1.3.3 变更**：`Game` 构造末尾的 `if (!hasSave()) ui->openHelp(0)`（新开局自动弹出）**已删除**，`autoOpenHelp` 设置也随之删除；系统性教学改由**「新手教程」独立模式**承担（见 §5.11）。
- **新手教程 / 叙事播报**（v1.3.3）：
  - `systems/TutorialSystem.*`：章节/步骤/任务/进度，进度存 `saves/tutorial.json`；顶部引导横幅 + 跳过按钮，`F2` 跳过/重开；教程模式不出自动波次，敌人由教学步骤手动召唤。
  - `systems/Narrative.*`：事件播报 + 右下角通讯条 `GameUI::showComms`（头像/说话人/台词，独立通道，不被 Toast 挤掉）；教程模式下不叠加。
  - 入口：`Game::gameMode`（`Normal` / `Tutorial`），由 `ui/EntrySystem` 主菜单选择（`T` 教程 / `N` 普通关卡）。

### 5.10 快捷键总表
WASD 镜头 / 滚轮缩放 / 空格暂停 / 1塔2电塔3矿机4管道5桶6熔炉7组装机8发电机9电线杆0燃煤发电机 -电容 =电线 \分流器 / TAB循环 / 左键放置（带方向弹窗） / 右键：**矿机→采矿场设置面板（模式/矿种/旋转输出面）**、组装机配方、通物设备→终端面板、电线机器桶发电机→面编辑器 / DEL拆除返还 / Z X C U 刷怪 / B 商店 / V 工作台 / M 世界地图 / H F1 说明书 / **F2 教程跳过·重开** / **F5 打开保存槽位页 / F9 打开读取槽位页（10 槽位；教程模式不保存/不读取）** / ESC：有面板先关面板，否则打开暂停面板（继续游戏/保存存档/载入存档/设置/返回主界面）

> 主菜单快捷键：`T` 新手教程 / `N` 普通关卡 / `L`（或 `C`）载入存档。

---

## 6. 建筑类型索引（存档 type 字段；**旧值不可改，新增只能追加末尾**）

0 TowerBasic, 1 TowerRapid, 2 TowerSniper, 3 TowerElectric,
4 Miner, 5 MinerL2, 6 MinerL3, 7 MinerVoid,
8 Furnace, 9 Assembler, 10 Generator(旧版大功率), 11 PowerPole,
12 PowerGenerator, 13 Capacitor, 14 PowerWire,
15 Pipe, 16 Bucket, 17 Splitter, 18 AlloyFurnace,
19 MeInterface, 20 MeDrive, 21 MeTerminal
（MachineKind：Miner/Furnace/AlloyFurnace/Assembler）

---

## 7. 建筑放置与拆除（Game.cpp）

- `canPlace`：越界/占用检查；**管道可放在路径上**（其余建筑要求草地）；占地范围来自 `cfg::buildingSize(t)` 逐格扫描（**当前全部建筑 1×1**）。
- `placeBuilding(tx,ty,t,dir,deduct)`：创建 Building+按类型加组件 → `registerToGrid` → 管道/分流器/ME设备时 `PipeSystem::updateNeighbors` + `MeSystem::markDirty` → 电网相关 `power.dirty=true`。
- `removeBuilding`：退费 → 注销网格 → destroy → 物流掩码刷新 / 电网脏标记。
- 成本：`BUILDING_INFOS`（GameConfig.h，可被 config.json `building_costs` 覆盖）；**测试版资源无限**（RESOURCE_INFINITE=true）：`canAfford` 恒真、`deductCost/refundCost` 空操作、全部20物品发999999、读档后补全（这是用户明确要的"对内测试版所有资源无限"）。

---

## 8. 数值配置（两处，缺一不可）

### 8.1 `src/GameConfig.h` —— 内置默认值（中文注释）
- `inline` 变量 = 可被 JSON 覆盖（注释标 `[JSON可调]`）；`inline constexpr` = 编译期常量。
- 改这里需要重新编译。
- 注意顺序依赖：SMELT_TIME_* 声明在配方表之前；BuildingType 枚举在 SHOP_OFFERS 之前。

### 8.2 `assets/config.json` —— 运行时覆盖（★用户最看重："改数值无需重编译"）
- **游戏实际读取的是 `build/assets/config.json`**（相对 cwd）。改源码 assets 版要重新构建（自动复制）；改 build 版重启即生效。
- 加载器 `ConfigLoader::loadConfig`（Game 构造函数最先调用）：**只覆盖存在的字段**，缺字段用内置默认值；JSON 解析失败静默回退默认。
- 主要键：initial_gold/lives、wave_*、infinite_resource/resource_infinite（无限资源开关）、ore_counts{8种}+ore_infinite+ore_deposit_amount、miner_radius/rate_*+void_miner_rate、smelt_time_*、tower_stats{4种}、enemy_stats{3种}、assembler/furnace/alloy_furnace/crafting_recipes（可增删配方）、shop_offers、building_costs、电网项(powergen_output_eu_s/alloy_furnace_energy=16/capacitor_*)、pipes_*、splitter_*、me_*。
- 配方表在 JSON 里可以**增删条目**（loadRecipes 全量替换 vector）。
- ⚠ 物品键名/建筑键名必须与 §5.1/§6 一致（ItemSystem::parse、ConfigLoader::parseBuilding）。

---

## 9. 存档（SaveSystem.cpp，JSON：build/saves/slot_01.json … slot_10.json）

- **v1.3.4 起为「手动保存 · 10 槽位」**（参考 WorldBox）：`SAVE_SLOT_COUNT=10`，槽位文件 `saves/slot_NN.json`；**没有任何自动保存路径**，只有玩家在暂停面板/主菜单点保存或按 F5 才写盘。
  - `SaveSlotInfo{path, used, corrupt, bytes, savedAt, summary}`；`querySlot(i)` 解析摘要（`金币 · 第 N 波 · 建筑数`），解析失败→`corrupt`。
  - `saveGameToSlot / loadGameFromSlot / deleteSlot / anySlotUsed / newestSlot`；底层 `saveGameToFile / loadGameFromFile`（自检复用）。
  - 写入 `j["v"]=2; j["saved_at"]=本地时间串`；**旧 v1 存档仍可读**（缺 `saved_at` 只影响显示）。
  - `migrateLegacySave()`：旧单档 `saves/factory_td.json` → `saves/slot_01.json`（槽位 1 已占用则不动）。
  - UI：启动菜单「载入存档」`L`/`C` + 暂停面板 5 项，共用 2×5 槽位页；覆盖/删除前二次确认、默认焦点「取消」；`F5`/`F9` = 打开保存/读取槽位页（不再直接读写盘）。
- 另有**独立**的教程进度存档 `saves/tutorial.json`（v1.3.3，`systems/TutorialSystem.*`），与主存档**互不影响**；**教程模式调用 `saveToSlot/loadFromSlot` 会被拒绝并 Toast**（不占用玩家槽位）。
- **保存顺序**：v/gold/lives/wave状态/cam_x,cam_y/inv → ores(含amount) → buildings → pipes(缓冲) → me_networks → enemies。
- **加载顺序**：解析JSON(失败直接返回，不清世界) → 清空reg+网格 → 矿点 → 建筑(placeBuilding+恢复各类型状态+统一恢复faces) → 管道缓冲(按坐标直接回填，不再重复place) → ME网络(rebuildNetworks后按编号回填) → 敌人 → 全局状态 → 无限资源补全 → power.dirty。
- **每类建筑保存/恢复的内容**：塔(ammo/barrel/power)、矿机(level/void/acc/**库存items** + v1.3.3 新增 **fixed_ore/ore_filter/has_bound/bound_x/bound_y**)、熔炉合金组装机(has_job/job_input/job_time/job_total/recipe/items)、发电机(coal/fuel_time/burn)、桶(items/output_timer)、电容(energy)、分流器(queue/output_index)、**所有 FaceConfig 统一保存 faces**、摄像机。
- 未保存（有意/可接受）：子弹、塔冷却、UI瞬态、管道计时器。
- ME 网络编号确定性：布局相同→行主序BFS编号相同→按数组下标回填。
- ⚠ **读档不做"自动吸附/重排"**：v1.3.3 采矿场的固定矿点模式在**读档时只回填缓存字段、绝不调用 `snapBeside`**（否则旧存档的建筑会在读档瞬间被挪位置）。`bound_x/bound_y` 与矿点 `GridPos` 对不上时，由 `MinerSystem::boundDeposit()` 在运行时回退重新绑定。

### 9.1 ⚠ 存档史上最严重 bug（已修，勿重犯）
读档代码曾写 `bj.value("items", json::object()).begin()/end()`——两次 `value()` 产生**两个临时对象**，迭代器悬空 → 只要库存非空读档就崩，方块物品全丢。**规则：`value()` 结果必须先绑定 `const json&` 再取 begin/end**（全项目已清理4处，新代码同样遵守）。

---

## 10. 渲染与贴图

- RenderSystem 分 13 层顶点数组批量渲染+视野剔除；贴图分辨率无关（drawGridSprite 归一化到 TILE_SIZE）。
- 程序化生成（AssetManager::generateStaticTextures）：地形(覆盖PNG)、熔炉/合金炉4方向、电容、发电机、矿机L2/L3/虚空4方向、5种新矿石（PNG缺失时兜底）。
- ⚠ **贴图朝向约定（v1.3.3 修正，勿再改回）**：`machine_miner/generator/assembler_<dir>.png` 的**后缀 == 视觉朝向**（PNG 由 `organize_sprites.py` 顺时针 `rotate(-dir*90)` 生成）；`AssetManager::machineKey(name, dir)` 直接用 `DIR_NAMES_4[dir]`（`rotated` 参数保留但**不再参与映射**），程序化箭头绘制的 `texDir` 也必须等于 `dir`。**三处必须同改。**（旧版沿用 Python 的 `(d+3)%4` 逆时针映射，导致全部机器贴图偏左 90°。）塔的 8 方向 `towerKey` + RenderSystem `setRotation` 是另一套，未受影响。
- 管道/分流器/ME设备在 RenderSystem 第4层程序化绘制（外壳+connMask连接条+端点暗色端口；ME青色）；ME接口菱形/存储容量条/终端屏幕。
- **纯紫块=PNG缺失回退**：`loadTexture` 失败插紫红；`tryLoadTexture` 失败不插（给程序化兜底让路）——新加"PNG可有可无+程序化兜底"的贴图用后者。

---

## 11. 踩过的坑清单（给下一个 AI 避雷）

1. EnTT 3.13：view 无 size/empty；each() 单组件2元组；结构化绑定数必须匹配。
2. `sf::Color` 非字面量 → 用 `inline const` 而非 `constexpr`。
3. `sf::String::fromUtf8` 需要双指针重载（begin/end）。
4. 负坐标取格要 floor（tileAt）。
5. 窗口最大化点击失效 → 全部布局用运行时窗口尺寸（updateLayout/Resized）。
6. DPI 150%：自动化测试坐标÷1.5。
7. LTO 警告→exit code 1 假象（见 §3.2）。
8. 存档 JSON 迭代器悬空（§9.1）。
9. 管道接收端遗忘（机器/发电机/ME设备都曾漏过）→ 新增"端点"时同步改 PipeSystem::acceptsItem/deliverItem。
10. ME 设备入口只认接口 → 已改为任意 ME 设备（meNetworkOf）。
11. 管道 buffer 双重保存→读档重复placeBuilding失败 → 已改为按坐标回填。
12. 无限资源只发4种物品 → 已改全物品发放+canAfford恒真。
13. 建筑枚举加成员**只能追加末尾**（存档 type 兼容）。
14. ItemType 加成员：枚举+ITEM_INFOS+ITEM_KEYS 三处同步。
15. 沙箱：pwsh 无状态（用 workdir）；禁写 CMakeLists 名文件/点文件/entt-3.13.2 目录；联网走 127.0.0.1:7897。
16. 机器输出面（FaceConfig）曾漏存 → 现在所有 FaceConfig 统一保存。
17. **文档只改 `build/` 副本**：`build/update.md`、`build/README.md` 是构建产物，改它们不会同步回源码目录。**文档源在根目录**（README.md / update.md / ai.md），已发生过落后两个版本的事故。
18. **菜单类界面的鼠标悬停不要每帧夺焦**（本轮踩过、已修，见 §0.3）：写 `if (r.contains(mp)) settingRow = i;` 会导致鼠标停住时键盘焦点被每帧弹回该行，↑↓/←→ 形同失效。**鼠标悬停只该改"高亮"，只有在鼠标真正移动时才允许它接管键盘焦点**（记住上一帧鼠标位置做比较）。
19. **不能在 `pollEvent` 事件循环内重建 SFML 窗口**：切换显示模式（全屏/分辨率）需重建窗口，必须延后到事件循环之外执行，否则崩溃/事件丢失。EntrySystem 用 `pendingDisplayApply` 标志在 `render()` 开头统一处理。
20. **exe 必须在 `build/` 下运行**：所有路径（assets / saves）都相对 cwd。在 `factory-td/` 下跑会写出杂散文件 `factory-td/saves/settings.json`（已实际发生）。
21. **贴图朝向映射不能"照抄 Python"**（v1.3.3 踩过、已修，见 §0.3-4）：C++ 版 PNG 是"顺时针生成、文件名即朝向"，却沿用了 Python 的逆时针键表 → 全部机器贴图偏左 90°。**改朝向映射前先看 `assets/sprites/machines/` 的文件名与箭头方向。** 相关三处（`machineKey` / PNG 路径 / 程序化 `texDir`）必须一起改。
22. **SFML `setVerticalSyncEnabled` 与 `setFramerateLimit` 互相拆台**（v1.3.3 踩过）：必须走 `gset::applyFrameMode()` 唯一入口，按固定顺序"二选一"；别在别处再直接调这两个 setter。
23. **一切"速度"乘 `dt`、"平滑"用帧率无关形式**（v1.3.3 踩过）：`Camera` 原先每帧固定步长，144Hz 屏上比 60Hz 快 2.4 倍。新增移动/动画/计时同理。
24. **新增存档字段必须"可缺省"**（v1.3.3 采矿场模式字段的示范）：读取一律 `bj.value("key", 默认值)`，旧存档缺键时回落到旧行为（`fixed_ore` 缺省 = false = 原有模式），且**读档不做任何"自动重排/吸附"**，避免旧存档被悄悄改动。**存档格式只增不改不删**（`type` 字段值尤其不能改）。
25. **教程进度的落盘必须与主存档隔离**（v1.3.3）：`saves/tutorial.json` 独立文件；教程模式内 `F5`/`F9` 明确提示"不保存/不读取"，**绝不能共用主存档**，否则教学中的临时建造会污染玩家正式存档。
26. **本作是手动存档，禁止再引入任何自动写盘路径**（v1.3.4）：`Game` 构造末尾、`run()` 退出、`returnToMenu`、`EntrySystem` 退出等处**一律不得调用保存**；写盘只发生在玩家明确选择槽位时（`Game::saveToSlot`）。**"新开一局 → 直接关窗"绝不能覆盖任何存档**——这是本版重构的根本目的，回退即事故。
27. **旧符号清理要彻底**（v1.3.4 踩过）：改接口（`saveGame/loadGame`→`saveToSlot/loadFromSlot`、删单档 `kSavePath`/`Dialog::ConfirmNewGame`）后，务必全仓搜索旧名（`main.cpp` 自检、`PlayerSystem` 的 F5/F9、`EntrySystem::advanceLoading/requestStart` 都曾残留引用）——**编译通过 ≠ 全部清理**，但旧符号残留必然编译失败，所以改完**立刻编译**。
28. **建筑尺寸只能从 `cfg::buildingSize()` 取**（v1.3.4）：以前 `canPlace` 和 `placeBuilding` 各自内联 `t == Generator ? 2 : 1`，改一处忘另一处就会"能放但只占1格"（该 2×2 已废止，现全部 1×1）。新增任何多格建筑：**只改 `BUILDING_INFOS` 那一行的 `w/h`**，其余全部自动跟随（`registerToGrid`/`removeBuilding`/`hitBuilding`/`drawGridSprite` 本来就按 `b.w/b.h` 走）。
29. **多格建筑别只扫"左上格"的邻居**（v1.3.4）：任何"遍历建筑四邻"的代码都可能是 1×1 思维。用 `faceNeighborTiles(b, dir)` 逐格遍历每个面的外侧格——旧版 2×2 发电机曾因此只有左/上方能进煤（下/右两面接管道无效）。注意 `continue` 的作用域会从"面向"变成"格向"，语义上是变好了，但要确认无误。

---

## 12. 未完成 / 用户明确暂缓事项

| 事项 | 状态（用户原话） |
|---|---|
| **手动存档 · 10 槽位（v1.3.4，已发布）** | **代码完成、编译/自检/冒烟通过、文档已同步、人工实机走查已通过（2026-09-15 用户实机）**；GitHub Release `Alpha v1.3.4` 已上线（Latest），发布包已剔除版权字体（见 §0.1-I / §0.1-J）；§0.4 清单转为回归参考 |
| 新手教程模式 + 叙事播报（v1.3.3，已发布） | **代码完成、编译/自检/冒烟通过、文档已同步**；遗留 = 人工实机走查（教程全流程 / 通讯条观感，见 §0） |
| 采矿场双模式（v1.3.3，已发布） | 代码完成；遗留 = 实机确认"吸附位置 + 两模式产率一致 + 读档恢复"（见 §0） |
| 贴图朝向 / 高刷速度 / 高DPI 修复（v1.3.3，已发布） | 代码完成；遗留 = 实机目视确认（这三项**必须人眼验证**，见 §0.3） |
| 游戏内暂停面板（v1.3.2，已发布） | **代码完成、编译/自检/冒烟通过、文档已同步**；唯一遗留 = 人工实机视觉走查（见 §0） |
| 启动入口系统（v1.3.1，已发布） | 代码完成、编译/自检/冒烟通过、文档已同步；人工视觉走查见 §0 |
| 游戏内"设置"入口 | 未做（当前设置只在启动菜单/暂停面板改）；做之前先问用户 |
| 商店正式配方表 | **以后再写**；当前 shop_offers 是占位（示例"200币+10电路板=1组装机"禁止加入） |
| 音效 | 以后再加 |
| 粒子特效 | 性能问题暂时不加 |
| 自动化测试入库 | 暂时不用（但已有 --selftest-save 可用） |
| 波次"显示刷怪"UI | 目前不打开（wave_auto_spawn=false；分键召唤已做） |
| 矿机合成表 | 待定（逻辑已写好，造价是占位） |
| 游戏内说明书 | **已完成**（v1.3.0：H / F1 + 左上角帮助按钮，6 标签页；v1.3.3 起**不再新开局自动弹出**） |
| 新手教程模式 | **已完成**（v1.3.3：主菜单独立入口 `T`，`systems/TutorialSystem.*`，`F2` 跳过/重开，进度存 `saves/tutorial.json`） |
| 叙事 / 织女星通讯 | **已完成**（v1.3.3：`systems/Narrative.*` + `GameUI::showComms`） |
| 性能基准 | 用户说不需要 |
| 电网损耗/过载 | 设计如此，不改 |
| 击杀掉落 | 只加金币，不产生掉落（按需求） |

可能被用户后续提出的方向（来自对话）：逐格传递式管道（用户最终选了瞬时+网络全局显示，但可能再改主意）、更多合金/配方、正式商店价目、音效、GUI 精修、AI 敌人、波次开关 UI。

---

## 13. 用户交流偏好（重要）

- **中文**交流；代码注释、文档全部中文。
- 需求源：`add.txt` 是最高优先级；口头反馈常以"为啥XXX"提问——**先解析根因（最好看存档/布局数据）再修**，修完解释清楚。
- 用户会实际游玩并截图报 bug（图里信息量很大，仔细读布局）。
- 标准：**"代码合理简洁无bug可扩展"**；每批改动必须编译+运行验证；数值必须集中可配置（改数值不重编译）。
- 用户对"设计如此"的决定要尊重（如无损耗电网、测试版无限资源、波次关闭）。
- 改动记录：每次有实质变更要更新 `update.md`（Python 日志格式：## Alpha vX.Y.Z (当前版本 - 日期) + ### 新增功能/Bug修复/系统优化）、README、PORTING.md 同步。

---

## 14. 版本与文档

- 代码版本进度见 `update.md`：**Alpha v1.3.4 为当前版本**（**手动存档 · 10 槽位**，参考《世界盒子》；旧单档自动迁移到槽位 1）；v1.3.3 品牌重塑《织星计划》+ 新手教程模式 + 叙事播报 + 采矿场双模式 + 贴图/高刷/高DPI 修复 + ME→通物；v1.3.2 游戏内暂停面板 + 设置项共用化；v1.3.1 启动入口系统 + 跨场景用户设置；v1.3.0 游戏内说明书 / 新手引导；v1.2.4 商店与背包重构 + 合金炉改为选定配方且无需电力；v1.2.3 ME 接口输出过滤 + GUI/贴图重设计；v1.2.1 ME 网络 + 存档修复；v1.2.0 为 add.txt 主体功能（含管道设计解析）；v1.1.0 为早期重构。
- 注意 update.md 里的日期（08-15~17）比实际开发日（08-25）滞后一周左右，继续写时用真实日期即可。
- Python 老版在 `python版（老版）/`，PORTING.md 有逐文件对照，不再维护。
