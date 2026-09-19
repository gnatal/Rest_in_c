# Rest_in_c

A production-grade, full-featured REST API in pure C11 powered by the **CExpress** web engine.

Demonstrates real-world Express.js-style development in C:
- **Authentication**: Signup, Signin, Password Recovery (Forgot/Reset password), Bearer tokens & Session cookies.
- **Authorization & Roles**: Role-Based Access Control (`user`, `admin`), route guards, and permission checks.
- **Multi-Tenant / User Isolation**: User-scoped resources where users only access their own data, with administrative override.
- **Modern Security**: OpenSSL PBKDF2-HMAC-SHA256 password hashing with random per-user salts and cryptographically secure random tokens.
- **Fast JSON**: Allocation-safe parsing and high-performance streaming serialization with CExpress `JsonWriter`.
- **Zero SQLite dependency** in the core CExpress framework.

---

## Architecture

```text
Rest_in_c/
├── vendor/
│   └── cexpress/             # Clean CExpress engine (lib/ only, zero app/ conflict)
├── src/
│   ├── common/               # Crypto (PBKDF2/SHA256) & JSON utility helpers
│   ├── models/               # Data structures & thread-safe repositories
│   │   ├── types.h           # User, Role, SessionToken, PasswordResetToken, Todo
│   │   ├── user_store.h/.c   # User storage & seed admin
│   │   ├── token_store.h/.c  # Active sessions & single-use reset tokens
│   │   └── todo_store.h/.c   # User-scoped Todo CRUD store
│   ├── middleware/           # Interceptors: CORS, Logger, Bearer/Cookie Auth
│   ├── routes/               # Modular route definitions:
│   │   ├── auth_routes.h/.c  # /api/signup, /api/signin, /api/forgot_password, /api/reset_password
│   │   ├── user_routes.h/.c  # /api/user
│   │   └── todo_routes.h/.c  # /api/todos (GET, POST, PUT, DELETE)
│   └── main.c                # Server bootstrap & event loop
├── Makefile                  # Cross-platform build script
├── plan.md                   # Phased implementation roadmap
├── AGENTS.md                 # LLM / AI context & coding rules
└── test_api.sh               # Automated end-to-end integration test suite
```

---

## Quickstart

### Prerequisites
- C11 compiler (`gcc` or `clang`)
- OpenSSL (`libssl`, `libcrypto`)

### Build and Run
```bash
# Build the server
make

# Run the server on port 8080 (or override with PORT=3000)
make run
```

### Run Test Suite
Run the 16-step automated integration test suite:
```bash
make test
# Or directly:
./test_api.sh
```

---

## Default Administrator Account

The server initializes with a default admin account on startup:
- **Email**: `admin@example.com`
- **Password**: `Admin123!`
- **Role**: `admin`

---

## API Reference & cURL Examples

### 1. Healthcheck
```bash
curl http://localhost:8080/health
```
```json
{
  "status": "ok",
  "app": "Rest_in_c",
  "uptime_seconds": 42,
  "users_count": 1,
  "todos_count": 0
}
```

### 2. User Signup
```bash
curl -X POST http://localhost:8080/api/signup \
  -H "Content-Type: application/json" \
  -d '{
    "email": "alice@example.com",
    "name": "Alice Smith",
    "password": "Password123!"
  }'
```
Returns `201 Created` with a new Bearer token and sets a `session` cookie.

### 3. User Signin
```bash
curl -X POST http://localhost:8080/api/signin \
  -H "Content-Type: application/json" \
  -d '{
    "email": "alice@example.com",
    "password": "Password123!"
  }'
```

### 4. Authenticated Profile
```bash
curl http://localhost:8080/api/user \
  -H "Authorization: Bearer <YOUR_TOKEN>"
```

### 5. Password Recovery
```bash
# 1. Request reset token
curl -X POST http://localhost:8080/api/forgot_password \
  -H "Content-Type: application/json" \
  -d '{"email": "alice@example.com"}'

# 2. Reset password with token
curl -X POST http://localhost:8080/api/reset_password \
  -H "Content-Type: application/json" \
  -d '{
    "token": "<RESET_TOKEN>",
    "new_password": "NewSecretPassword456!"
  }'
```

### 6. Create Todo (Assigned to Authenticated User)
```bash
curl -X POST http://localhost:8080/api/todos \
  -H "Authorization: Bearer <YOUR_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Design REST API in C",
    "description": "Demonstrate Express.js patterns in C11"
  }'
```

### 7. List Todos (User-Scoped)
```bash
# Get only caller's todos:
curl http://localhost:8080/api/todos \
  -H "Authorization: Bearer <YOUR_TOKEN>"

# Filter by completion:
curl "http://localhost:8080/api/todos?completed=true" \
  -H "Authorization: Bearer <YOUR_TOKEN>"
```

### 8. Update and Delete Todo
```bash
# Update
curl -X PUT http://localhost:8080/api/todos/1 \
  -H "Authorization: Bearer <YOUR_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{"completed": true}'

# Delete
curl -X DELETE http://localhost:8080/api/todos/1 \
  -H "Authorization: Bearer <YOUR_TOKEN>"
```

---

## Updating the CExpress Engine

To sync the latest engine updates from `c_server`:
```bash
# From the c_server repository root:
./scripts/export_framework.sh ../Rest_in_c/vendor/cexpress
```
