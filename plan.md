# Project: RealWorld (Conduit) API Implementation
## Goal
Implement the backend API specification for the RealWorld "Conduit" application (a Medium.com clone) to serve as a benchmark and integration test.

## Technical Constraints & Stack
- **Language:** C11.
- **Framework:** CExpress (a custom, lightweight, open-source HTTP server framework with Express-like routing and middleware patterns, designed for low-memory, high-throughput services).
- **Database:** SQLite3 (via raw C bindings) to minimize external dependencies during benchmarking, or PostgreSQL if preferred for concurrency.
- **Memory Management:** Strict adherence to C11 memory safety. The agent must document any heap allocations and ensure corresponding `free()` calls within the request lifecycle.
- **JSON Handling:** Use `cJSON` (or specified C library) for parsing incoming payloads and formatting the strict RealWorld JSON response structures.

## CExpress Paradigm Guide for the Agent
Since CExpress is a custom framework, adhere to the following architectural patterns:
1. **Routing:** Mount handlers similarly to Express.js (e.g., matching HTTP verbs and route parameters).
2. **Middleware:** Implement Authentication (JWT verification) as a middleware function that intercepts requests to protected routes `/api/user`, `/api/articles`, etc., before passing control to the final controller.
3. **Error Handling:** All validation errors must be caught and returned matching the exact RealWorld specification: `{"errors": {"body": ["can't be empty"]}}`.

---

## Execution Phases

### Phase 1: Core Scaffolding & User Authentication
1. Initialize the CExpress server entry point (`main.c`) and mount the base `/api` router.
2. Implement the SQLite database schema for the `users` table.
3. Build the JWT generation and verification logic in C.
4. Implement endpoints:
   - `POST /api/users` (Register)
   - `POST /api/users/login` (Login)
   - `GET /api/user` (Current User - protected)
5. **Artifact Required:** A verified Postman collection run against the User endpoints showing 200 OK and 401 Unauthorized for missing tokens.

### Phase 2: Articles & Relational Data
1. Implement the database schema for `articles`, `tags`, and the many-to-many relationship for `article_tags`.
2. Build the CRUD controllers for Articles, ensuring the routing handles slug-based URLs (`/api/articles/:slug`).
3. Implement author authorization (only the creator can update/delete an article).
4. **Artifact Required:** A brief summary of how the C struct mapping is handling the nested JSON required for the Article response (including the `author` profile object).

### Phase 3: Social Graph (Profiles, Favorites, Comments)
1. Implement the `followers` join table and the Profile endpoints (`/api/profiles/:username`).
2. Implement the `favorites` join table and endpoints (`POST/DELETE /api/articles/:slug/favorite`).
3. Implement `comments` linked to articles.
4. **Artifact Required:** Successful execution of the official RealWorld Postman backend test suite against the local CExpress instance.

-- Enable foreign key support (Crucial: The agent must execute this PRAGMA on every new SQLite connection)
PRAGMA foreign_keys = ON;

CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    email TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL,
    bio TEXT,
    image TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE articles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    slug TEXT UNIQUE NOT NULL,
    title TEXT NOT NULL,
    description TEXT NOT NULL,
    body TEXT NOT NULL,
    author_id INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (author_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE comments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    body TEXT NOT NULL,
    article_id INTEGER NOT NULL,
    author_id INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (article_id) REFERENCES articles(id) ON DELETE CASCADE,
    FOREIGN KEY (author_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL
);

-- Junction table for the Many-to-Many relationship between Articles and Tags
CREATE TABLE article_tags (
    article_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (article_id, tag_id),
    FOREIGN KEY (article_id) REFERENCES articles(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

-- Junction table for Users favoriting Articles
CREATE TABLE favorites (
    user_id INTEGER NOT NULL,
    article_id INTEGER NOT NULL,
    PRIMARY KEY (user_id, article_id),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (article_id) REFERENCES articles(id) ON DELETE CASCADE
);

-- Junction table for Users following other Users
CREATE TABLE followers (
    follower_id INTEGER NOT NULL,
    followed_id INTEGER NOT NULL,
    PRIMARY KEY (follower_id, followed_id),
    FOREIGN KEY (follower_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (followed_id) REFERENCES users(id) ON DELETE CASCADE
);