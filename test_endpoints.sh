#!/bin/bash
./realworld_api &
SERVER_PID=$!
sleep 1

echo "--- Testing Registration ---"
curl -s -X POST -H "Content-Type: application/json" -d '{"user":{"username":"testuser","email":"test@example.com","password":"mypassword"}}' http://localhost:8080/api/users
echo -e "\n"

echo "--- Testing Login ---"
curl -s -X POST -H "Content-Type: application/json" -d '{"user":{"email":"test@example.com","password":"mypassword"}}' http://localhost:8080/api/users/login > login_res.json
cat login_res.json
echo -e "\n"

TOKEN=$(grep -o '"token":"[^"]*' login_res.json | cut -d'"' -f4)

echo "--- Testing Protected Endpoint (With Token) ---"
curl -s -H "Authorization: Token $TOKEN" http://localhost:8080/api/user
echo -e "\n"

echo "--- Testing Protected Endpoint (Without Token) ---"
curl -i -s http://localhost:8080/api/user | head -n 1
echo -e "\n"

kill $SERVER_PID
rm login_res.json
