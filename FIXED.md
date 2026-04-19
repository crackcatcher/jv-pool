**结论**

严格意义上看，这个库在“正常创建并最终调用 `jv_pool_reset()` / `jv_pool_destroy()`”的路径上，没有明显的“分配后丢失引用、再也释放不到”的固定内存泄露；新增 block 都挂在 `pool->first` 链表上，`reset/destroy` 会统一释放它们，[jv_pool.c#L195](/Users/data/dev/jv-pool/jv_pool.c#L195) [jv_pool.c#L261](/Users/data/dev/jv-pool/jv_pool.c#L261) [jv_pool.c#L369](/Users/data/dev/jv-pool/jv_pool.c#L369) [jv_pool.c#L391](/Users/data/dev/jv-pool/jv_pool.c#L391)。

但这个工程有几个更严重的问题，其中一个已经在当前 `arm64` 机器上被 UBSan 实测打出来，属于未定义行为，风险高于“普通泄露”。

**主要问题**

- 严重: 对齐策略在 `arm64` 上是错的，会导致 `jv_lump_t` 和返回给用户的指针只有 4 字节对齐，但该结构实际需要 8 字节对齐。根因是用 `__x86_64__` 判断 64 位平台，导致 `arm64` 落到 `JV_WORD_SIZE=32`，之后所有 `jv_align(..., JV_WORD_SIZE / 8)` 都按 4 对齐。[jv_pool.h#L8](/Users/data/dev/jv-pool/jv_pool.h#L8) [jv_pool.h#L25](/Users/data/dev/jv-pool/jv_pool.h#L25) [jv_pool.c#L24](/Users/data/dev/jv-pool/jv_pool.c#L24) [jv_pool.c#L124](/Users/data/dev/jv-pool/jv_pool.c#L124) [jv_pool.c#L69](/Users/data/dev/jv-pool/jv_pool.c#L69)  
  我本地跑 `UBSan` 时出现了大量 `misaligned address`，这不是告警噪音，而是真实 UB。

- 高: `jv_pool_realloc()` 语义是错的。它先 `free` 旧块，再重新 `alloc` 新块，完全没有拷贝旧数据；如果新分配失败，旧数据也已经丢了。[jv_pool.c#L183](/Users/data/dev/jv-pool/jv_pool.c#L183) [jv_pool.c#L184](/Users/data/dev/jv-pool/jv_pool.c#L184) [jv_pool.c#L185](/Users/data/dev/jv-pool/jv_pool.c#L185) [jv_pool.c#L128](/Users/data/dev/jv-pool/jv_pool.c#L128)  
  我单独验证过，`realloc` 后 `preserve=0`；对超大尺寸 `realloc` 时返回 `NULL`，原指针也已经失效。

- 高: `JV_POOL_QUICK_MODE` 的指针校验基本不安全。`jv_pool_exist()` 在 quick mode 下只是回退到 `ptr - sizeof(jv_lump_t)` 看 `size` 是否按对齐取模，并不检查这个指针是否真的属于当前 pool；之后 `free/recycle` 会直接改写那块内存。[jv_pool.c#L159](/Users/data/dev/jv-pool/jv_pool.c#L159) [jv_pool.c#L162](/Users/data/dev/jv-pool/jv_pool.c#L162) [jv_pool.c#L297](/Users/data/dev/jv-pool/jv_pool.c#L297) [jv_pool.c#L339](/Users/data/dev/jv-pool/jv_pool.c#L339)  
  这意味着 quick mode 下传错指针，可能直接破坏外部内存。

- 中: `free`/`sizeof`/`exist` 对“已释放指针”的行为不一致，且部分场景下重复释放不会被识别。`jv_pool_free()` 里原本的 `idle->used != 1` 检查被注释掉了。[jv_pool.c#L303](/Users/data/dev/jv-pool/jv_pool.c#L303) [jv_pool.c#L308](/Users/data/dev/jv-pool/jv_pool.c#L308) [jv_pool.c#L136](/Users/data/dev/jv-pool/jv_pool.c#L136) [jv_pool.c#L171](/Users/data/dev/jv-pool/jv_pool.c#L171)  
  我验证过一种场景：前后块都还在使用时，对同一个指针连续 `free` 两次，两次都返回 `JV_OK`；释放后 `jv_pool_sizeof()` 仍然还能返回原大小。

- 中: `free` 只做 lump 合并，不会回收空闲 block，所以长期运行时内存占用只涨不降，直到 `reset/destroy` 才会还给系统。这不算严格意义的 leak，但会表现成明显的“高水位常驻”。[jv_pool.c#L190](/Users/data/dev/jv-pool/jv_pool.c#L190) [jv_pool.c#L256](/Users/data/dev/jv-pool/jv_pool.c#L256) [jv_pool.c#L311](/Users/data/dev/jv-pool/jv_pool.c#L311) [jv_pool.c#L354](/Users/data/dev/jv-pool/jv_pool.c#L354) [jv_pool.c#L386](/Users/data/dev/jv-pool/jv_pool.c#L386)  
  我本地验证过：申请一个大块后再 `free`，`block_count` 仍保持增长后的值，不会下降。

- 中: 空指针防御不完整，`jv_pool_reset(NULL)` 和 `jv_pool_destroy(NULL)` 会先解引用再判断，直接崩溃。[jv_pool.c#L354](/Users/data/dev/jv-pool/jv_pool.c#L354) [jv_pool.c#L358](/Users/data/dev/jv-pool/jv_pool.c#L358) [jv_pool.c#L360](/Users/data/dev/jv-pool/jv_pool.c#L360) [jv_pool.c#L386](/Users/data/dev/jv-pool/jv_pool.c#L386) [jv_pool.c#L389](/Users/data/dev/jv-pool/jv_pool.c#L389)

- 低: 头文件可移植性一般。这里用了 `uint32_t` 却没有显式包含 `<stdint.h>`，还手工定义了 `intptr_t/uintptr_t`，在别的编译器或头文件组合下可能出问题。[jv_pool.h#L20](/Users/data/dev/jv-pool/jv_pool.h#L20) [jv_pool.h#L27](/Users/data/dev/jv-pool/jv_pool.h#L27) [jv_pool.h#L68](/Users/data/dev/jv-pool/jv_pool.h#L68)

**关于“是否有内存泄露”**

- 如果调用方最终会执行 `jv_pool_destroy()`，我没有看到库内部那种确定性的永久泄露点。
- 但如果你的关注点是“内存占用会不会越来越大、即使对象 free 了也不下来”，答案是会。因为额外 block 只有在 `reset/destroy` 才释放，这在服务型进程里会很像泄露。

**我做过的验证**

- `make` 可通过。
- 在当前 `arm64` 环境下用 `ASan/UBSan` 运行测试，出现大量未对齐访问报错。
- 我额外做了几个最小化验证：`realloc` 不保留数据、失败时丢原对象、部分场景下 `double free` 返回成功、释放大块后 `block_count` 不下降。

如果你要继续，我建议下一步优先修 3 件事：对齐策略、`realloc` 语义、`free`/`exist` 的状态校验。我可以直接给你出一版修复补丁。

---

下面给你一版按三档拆的“面向性能重构”方案。基线判断先说清楚：

当前瓶颈主要在这几处：

- 热路径日志输出很多，直接污染吞吐。[jv_pool.c#L164](/Users/data/dev/jv-pool/jv_pool.c#L164) [jv_pool.c#L182](/Users/data/dev/jv-pool/jv_pool.c#L182) [jv_pool.c#L201](/Users/data/dev/jv-pool/jv_pool.c#L201) [jv_pool.c#L278](/Users/data/dev/jv-pool/jv_pool.c#L278)
- 指针校验和 block 定位都是线性扫描。[jv_pool.c#L19](/Users/data/dev/jv-pool/jv_pool.c#L19) [jv_pool.c#L57](/Users/data/dev/jv-pool/jv_pool.c#L57)
- `alloc/free` 依赖全局 lump 链表遍历，规模一大就退化。[jv_pool.c#L169](/Users/data/dev/jv-pool/jv_pool.c#L169) [jv_pool.c#L427](/Users/data/dev/jv-pool/jv_pool.c#L427)
- `alloc` 总是清零，`realloc` 也走“重新分配+拷贝”。[jv_pool.c#L229](/Users/data/dev/jv-pool/jv_pool.c#L229) [jv_pool.c#L282](/Users/data/dev/jv-pool/jv_pool.c#L282)
- 头文件把内部结构全暴露了，后续大改很容易破坏兼容性。[jv_pool.h#L41](/Users/data/dev/jv-pool/jv_pool.h#L41) [jv_pool.h#L53](/Users/data/dev/jv-pool/jv_pool.h#L53)

**低改动**
目标：不推翻当前设计，尽量保持 API 兼容，先把最明显的性能损耗去掉。

建议改动：

- 把所有 `printf` 改成可编译关闭的调试宏，默认关闭。
- 新增 `jv_pool_alloc_nz()`，返回未清零内存；保留现有 `jv_pool_alloc()` 作为 zero-fill 包装。
- `jv_pool_realloc()` 增加原地优化：
  - 缩容时原地分裂
  - 后继 lump 空闲且足够时原地扩容
  - 只有不满足时才新分配+拷贝
- 给 `jv_lump_t` 增加 `owner block` 或轻量校验字段，去掉 `jv_pool_find_block()` 的线性扫描。
- `jv_pool_exist/free/sizeof` 在 safe mode 下改成“从 `ptr` 直接回退到 header 校验”，不要再扫全链表。
- 把 `recycle` 标成 deprecated，或者内部直接转发到 `free`，避免长期碎片。

收益预期：

- 单对象 `alloc/free` 会明显改善。
- `realloc` 成本会降很多。
- 不改调用方的前提下，通常是最划算的一档。

代价：

- 代码仍然是“通用 lump 链表”模型，复杂度上限没变。
- 对大量小对象 churn 场景，仍然不算最优。

适合场景：

- 你想先把库变成“可上线可用”，但不想大动接口。

工作量：

- 1 到 2 天。

**中改动**
目标：保留大体 API，但把内部从“全局扫描型 pool”升级成“size class + 通用块”的混合设计。

建议改动：

- 小对象走 size class：
  - 例如 `16/32/64/128/256/512/1024`
  - 每个 class 维护自己的 free list
  - 分配/释放尽量做到 O(1)
- 大对象继续走现有 block/lump 逻辑，作为 fallback。
- 小对象页使用 slab/page：
  - 一个 page 只服务一个 size class
  - 空闲块用单链表串起来
  - page 全空时可回收
- 指针头部改成紧凑元数据：
  - class id
  - owner page/block
  - flags
- `reset` 分成两类：
  - arena/bulk 区域直接 O(1) 回卷
  - slab 区域批量重置 free list
- API 补齐：
  - `jv_pool_alloc_nz`
  - `jv_pool_stats`
  - `jv_pool_reserve`
  - `jv_pool_trim`
- `jv_pool_recycle` 重新定义，否则建议删掉。
- 加一组正式 benchmark，至少覆盖：
  - 小对象高频 alloc/free
  - 批量 alloc + reset
  - 大对象混合场景
  - 碎片场景

收益预期：

- 小对象性能通常会有数量级提升。
- 指针校验和 free 不再依赖全局扫描。
- 碎片和 block size 敏感性会明显下降。

代价：

- 内部实现复杂度会高一个量级。
- 测试矩阵要补全，不然很容易引入 slab 边界 bug。

适合场景：

- 你的主要负载是大量小对象，或者希望这个库真正成为“通用池”。

工作量：

- 3 到 6 天。

**重构版**
目标：把它从“内存池封装”重做成“按场景分层的 allocator 组件”。

建议架构：

- `arena` 层：
  - 面向“请求级/阶段级”分配
  - bump pointer 分配
  - `reset` O(1)
  - 极致快，但不支持单对象 free 或只做弱支持
- `slab` 层：
  - 面向高频小对象复用
  - 固定 size class
  - O(1) alloc/free
- `large object` 层：
  - 专门管理大块
  - 独立链表
  - 支持回收、trim、统计
- `debug/stats` 层：
  - 可选 poison
  - double free 检查
  - usage/high-watermark
  - 碎片率统计
- `backend` 层：
  - 支持自定义底层 allocator
  - 方便接 jemalloc/tcmalloc 风格环境或嵌入式环境

同时建议 API 也重构：

- 对外隐藏 `jv_block_t/jv_lump_t`，改成真正 opaque handle。
- 把“按用途”接口拆开：
  - `jv_arena_alloc`
  - `jv_pool_alloc`
  - `jv_pool_alloc_nz`
  - `jv_pool_free`
  - `jv_pool_reset`
- 如果需要并发，再加：
  - thread-local arena
  - central page cache
  - optional lock-free free list

收益预期：

- 能把“批量生命周期”和“小对象复用”两类场景都吃下来。
- 可维护性、可测试性、可扩展性都会比当前版本高很多。
- 这是唯一一档能比较合理地谈“性能和功能都完善”的方案。

代价：

- 基本不再是“小修小补”，而是新 allocator 设计。
- 兼容性需要专门做迁移层。

适合场景：

- 你想把这个项目做成真正可复用的基础库，而不是 demo/轻量工具。

工作量：

- 1 到 2 周。

**我建议的路线**

如果你要稳妥推进，我建议按这个顺序：

1. 先做“低改动”  
   先把日志、原地 `realloc`、非清零分配、O(1) header 校验解决掉。

2. 再决定是否进“中改动”  
   如果实际负载以小对象高频 churn 为主，就上 size class/slab。

3. 只有在要做成长期基础库时，才上“重构版”  
   否则投入产出比未必划算。

如果你愿意，我下一步可以直接给你出一份“低改动版本”的实施清单，精确到要改哪些结构体、哪些函数、哪些测试。

---

按“低改动”那档直接改完了，重点在 [jv_pool.h](/Users/data/dev/jv-pool/jv_pool.h) 和 [jv_pool.c](/Users/data/dev/jv-pool/jv_pool.c)。

- 加了 `JV_POOL_ENABLE_LOG`，默认关闭热路径日志，避免 `printf` 直接拖垮吞吐。[jv_pool.h#L16](/Users/data/dev/jv-pool/jv_pool.h#L16)
- 新增 `jv_pool_alloc_nz()`，给不需要清零的场景走更快路径。[jv_pool.h#L87](/Users/data/dev/jv-pool/jv_pool.h#L87) [jv_pool.c#L399](/Users/data/dev/jv-pool/jv_pool.c#L399)
- 给 pool 加了指针索引表，`exist/sizeof/free/realloc` 不再扫 lump 链表做指针定位，主路径改成 O(1) 查找。[jv_pool.h#L66](/Users/data/dev/jv-pool/jv_pool.h#L66) [jv_pool.c#L68](/Users/data/dev/jv-pool/jv_pool.c#L68) [jv_pool.c#L176](/Users/data/dev/jv-pool/jv_pool.c#L176)
- 给 lump 增加了 `block` 回指针，去掉了释放时再扫 block 链表定位归属 block 的成本。[jv_pool.h#L58](/Users/data/dev/jv-pool/jv_pool.h#L58) [jv_pool.c#L503](/Users/data/dev/jv-pool/jv_pool.c#L503)
- `realloc` 现在先尝试原地缩容/扩容，只有不满足时才走“新分配+拷贝”。[jv_pool.c#L536](/Users/data/dev/jv-pool/jv_pool.c#L536) [jv_pool.c#L603](/Users/data/dev/jv-pool/jv_pool.c#L603)

我也补了文档和回归测试，尤其是 `alloc_nz`、失败 `realloc`、以及原地扩容语义。[README.md#L107](/Users/data/dev/jv-pool/README.md#L107) [README.md#L188](/Users/data/dev/jv-pool/README.md#L188) [jv_pool_test.c#L341](/Users/data/dev/jv-pool/jv_pool_test.c#L341)

验证结果：

- `make` 通过
- `./jv_pool_test` 通过
- `./jv_pool_main` 通过
- `ASan/UBSan` 跑 `jv_pool_test` 通过

我另外做了一个本地 O2 小基准，结果大致是：

- `alloc_nz/free`：约 `0.002247s`
- `alloc_nz + reset`：约 `0.051766s`

这版还不算“中改动”的 size-class/slab 方案，但已经把当前实现里最明显的热路径成本降下来了。工作区里还有一个已有修改的 [FIXED.md](/Users/data/dev/jv-pool/FIXED.md) 没动；`make` 重新生成了 `jv_pool_test` 二进制。
