# Rest_in_c Implementation Plan & Phased Roadmap

A production-grade REST API in pure C11 powered by the **CExpress** web engine.
This document outlines the architecture, data models, endpoints, and step-by-step phases so development can proceed predictably and cleanly.

---

## 1. System Architecture Overview

```text
Rest_in_c/
├── vendor/
│   └── cexpress/             # Pure CExpress engine (lib/ only, zero app/ conflicts)
│       ├── lib/              # Event loop, HTTP parser, TLS, Router, Response, JSON
│       ├── API.md            # Quick reference index of all CExpress functions
│       ├── examples/         # cookbook.c recipes
│       └── Makefile          # Builds build/lib/libcexpress.a without SQLite
├── src/
│   ├── common/               # Shared helpers
│   │   ├── json_util.h/.c    # Standard JSON serialization & error helpers
│   │   └── crypto.h/.c       # OpenSSL PBKDF2-HMAC-SHA256 & random token generation
│   ├── models/               # Data structures & in-memory thread-safe repositories
│   │   ├── types.h           # Core structs: User, Role, SessionToken, PasswordResetToken, Todo
│   │   ├── user_store.h/.c   # Thread-safe user storage, uniqueness checks, seed admin
│   │   ├── token_store.h/.c  # Active sessions & single-use password reset tokens
│   │   └── todo_store.h/.c   # User-scoped Todo repository with CRUD operations
│   ├── middleware/           # HTTP Request/Response interceptors
│   │   ├── cors.h/.c         # Cross-Origin headers
│   │   ├── logger.h/.c       # Request logger with timestamps
│   │   └── auth.h/.c         # Bearer/Cookie token authentication & role guards
│   ├── routes/               # API route definitions
│   │   ├── auth_routes.h/.c  # Signup, Signin, Forgot Password, Reset Password
│   │   ├── user_routes.h/.c  # Current user profile
│   │   └── todo_routes.h/.c  # User-filtered and Admin-filtered Todo CRUD
│   └── main.c                # Server bootstrap, middleware registration, graceful drain
├── Makefile                  # Cross-platform build system (macOS / Linux)
├── AGENTS.md / CLAUDE.md     # AI context guidelines and memory rules
├── plan.md                   # This roadmap
└── test_api.sh               # End-to-end integration test suite
```

---

## 2. API Endpoints Specification

| Method | Endpoint | Protection | Description | Status Code |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/health` | Public | Service health & server uptime | `200` |
| `POST` | `/api/signup` | Public | Register new account (`email`, `name`, `password`, `role`) | `201` |
| `POST` | `/api/signin` | Public | Authenticate credentials; returns token & sets cookie | `200` |
| `POST` | `/api/forgot_password` | Public | Request a password reset token for account | `200` |
| `POST` | `/api/reset_password` | Public | Reset password using valid reset token | `200` |
| `GET` | `/api/user` | Authenticated | Get current authenticated user profile & role | `200` |
| `GET` | `/api/todos` | Authenticated | List todos for current user (`?completed=true\|false`) | `200` |
| `POST` | `/api/todos` | Authenticated | Create a todo assigned to current user | `201` |
| `GET` | `/api/todos/:id` | Owner / Admin | Retrieve a specific todo by ID | `200` / `403` / `404` |
| `PUT` | `/api/todos/:id` | Owner / Admin | Update title, description, or completed status | `200` / `403` / `404` |
| `DELETE`| `/api/todos/:id` | Owner / Admin | Delete a specific todo | `200` / `403` / `404` |

---

## 3. Phased Implementation Roadmap

### Phase 1: Clean Foundation & Scaffolding
- [x] Create project layout (`src/common`, `src/models`, `src/middleware`, `src/routes`)
- [x] Configure cross-platform `Makefile` with OpenSSL detection and header dependency tracking
- [x] Provide AI context files (`AGENTS.md`, `CLAUDE.md`, `.cursorrules`)
- [ ] Export CExpress engine into `vendor/cexpress` without the demo `app/` folder (Option 1)
- [x] Implement standardized JSON utilities (`src/common/json_util.h/.c`)
- [x] Implement global middlewares (CORS, Logger, JSON Error Handler)
- [x] Build and verify baseline server with `GET /health`

### Phase 2: Authentication & User Management
- [x] Core data models in `src/models/types.h` (`User`, `Role`, `SessionToken`, `PasswordResetToken`, `Todo`)
- [x] Cryptographic utilities in `src/common/crypto.h/.c` using OpenSSL PBKDF2 and secure random bytes
- [x] Thread-safe `UserStore` in `src/models/user_store.h/.c` with seeded default admin account
- [x] Thread-safe `TokenStore` in `src/models/token_store.h/.c` for active sessions and password reset tokens
- [x] Auth middlewares in `src/middleware/auth.h/.c` (`mw_authenticate`, `mw_require_admin`)
- [x] Auth route handlers in `src/routes/auth_routes.h/.c`:
  - `POST /api/signup`
  - `POST /api/signin`
  - `POST /api/forgot_password`
  - `POST /api/reset_password`
- [x] User route handler in `src/routes/user_routes.h/.c`:
  - `GET /api/user`

### Phase 3: Role-Based Access Control & User-Scoped Todos
- [x] Thread-safe `TodoStore` in `src/models/todo_store.h/.c`:
  - Scoped by `user_id` or queryable across all users
  - CRUD operations (create, read, update, delete, filter by completion)
- [ ] Todo route handlers in `src/routes/todo_routes.h/.c`:
  - `GET /api/todos`: Scoped to caller user ID (`ROLE_ADMIN` can view all or filter by user)
  - `POST /api/todos`: Assigns created todo to caller user ID
  - `GET /api/todos/:id`: Enforces ownership (owner or admin only)
  - `PUT /api/todos/:id`: Enforces ownership (owner or admin only)
  - `DELETE /api/todos/:id`: Enforces ownership (owner or admin only)
- [ ] Wire all sub-routes into `src/main.c`

### Phase 4: Automated Testing & Verification
- [ ] Create `test_api.sh` end-to-end integration test suite using `curl`:
  1. Healthcheck (`GET /health`)
  2. Public signup (`POST /api/signup`)
  3. Duplicate email conflict rejection (`409 Conflict`)
  4. Authentication (`POST /api/signin`)
  5. Password recovery flow (`POST /api/forgot_password` -> `POST /api/reset_password`)
  6. Authenticated profile (`GET /api/user`)
  7. Unauthorized access check (`401 Unauthorized` without token)
  8. User-scoped Todo lifecycle (Create, List, Update, Delete)
  9. Cross-user isolation check (`403 Forbidden` when attempting to access another user's todo)
  10. Admin override check (Admin accessing user's todo)

### Phase 5: Documentation & Polish
- [ ] Create `README.md` with full setup instructions, curl examples, and design notes.
- [ ] Verify zero memory leaks or dangling pointers.
