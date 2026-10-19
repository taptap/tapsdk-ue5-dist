# TapDB Identity 机器绑定设计方案

> **状态**：已实现
> **基础能力位于**：`TapCore/Source/TapCore/Public/Internal/Storage/` — 可供其他模块复用
> **业务调用位于**：`TapCore/Source/TapCore/Private/Event/TapDBEventTracker.cpp`

---

## 1. 问题背景

### 1.1 原始行为

`FTapDBIdentity::Init()` 初始化时从缓存文件读取三个身份 ID：

| ID | 存储 Key | 生成方式 |
|----|----------|---------|
| `DeviceId` | `tapdb_unique_id` | Windows: MD5(进程创建时间 + PID)；Mac/Linux: MD5(高精度计时器 + PID) |
| `PersistentId` | `tapdb_persist_id` | `FGuid::NewGuid()` |
| `InstallationId` | `tapdb_install_id` | `FGuid::NewGuid()` |

缓存文件路径：`{ProjectSavedDir}/TapDB/tapdb_storage_{clientId}.json`

读取逻辑：**缓存命中则直接使用，不命中才生成新值**。

### 1.2 问题

```
机器 A                              机器 B
┌─────────────────────┐             ┌─────────────────────┐
│  tapdb_storage.json │  ──拷贝──►  │  tapdb_storage.json │
│  device_id: "aaa"   │             │  device_id: "aaa"   │  ← 同一个 ID！
│  persist_id: "bbb"  │             │  persist_id: "bbb"  │  ← 同一个 ID！
│  install_id: "ccc"  │             │  install_id: "ccc"  │  ← 同一个 ID！
└─────────────────────┘             └─────────────────────┘
```

缓存文件没有与机器绑定，天然可移植，导致：
- 不同物理机上出现相同 device_id，数据归因错误
- persistent_id / installation_id 重复，无法区分独立安装
- 未发送的事件队列也被拷贝，可能导致事件重复上报

---

## 2. 解决方案：机器指纹绑定 + 物理清空

### 2.1 核心思路

缓存文件中存入一个机器指纹（`tap_machine_fp`）。启动时比对指纹：

- **匹配** → 正常使用缓存
- **不匹配** → 物理清空所有缓存数据 + 清空事件队列 → 上层通过原有的"读空 → 生成 → 写入"逻辑自然重建

无需 `bIsValid` 标志，无需 checksum。数据被物理清空后，缓存真的是空的，上层代码零改动。

---

## 3. 架构

### 3.1 模块分层

```
TapCore 基础层（基础能力，可复用）
├── FTapPrefs                  — 通用 JSON KV 存储
└── FTapMachineBoundPrefs      — 机器绑定缓存层（代理 FTapPrefs）

TapCore 事件层（业务层）
├── FTapDBIdentity             — 使用 FTapMachineBoundPrefs 读写 Identity ID
├── FTapDBUserManager          — 使用 FTapMachineBoundPrefs 读写用户/时长数据
├── FTapDBEventSender          — 独立管理事件队列文件
└── FTapDBEventTracker         — 组装以上组件，处理机器失效通知
```

### 3.2 依赖关系

```
FTapDBEventTracker
    │
    ├── FTapMachineBoundPrefs      ← 所有本地 KV 存储的唯一入口
    │       └── FTapPrefs          ← 内部实现，外部不可见
    │
    ├── FTapDBIdentity             ← 通过 FTapMachineBoundPrefs 读写
    ├── FTapDBUserManager          ← 通过 FTapMachineBoundPrefs 读写
    └── FTapDBEventSender          ← 独立文件，通过 WasInvalidated() 通知清空
```

---

## 4. 机器指纹

### 4.1 来源：分层策略

不使用 MAC 地址、CPU 序列号、磁盘序列号——这些在虚拟环境、容器、云主机中不稳定且权限要求高。
使用 UE 平台层封装的稳定 OS 标识：

