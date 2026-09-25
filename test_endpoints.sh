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

echo "--- Testing Create Article ---"
curl -s -X POST -H "Content-Type: application/json" -H "Authorization: Token $TOKEN" -d '{"article":{"title":"My First Post","description":"This is a description","body":"This is the body of the post."}}' http://localhost:8080/api/articles > article_res.json
cat article_res.json
echo -e "\n"

SLUG=$(grep -o '"slug":"[^"]*' article_res.json | cut -d'"' -f4)

echo "--- Testing Get Article ---"
curl -s http://localhost:8080/api/articles/$SLUG
echo -e "\n"

echo "--- Testing Update Article ---"
curl -s -X PUT -H "Content-Type: application/json" -H "Authorization: Token $TOKEN" -d '{"article":{"title":"My Updated Post","description":"Updated description","body":"Updated body"}}' http://localhost:8080/api/articles/$SLUG
echo -e "\n"


echo "--- Testing Registration (Second User) ---"
RND=$RANDOM
curl -s -X POST -H "Content-Type: application/json" -d '{"user":{"username":"seconduser'$RND'","email":"second'$RND'@example.com","password":"mypassword"}}' http://localhost:8080/api/users > second_user.json
TOKEN2=$(grep -o '"token":"[^"]*' second_user.json | cut -d'"' -f4)
echo -e "\n"

echo "--- Testing Follow User ---"
curl -s -X POST -H "Authorization: Token $TOKEN2" http://localhost:8080/api/profiles/testuser/follow
echo -e "\n"

echo "--- Testing Favorite Article ---"
curl -s -X POST -H "Authorization: Token $TOKEN2" http://localhost:8080/api/articles/$SLUG/favorite
echo -e "\n"

echo "--- Testing Add Comment ---"
curl -s -X POST -H "Content-Type: application/json" -H "Authorization: Token $TOKEN2" -d '{"comment":{"body":"This is a great article!"}}' http://localhost:8080/api/articles/$SLUG/comments > comment_res.json
cat comment_res.json
echo -e "\n"

echo "--- Testing Get Comments ---"
curl -s http://localhost:8080/api/articles/$SLUG/comments
echo -e "\n"

echo "--- Testing Delete Article ---"
curl -i -s -X DELETE -H "Authorization: Token $TOKEN" http://localhost:8080/api/articles/$SLUG | head -n 1
echo -e "\n"

kill $SERVER_PID
rm -f login_res.json article_res.json second_user.json comment_res.json
