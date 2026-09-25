#!/bin/bash
echo "Starting API server in background..."
rm -f app.db
./realworld_api &
SERVER_PID=$!
sleep 1

echo "Running Newman Postman Tests..."
npx -y newman run Conduit.postman_collection.json \
  --delay-request 200 \
  --global-var "APIURL=http://localhost:8080/api" \
  --global-var "USERNAME=u$(date +%s)" \
  --global-var "EMAIL=e$(date +%s)@test.com" \
  --global-var "PASSWORD=password" > postman_results.txt

kill $SERVER_PID
echo "Newman tests completed. Results saved to postman_results.txt"
