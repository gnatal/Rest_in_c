# Rest_in_c - Project Guidelines & CExpress Reference for AI Assistants

This repository is a production-grade REST API implemented in C11 using the **CExpress** web engine (vendored as a Git submodule in `vendor/cexpress/`).

This document provides essential context, type references, and memory rules to enable LLMs (Antigravity, Claude, Cursor, Copilot) to generate safe, idiomatic, and high-performance code without hallucinations.

---

## 1. Essential Engine Resources to Read First

When designing or writing handlers, always refer directly to the vendored CExpress source:
1. `vendor/cexpress/lib/API.md`: 1-line index of every public CExpress function.
2. `vendor/cexpress/lib/examples/cookbook.c`: Tested recipes for path parameters, query parsing, JSON serialization, headers, and middleware.
3. `vendor/cexpress/lib/app_types.h`: Core struct definitions (`Request`, `Response`, `App`, `Router`) and compile-time limits.
4. `vendor/cexpress/lib/cexpress.h`: Primary master include.

---

## 2. Strict CExpress Ownership & Memory Rules

Failure to follow these rules will cause memory corruption, use-after-free, or memory leaks:

1. **Request Ownership**:
   - Pointers returned by `req_get_param()`, `req_get_query()`, `req_get_header()`, and `req_get_cookie()` point directly into engine memory.
   - `req->body` is managed by the CExpress engine and is freed when the handler returns.
   - **RULE**: NEVER call `free()` on any pointer accessed through `req`. NEVER retain pointers to `req->body` or params past the return of your handler. If you need data later, copy it (`strdup` or `strncpy`).

2. **Response Handling**:
   - Handlers receive `(const Request *req, Response *res)`.
   - Never allocate or free `res`. Use `res_status(res, code)`, `res_json(res, json_str)`, `res_send(res, body_str)`, or `res_set_header(res, key, val)`.
   - Exactly one terminal response must be emitted per request. A second call to `res_send` / `res_json` replaces the first.

3. **JsonWriter (Fast JSON Emission)**:
   - Prefer `JsonWriter` (`jw_*`) over building tree nodes or unsafe `snprintf`.
   - Pattern:
     ```c
     JsonWriter w;
     jw_init(&w);
     jw_object_begin(&w);
     jw_key(&w, "status");
     jw_string(&w, "ok");
     jw_object_end(&w);
     if (jw_ok(&w)) {
         res_status(res, 200);
         res_json(res, jw_data(&w));
     } else {
         res_status(res, 500);
         res_json(res, "{\"error\":\"json serialization failed\"}");
     }
     jw_free(&w); // MANDATORY: always free the writer!
     ```
   - Use our shared helper `send_json(res, status, &w)` in `src/common/json_util.h` which guarantees `jw_free(&w)` is called even on failure.

4. **JsonValue (Parsing JSON Request Bodies)**:
   - Parse incoming body with `json_parse(req->body, err_buf, sizeof(err_buf))`.
   - String values obtained via `json_as_string(val, default)` point into the parsed tree.
   - **RULE**: Always finish reading or copying strings before calling `json_free(val)`.

5. **Middleware Calling Conventions**:
   - Middleware signature: `void my_mw(const Request *req, Response *res, MiddlewareChain *chain);`
   - To pass control to the next handler: call `chain_next(chain);`.
   - To abort and return an error: call `chain_error(chain, status, message);` or call `res_status` + `res_json` directly.
   - **RULE**: Never call `chain_next` more than once, and never call `chain_next` after sending a response.

---

## 3. Project Architecture

The application is structured into modular layers:

```text
Rest_in_c/
├── vendor/
│   └── cexpress/             # Submodule containing CExpress framework
├── src/
│   ├── common/               # Shared utilities
│   │   ├── json_util.h/.c    # Standardized JSON response helpers
│   │   └── crypto.h/.c       # Password hashing & token generation (OpenSSL)
│   ├── models/               # Data structures and thread-safe repositories
│   │   ├── types.h           # Core structs: User, Role, Session, Todo
│   │   ├── user_store.h/.c   # Thread-safe user storage and lookup
│   │   ├── token_store.h/.c  # Token/session and password reset store
│   │   └── todo_store.h/.c   # User-scoped Todo storage with CRUD
│   ├── middleware/           # HTTP Middlewares
│   │   ├── cors.h/.c         # Cross-Origin Resource Sharing headers
│   │   ├── logger.h/.c       # Request logger and request ID
│   │   └── auth.h/.c         # Bearer token verification & role guards
│   ├── routes/               # API route definitions & handlers
│   │   ├── auth_routes.h/.c  # /api/signup, /api/signin, /api/forgot_password, etc.
│   │   ├── user_routes.h/.c  # /api/user
│   │   └── todo_routes.h/.c  # /api/todos (GET, POST, PUT, DELETE)
│   └── main.c                # Application entrypoint & route registration
├── Makefile                  # Cross-platform build script
├── AGENTS.md                 # This file (AI context)
└── README.md                 # User documentation & API curl examples
```

---

## 4. API Endpoints Specification

| Method | Endpoint | Auth Required | Description |
| :--- | :--- | :--- | :--- |
| `GET` | `/health` | None | Service health status & uptime |
| `POST` | `/api/signup` | None | Register new user account |
| `POST` | `/api/signin` | None | Authenticate credentials & get Bearer token |
| `POST` | `/api/forgot_password` | None | Request password reset token |
| `POST` | `/api/reset_password` | None | Reset password with token |
| `GET` | `/api/user` | User/Admin | Get current authenticated user profile |
| `GET` | `/api/todos` | User/Admin | List user's todos (Admin can list all/filter) |
| `POST` | `/api/todos` | User/Admin | Create a new todo attributed to user |
| `GET` | `/api/todos/:id` | Owner/Admin | Get specific todo by ID |
| `PUT` | `/api/todos/:id` | Owner/Admin | Update specific todo by ID |
| `DELETE` | `/api/todos/:id` | Owner/Admin | Delete specific todo by ID |

---

## 5. Build & Test Commands

- Build application: `make`
- Run application: `make run`
- Clean build: `make clean`
- Default server port: `8080` (Override with `PORT=3000 make run`)
