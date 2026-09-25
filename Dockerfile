FROM ubuntu:24.04

# Avoid prompts from apt during build
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    pkg-config \
    libsqlite3-dev \
    libssl-dev \
    liburing-dev \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the entire project (including the 350MB realworld.db)
COPY . .

# Build the application using the Linux target (which uses io_uring for CExpress)
RUN make clean && make

EXPOSE 8080

CMD ["./realworld_api"]