```
主指纹：FPlatformMisc::GetMachineId()
    Windows → HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography\MachineGuid
    macOS   → IOPlatformUUID（Hardware UUID）
    Linux   → /etc/machine-id

Fallback：FPlatformMisc::GetDeviceId()
    主指纹为空时启用

最终指纹 = GetMachineId() 非空 ? GetMachineId() : GetDeviceId()
```

直接存储原始值，不做哈希 — 仅用于本地比对，不上报服务端，无需脱敏。

### 4.2 指纹稳定性

| 场景 | GetMachineId() 是否变化 |
|------|----------------------|
| 重启游戏 | 不变 |
| 重新安装游戏 | 不变（OS 级别标识） |
| 普通文件拷贝到另一台机器 | 变化（核心防护场景）|
| 重装操作系统 | 变化（新 MachineGuid 生成） |
| 虚拟机还原快照 | 不确定（取决于 VM 配置） |

---

## 5. 缓存存储结构

### 5.1 文件格式

文件路径：`{ProjectSavedDir}/TapDB/tapdb_storage_{clientId}.json`

```json
{
  "tap_machine_fp":  "MachineGuid-value",

  "tapdb_unique_id":  "device-id-value",
  "tapdb_persist_id": "persist-id-value",
  "tapdb_install_id": "install-id-value",
  "tapdb_played_duration_user_id": "user123",
  "tapdb_played_duration": 12345
}
```

| 字段 | 写入方 | 说明 |
|------|--------|------|
| `tap_machine_fp` | `FTapMachineBoundPrefs`（自动） | 机器指纹，每次 Save 自动更新 |
| `tapdb_unique_id` | `FTapDBIdentity` | 设备唯一 ID |
| `tapdb_persist_id` | `FTapDBIdentity` | 跨会话持久 ID |
| `tapdb_install_id` | `FTapDBIdentity` | 安装 ID |
| `tapdb_played_duration_user_id` | `FTapDBUserManager` | 当前登录用户 ID |
| `tapdb_played_duration` | `FTapDBUserManager` | 累计游玩时长（秒） |

### 5.2 不使用 checksum

原方案设计了 checksum 字段，最终实现中删除：
- JSON 解析失败已由 `FTapPrefs::Load()` 处理（返回空对象）
- checksum 只能防意外损坏，不能防恶意伪造
- 增加一个字段 + 计算逻辑带来的复杂度 > 收益

---

## 6. 初始化流程

```
FTapDBEventTracker::Init(ClientID)
│
├── 1. FTapMachineBoundPrefs::Init("TapDB/tapdb_storage_{clientId}.json")
│       ├── FTapPrefs::Init() — 加载 JSON 文件
│       ├── 计算当前机器指纹
│       ├── 与存储的 tap_machine_fp 比对
│       │     ├── 匹配 → return（缓存有效）
│       │     └── 不匹配 → Prefs.Clear() + bWasInvalidated = true
│       └── 写入指纹 + Save
│
├── 2. FTapDBIdentity::Init(Prefs)
│       ├── 读 DeviceId → 空（被清空了）→ GenerateDeviceId() → 写入
│       ├── 读 PersistentId → 空 → NewGuid → 写入
│       └── 读 InstallationId → 空 → NewGuid → 写入
│
├── 3. FTapDBEventSender::Init(ClientID, Region)
│       └── LoadEvents() — 加载事件缓存文件
│
├── 4. if Prefs->WasInvalidated():
│       └── EventSender.ClearCache() — 清空事件队列 + 删除事件缓存文件
│
├── 5. FTapDBUserManager::Init(Prefs)
│       ├── 读 user_id → 空 → 不启动录制
│       └── 读 played_duration → 0
│
└── 正常启动，所有本地状态属于当前机器
```

---

## 7. DeviceId 生成逻辑（对齐 Android TUDBDevice::GetDeviceId5）

### 7.1 三平台统一模式

`MD5(唯一时间标识 + "-" + PID)` → 32 位小写十六进制字符串

