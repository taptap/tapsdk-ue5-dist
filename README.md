# TapSDK UE5

TapSDK UE5 源码插件发布仓库。SDK 版本以 Git tag 和 GitHub Release 为准，开发者通过 Release 附件按需下载模块。

> 当前处于 Beta 阶段，仓库保持 Internal，仅供组织内成员验证。Beta 版本使用带后缀的版本号并标记为 Pre-release，暂不作为正式对外下载入口。

## 支持范围

| Unreal Engine | 状态 |
|---|---|
| 5.7 | 支持 |
| 5.8 | 支持 |
| 5.6 及更早版本 | 不兼容 |

发布包是需要项目自行编译的源码 Plugin，不固定 `.uplugin` 的 `EngineVersion`。每个正式支持的引擎版本都需要通过对应的编译和冒烟验证。

## 下载与安装

1. 打开 [Releases](https://github.com/taptap/tapsdk-ue5-dist/releases)，选择需要的版本。
2. 下载业务模块 zip 及其依赖模块，同时下载 `checksums.txt`。
3. 校验下载文件的 SHA256，例如：

   ```bash
   grep 'TapSDK-UE5-TapLogin-' checksums.txt | shasum -a 256 -c -
   ```

4. 将每个 zip 解压到项目的 `<ProjectRoot>/Plugins/` 目录。
5. 确认目录形如 `<ProjectRoot>/Plugins/TapCore/TapCore.uplugin`，然后重新生成工程文件并编译项目。

不要使用 GitHub 自动生成的 `Source code` 压缩包代替 Release 附件，也不要把整个仓库直接放进项目的 `Plugins` 目录。

## 模块依赖

下表列出每个模块的直接依赖。安装时需要递归补齐依赖模块；例如 TapLogin 依赖 TapCore，而 TapCore 还依赖 TapBasic 和 TapDB，因此这四个模块都需要下载。

| 模块 | TapSDK 直接依赖 |
|---|---|
| TapBasic | 无 |
| TapDB | TapBasic |
| TapCore | TapBasic、TapDB |
| TapLogin | TapCore |
| TapCompliance | TapCore、TapLogin |
| TapAchievement | TapCore、TapBasic、TapLogin |
| TapBattle | TapCore、TapLogin |
| TapCloudSave | TapCore、TapLogin |
| TapLeaderboard | TapCore、TapLogin、TapProfile |
| TapLicense | TapCore |
| TapMoment | TapCore、TapLogin |
| TapProfile | 无 |
| TapRelation | TapCore、TapLogin、TapProfile |
| TapRelationLite | TapCore、TapLogin、TapProfile |
| TapRep | TapCore |
| TapReview | TapCore |
| TapShare | TapCore |
| TapUpdate | TapCore |

## Android 依赖

Android 原生依赖由独立的 Android 发版流程维护，通过 Gradle/Maven 获取，不包含在 UE5 插件 zip 中。UE5 发布流程只校验所引用的 Maven 坐标，不构建或上传 AAR。

Beta 阶段按内部 Android 发版与接入说明验证。正式对外发布前，全部依赖必须能够从公开 Maven 仓库匿名下载，开发者不应配置内部仓库或访问令牌。

## 仓库内容

- `main`：已发布版本的插件源码，手动发布 Release 后由同步任务更新，用于审查和追溯。
- Git tag：指向对应版本的固定源码快照；创建草稿时生成，发布后不移动。
- Release：CI 创建草稿，维护者检查后手动发布；提供分模块 zip 和 `checksums.txt`。
- `release-manifest.json`：记录 SDK 版本、monorepo 源码提交、支持的引擎版本及模块校验值。

打 tag 或删除 Release 都不会自动修改 `main`。发布 Release 后才同步该版本源码；
撤版后的源码恢复需单独提交变更。

`.github/workflows/sync-published-release.yml` 与 `.github/scripts/ue5/` 是一次性初始化的
同步任务。后续 SDK 发版保留这些文件，任务自身的更新单独通过维护 PR 合入。
同步与历史脚本从 `main` 读取，版本 Tag 只提供该版本的源码与发布上下文。
补发低于 `main` 的版本时只跳过源码回退，公开版本的发布历史仍记录。
