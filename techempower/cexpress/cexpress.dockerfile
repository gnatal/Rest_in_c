FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential libpq-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY engine engine
COPY src src
COPY Makefile .
RUN make

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libpq5 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /app/tfb-cexpress /usr/local/bin/tfb-cexpress
ENV CEXPRESS_WORKERS=0
EXPOSE 8080
CMD ["tfb-cexpress"]
