---
status: draft
# draft | reviewed | active | deprecated
type: REST
# REST | gRPC | GraphQL | WebSocket | Event | Internal

stability: experimental
# experimental | stable | deprecated
breaking_change: false
---

# API-000: [接口 / 服务名称]

> 接口契约是接口的唯一规范来源。代码与本文档不一致时，以代码为准并同步修正文档。

## 概述

- 用途：TODO，一句话说明这个接口解决什么问题、供谁调用。
- 调用方：TODO
- 被调用方 / 提供方：TODO
- 协议：TODO（HTTP/gRPC/消息队列等）
- 基础路径 / Topic / Service：TODO
- 认证方式：TODO（Bearer Token / API Key / mTLS / 无）

---

## 接口定义

> 根据协议类型保留对应章节，删除不适用的。

### REST 接口

| 方法 | 路径 | 说明 | 认证 |
|------|------|------|------|
| GET | `/TODO` | TODO | 是/否 |
| POST | `/TODO` | TODO | 是/否 |

#### 请求示例

```http
POST /TODO HTTP/1.1
Content-Type: application/json
Authorization: Bearer <token>

{
  "field_a": "TODO",
  "field_b": 0
}
```

#### 响应示例（成功）

```json
{
  "id": "TODO",
  "status": "ok"
}
```

#### 错误码

| HTTP 状态码 | 错误码 | 含义 | 处理建议 |
|-------------|--------|------|----------|
| 400 | `INVALID_PARAM` | 参数校验失败 | 检查请求字段 |
| 401 | `UNAUTHORIZED` | 未认证 | 刷新 Token |
| 429 | `RATE_LIMITED` | 触发限流 | 退避重试 |
| 500 | `INTERNAL_ERROR` | 服务内部错误 | 上报告警 |

---

### gRPC 接口

```protobuf
// TODO: 粘贴 .proto 定义
service TodoService {
  rpc GetTodo (GetTodoRequest) returns (GetTodoResponse);
}

message GetTodoRequest {
  string id = 1;
}

message GetTodoResponse {
  string id = 1;
  string status = 2;
}
```

---

### 事件 / 消息队列

| 字段 | 类型 | 说明 | 必填 |
|------|------|------|------|
| `event_type` | string | 事件类型标识 | 是 |
| `payload` | object | 业务数据 | 是 |
| `timestamp` | int64 | Unix 毫秒时间戳 | 是 |
| `trace_id` | string | 链路追踪 ID | 否 |

```json
{
  "event_type": "TODO",
  "payload": {},
  "timestamp": 1718000000000,
  "trace_id": "TODO"
}
```

Topic / Queue：`TODO`
生产方：TODO
消费方：TODO
顺序保证：TODO（无序 / 分区有序 / 全局有序）
幂等要求：TODO

---

## 数据 Schema

> 如有独立的 JSON Schema / Protobuf / Avro 文件，在此放链接；简单字段直接列表。

| 字段 | 类型 | 必填 | 默认值 | 说明 | 约束 |
|------|------|------|--------|------|------|
| `TODO` | string | 是 | - | TODO | 最大 255 字符 |
| `TODO` | int | 否 | 0 | TODO | ≥ 0 |

---

## 版本与兼容性

- 当前版本：TODO
- 最低兼容版本：TODO
- 废弃计划：TODO（如：v1 在 YYYY-MM-DD 后停止支持）
- Breaking change 规则：TODO（如：字段删除 / 类型变更需提前 N 个迭代通知）
- 变更历史：

| 版本 | 日期 | 变更内容 | 是否 Breaking |
|------|------|----------|---------------|
| v1 | YYYY-MM-DD HH:MM:SS | 初始版本 | - |

---

## 非功能要求

- 限流：TODO（如：100 req/s per client）
- 超时建议：TODO（调用方应设置的超时时间）
- 重试策略：TODO（可重试 / 不可重试，指数退避建议）
- SLA / 可用性目标：TODO
- 幂等性：TODO（是否幂等，幂等键是哪个字段）

---

## 相关文档

- Feature: `../04_features/xxx.md`
- Module: `../03_modules/xxx.md`
- LLD: `../06_lld/LLD-XXX_xxx_lld.md`
- ADR: `../07_decisions/ADR-XXX_xxx.md`