| 平台 | 时间标识来源 | PID 来源 |
|------|------------|---------|
| Windows | `GetProcessTimes()` 进程创建时间 | `GetCurrentProcessId()` |
| Mac / Linux | `FPlatformTime::Cycles64()` 高精度计时器 | `FPlatformProcess::GetCurrentProcessId()` |

### 7.2 与 Android 对比

| | Android | UE5 (Windows) | UE5 (Mac/Linux) |
|-|---------|---------------|-----------------|
| 时间源 | `firstInstallTime` | `processCreationTime` | `Cycles64()` |
| PID | `Process.myPid()` | `GetCurrentProcessId()` | `GetCurrentProcessId()` |
| 前缀 | `v1-` | 无 | 无 |
| 持久化 | 否（每次重新生成） | 是（缓存） | 是（缓存） |

Windows 端 MD5 计算与参考代码 `TUDBDevice::GetDeviceId5()` 输出完全一致。

---

## 8. 文件清单

### 8.1 TapCore 基础层（基础能力，新增）

| 文件 | 说明 |
|------|------|
| `Public/Internal/Storage/TapPrefs.h` | 通用 JSON KV 存储，`TAPCORE_API` 导出 |
| `Private/Storage/TapPrefs.cpp` | 实现：Init / Get / Set / Remove / Clear / Save / Load |
| `Public/Internal/Storage/TapMachineBoundPrefs.h` | 机器绑定缓存代理，`TAPCORE_API` 导出 |
| `Private/Storage/TapMachineBoundPrefs.cpp` | 实现：指纹计算 + 校验 + 物理清空 + WasInvalidated |

### 8.2 TapCore 事件层（业务层，修改）

| 文件 | 变化 |
|------|------|
| `TapDBIdentity.cpp` | DeviceId 生成改为 MD5(processTime+PID)，使用 `FTapMachineBoundPrefs` |
| `TapDBIdentity.h` | 参数类型 `FTapMachineBoundPrefs&` |
| `TapDBUserManager.cpp / .h` | 同上，类型替换 |
| `TapDBEventTracker.cpp / .h` | 创建 `FTapMachineBoundPrefs`，构造路径，处理 WasInvalidated |
| `TapDBEventSender.cpp / .h` | 新增 `ClearCache()` 方法 |
| `TapDBConstants.h` | 移除 `PREF_MACHINE_FINGERPRINT`（已内置于 TapBasic） |

### 8.3 已删除

| 文件 | 原因 |
|------|------|
| `TapDBPrefs.h / .cpp` | 迁移到 TapBasic → `FTapPrefs` |
| `TapDBMachineBoundPrefs.h / .cpp` | 迁移到 TapBasic → `FTapMachineBoundPrefs` |

---

## 9. 其他模块复用示例

```cpp
#include "Internal/Storage/TapMachineBoundPrefs.h"

FTapMachineBoundPrefs Prefs;
Prefs.Init(TEXT("TapLogin/tap_login_cache.json"));

if (Prefs.WasInvalidated())
{
    // 机器变了，清理登录态等
}

FString Token = Prefs.GetString(TEXT("access_token"));
Prefs.Set(TEXT("access_token"), NewToken);
Prefs.Save();
```

---

## 10. 已知局限

| 场景 | 能否防止 | 说明 |
|------|---------|------|
| 普通文件拷贝到另一台机器 | ✅ 能防 | 本方案核心场景 |
| 克隆镜像（sysprep 前）/ 同 MachineGuid | ❌ 不能防 | OS 层面限制 |
| 虚拟机还原快照 | ❌ 不能防 | MachineGuid 随快照恢复 |
| GetMachineId() 在某些平台返回空 | ⚠️ 降级 | Fallback 到 GetDeviceId() |
| 恶意伪造缓存文件 | ❌ 不能防 | 无 HMAC，超出 SDK 职责 |

---

## 11. 隐私合规

- `GetMachineId()` 仅用于本地比对，**不上报服务端**
- 性质与 SDK 现有的 DeviceId 收集行为相同，不引入新的合规风险
- 隐私政策中应已有"收集设备唯一标识"的说明
