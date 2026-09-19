#!/bin/bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Rest_in_c - Automated End-to-End REST API Test Suite
# -----------------------------------------------------------------------------

TEST_PORT=8099
API_URL="http://127.0.0.1:${TEST_PORT}"
SERVER_PID=""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        echo -e "\n${BLUE}==> Stopping test server (PID: $SERVER_PID)...${NC}"
        kill "$SERVER_PID" || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

echo -e "${BLUE}=======================================================${NC}"
echo -e "${BLUE}   Rest_in_c REST API Test Suite${NC}"
echo -e "${BLUE}=======================================================${NC}"

# 1. Start Server
echo -e "\n${BLUE}[1/16] Starting server on port ${TEST_PORT}...${NC}"
PORT="${TEST_PORT}" ./rest_api > /dev/null 2>&1 &
SERVER_PID=$!

# Wait for server to accept connections
for i in {1..30}; do
    if curl -s "${API_URL}/health" > /dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

# Helper assertions
assert_status() {
    local name="$1"
    local expected="$2"
    local actual="$3"
    if [ "$expected" -eq "$actual" ]; then
        echo -e "  [PASS] ${name} (Status: ${actual})"
    else
        echo -e "  ${RED}[FAIL] ${name} - Expected ${expected}, got ${actual}${NC}"
        exit 1
    fi
}

assert_contains() {
    local name="$1"
    local needle="$2"
    local haystack="$3"
    if echo "$haystack" | grep -Fq "$needle"; then
        echo -e "  [PASS] ${name} (contains '${needle}')"
    else
        echo -e "  ${RED}[FAIL] ${name} - Missing expected substring: '${needle}'${NC}"
        echo "Response was: $haystack"
        exit 1
    fi
}

# 2. Healthcheck
echo -e "\n${BLUE}[2/16] Testing GET /health...${NC}"
HEALTH_RES=$(curl -s -w "\n%{http_code}" "${API_URL}/health")
HEALTH_BODY=$(echo "$HEALTH_RES" | sed '$d')
HEALTH_CODE=$(echo "$HEALTH_RES" | tail -n 1)
assert_status "Healthcheck status" 200 "$HEALTH_CODE"
assert_contains "Healthcheck body" '"status":"ok"' "$HEALTH_BODY"

# 3. Signup Alice
echo -e "\n${BLUE}[3/16] Testing POST /api/signup (User Alice)...${NC}"
SIGNUP_RES=$(curl -s -w "\n%{http_code}" -X POST "${API_URL}/api/signup" \
    -H "Content-Type: application/json" \
    -d '{"email":"alice@example.com","name":"Alice Smith","password":"Password123!"}')
SIGNUP_BODY=$(echo "$SIGNUP_RES" | sed '$d')
SIGNUP_CODE=$(echo "$SIGNUP_RES" | tail -n 1)
assert_status "Alice signup status" 201 "$SIGNUP_CODE"
assert_contains "Alice registered message" '"message":"user registered successfully"' "$SIGNUP_BODY"

ALICE_TOKEN=$(echo "$SIGNUP_BODY" | grep -o '"token":"[^"]*"' | cut -d'"' -f4)
if [ -z "$ALICE_TOKEN" ]; then
    echo -e "${RED}[FAIL] Could not extract Alice token${NC}"
    exit 1
fi
echo "  [PASS] Extracted Alice Auth Token: ${ALICE_TOKEN:0:16}..."

# 4. Duplicate Signup Rejection
echo -e "\n${BLUE}[4/16] Testing POST /api/signup (Duplicate Alice)...${NC}"
DUP_RES=$(curl -s -w "\n%{http_code}" -X POST "${API_URL}/api/signup" \
    -H "Content-Type: application/json" \
    -d '{"email":"alice@example.com","name":"Alice Clone","password":"Password123!"}')
DUP_BODY=$(echo "$DUP_RES" | sed '$d')
DUP_CODE=$(echo "$DUP_RES" | tail -n 1)
assert_status "Duplicate email conflict" 409 "$DUP_CODE"
assert_contains "Conflict error message" "already registered" "$DUP_BODY"

# 5. Invalid Signin
echo -e "\n${BLUE}[5/16] Testing POST /api/signin (Wrong password)...${NC}"
BAD_SIGNIN=$(curl -s -w "\n%{http_code}" -X POST "${API_URL}/api/signin" \
    -H "Content-Type: application/json" \
    -d '{"email":"alice@example.com","password":"WrongPassword"}')
BAD_CODE=$(echo "$BAD_SIGNIN" | tail -n 1)
assert_status "Wrong credentials rejected" 401 "$BAD_CODE"

# 6. Valid Signin
echo -e "\n${BLUE}[6/16] Testing POST /api/signin (Alice correct password)...${NC}"
SIGNIN_RES=$(curl -s -w "\n%{http_code}" -X POST "${API_URL}/api/signin" \
    -H "Content-Type: application/json" \
    -d '{"email":"alice@example.com","password":"Password123!"}')
SIGNIN_BODY=$(echo "$SIGNIN_RES" | sed '$d')
SIGNIN_CODE=$(echo "$SIGNIN_RES" | tail -n 1)
assert_status "Signin success" 200 "$SIGNIN_CODE"
assert_contains "Signin message" '"message":"signin successful"' "$SIGNIN_BODY"

# 7. Protected Route Without Auth
echo -e "\n${BLUE}[7/16] Testing GET /api/user (Unauthenticated)...${NC}"
UNAUTH_RES=$(curl -s -w "\n%{http_code}" "${API_URL}/api/user")
UNAUTH_CODE=$(echo "$UNAUTH_RES" | tail -n 1)
assert_status "Unauthorized access rejected" 401 "$UNAUTH_CODE"

# 8. Protected Route With Auth Token
echo -e "\n${BLUE}[8/16] Testing GET /api/user (Authenticated Alice)...${NC}"
USER_RES=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer ${ALICE_TOKEN}" "${API_URL}/api/user")
USER_BODY=$(echo "$USER_RES" | sed '$d')
USER_CODE=$(echo "$USER_RES" | tail -n 1)
assert_status "Authenticated user profile" 200 "$USER_CODE"
assert_contains "User email" '"email":"alice@example.com"' "$USER_BODY"
assert_contains "User role" '"role":"user"' "$USER_BODY"

# 9. Password Reset Flow
echo -e "\n${BLUE}[9/16] Testing Password Reset Flow (Forgot & Reset)...${NC}"
FORGOT_RES=$(curl -s -w "\n%{http_code}" -X POST "${API_URL}/api/forgot_password" \
    -H "Content-Type: application/json" \
    -d '{"email":"alice@example.com"}')
FORGOT_BODY=$(echo "$FORGOT_RES" | sed '$d')
FORGOT_CODE=$(echo "$FORGOT_RES" | tail -n 1)
assert_status "Forgot password accepted" 200 "$FORGOT_CODE"
RESET_TOKEN=$(echo "$FORGOT_BODY" | grep -o '"reset_token":"[^"]*"' | cut -d'"' -f4)
echo "  [PASS] Extracted Reset Token: ${RESET_TOKEN:0:16}..."

RESET_RES=$(curl -s -w "\n%{http_code}" -X POST "${API_URL}/api/reset_password" \
    -H "Content-Type: application/json" \
    -d "{\"token\":\"${RESET_TOKEN}\",\"new_password\":\"NewPassword456!\"}")
RESET_BODY=$(echo "$RESET_RES" | sed '$d')
RESET_CODE=$(echo "$RESET_RES" | tail -n 1)
assert_status "Password reset success" 200 "$RESET_CODE"

# Verify old password fails and new password works
OLD_PASS_TRY=$(curl -s -o /dev/null -w "%{http_code}" -X POST "${API_URL}/api/signin" \
    -H "Content-Type: application/json" \
    -d '{"email":"alice@example.com","password":"Password123!"}')
assert_status "Old password rejected" 401 "$OLD_PASS_TRY"

NEW_PASS_TRY=$(curl -s -w "\n%{http_code}" -X POST "${API_URL}/api/signin" \
    -H "Content-Type: application/json" \
    -d '{"email":"alice@example.com","password":"NewPassword456!"}')
NEW_BODY=$(echo "$NEW_PASS_TRY" | sed '$d')
NEW_CODE=$(echo "$NEW_PASS_TRY" | tail -n 1)
assert_status "New password accepted" 200 "$NEW_CODE"
ALICE_TOKEN=$(echo "$NEW_BODY" | grep -o '"token":"[^"]*"' | cut -d'"' -f4)

# 10. Create Todo (Alice)
echo -e "\n${BLUE}[10/16] Testing POST /api/todos (Alice creates a todo)...${NC}"
TODO_RES=$(curl -s -w "\n%{http_code}" -X POST "${API_URL}/api/todos" \
    -H "Authorization: Bearer ${ALICE_TOKEN}" \
    -H "Content-Type: application/json" \
    -d '{"title":"Buy groceries","description":"Milk, eggs, coffee"}')
TODO_BODY=$(echo "$TODO_RES" | sed '$d')
TODO_CODE=$(echo "$TODO_RES" | tail -n 1)
assert_status "Todo created" 201 "$TODO_CODE"
assert_contains "Todo title" '"title":"Buy groceries"' "$TODO_BODY"
TODO_ID=$(echo "$TODO_BODY" | grep -o '"id":[0-9]*' | head -n 1 | cut -d':' -f2)
echo "  [PASS] Created Todo ID: ${TODO_ID}"

# 11. List Todos (Alice)
echo -e "\n${BLUE}[11/16] Testing GET /api/todos (Alice lists her todos)...${NC}"
LIST_RES=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer ${ALICE_TOKEN}" "${API_URL}/api/todos")
LIST_BODY=$(echo "$LIST_RES" | sed '$d')
LIST_CODE=$(echo "$LIST_RES" | tail -n 1)
assert_status "Alice todo list" 200 "$LIST_CODE"
assert_contains "List contains item" "Buy groceries" "$LIST_BODY"

# 12. Multi-user Isolation (Signup Bob)
echo -e "\n${BLUE}[12/16] Testing User Isolation (Bob signs up)...${NC}"
BOB_SIGNUP=$(curl -s -X POST "${API_URL}/api/signup" \
    -H "Content-Type: application/json" \
    -d '{"email":"bob@example.com","name":"Bob Jones","password":"BobPassword123!"}')
BOB_TOKEN=$(echo "$BOB_SIGNUP" | grep -o '"token":"[^"]*"' | cut -d'"' -f4)

# Bob lists todos: must be empty []
BOB_LIST=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer ${BOB_TOKEN}" "${API_URL}/api/todos")
BOB_BODY=$(echo "$BOB_LIST" | sed '$d')
BOB_CODE=$(echo "$BOB_LIST" | tail -n 1)
assert_status "Bob sees his own empty list" 200 "$BOB_CODE"
assert_contains "Bob empty list" '[]' "$BOB_BODY"

# 13. Cross-User Authorization Guard
echo -e "\n${BLUE}[13/16] Testing Cross-User Forbidden Access (Bob views Alice's todo)...${NC}"
CROSS_RES=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer ${BOB_TOKEN}" "${API_URL}/api/todos/${TODO_ID}")
CROSS_CODE=$(echo "$CROSS_RES" | tail -n 1)
assert_status "Cross-user forbidden" 403 "$CROSS_CODE"

# 14. Admin Privilege Access
echo -e "\n${BLUE}[14/16] Testing Admin Authorization (Admin accesses Alice's todo)...${NC}"
ADMIN_SIGNIN=$(curl -s -X POST "${API_URL}/api/signin" \
    -H "Content-Type: application/json" \
    -d '{"email":"admin@example.com","password":"Admin123!"}')
ADMIN_TOKEN=$(echo "$ADMIN_SIGNIN" | grep -o '"token":"[^"]*"' | cut -d'"' -f4)

ADMIN_GET=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer ${ADMIN_TOKEN}" "${API_URL}/api/todos/${TODO_ID}")
ADMIN_CODE=$(echo "$ADMIN_GET" | tail -n 1)
assert_status "Admin allowed access to user todo" 200 "$ADMIN_CODE"

# 15. Update Todo (Alice)
echo -e "\n${BLUE}[15/16] Testing PUT /api/todos/:id (Alice updates todo)...${NC}"
PUT_RES=$(curl -s -w "\n%{http_code}" -X PUT "${API_URL}/api/todos/${TODO_ID}" \
    -H "Authorization: Bearer ${ALICE_TOKEN}" \
    -H "Content-Type: application/json" \
    -d '{"title":"Buy organic groceries","completed":true}')
PUT_BODY=$(echo "$PUT_RES" | sed '$d')
PUT_CODE=$(echo "$PUT_RES" | tail -n 1)
assert_status "Todo update status" 200 "$PUT_CODE"
assert_contains "Updated title" "Buy organic groceries" "$PUT_BODY"
assert_contains "Updated completed status" '"completed":true' "$PUT_BODY"

# 16. Delete Todo & Verify 404
echo -e "\n${BLUE}[16/16] Testing DELETE /api/todos/:id (Alice deletes todo)...${NC}"
DEL_RES=$(curl -s -w "\n%{http_code}" -X DELETE "${API_URL}/api/todos/${TODO_ID}" \
    -H "Authorization: Bearer ${ALICE_TOKEN}")
DEL_CODE=$(echo "$DEL_RES" | tail -n 1)
assert_status "Todo deleted" 200 "$DEL_CODE"

VERIFY_DEL=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer ${ALICE_TOKEN}" "${API_URL}/api/todos/${TODO_ID}")
VERIFY_CODE=$(echo "$VERIFY_DEL" | tail -n 1)
assert_status "Todo no longer exists (404)" 404 "$VERIFY_CODE"

echo -e "\n${GREEN}=======================================================${NC}"
echo -e "${GREEN}   ALL TESTS PASSED SUCCESSFULLY (16/16)!${NC}"
echo -e "${GREEN}=======================================================${NC}\n"
