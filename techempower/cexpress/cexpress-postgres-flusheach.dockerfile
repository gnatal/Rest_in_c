# libpq 17+ from the PostgreSQL apt repository (PGDG): PQsendPipelineSync lets a worker send every query of one
# event-loop turn in one flush. Ubuntu 24.04's own libpq is 16, whose PQpipelineSync flushes per request.
FROM ubuntu:24.04 AS pgdg
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl \
    && install -d /usr/share/postgresql-common/pgdg \
    && curl -fsSL -o /usr/share/postgresql-common/pgdg/apt.postgresql.org.asc \
       https://www.postgresql.org/media/keys/ACCC4CF8.asc \
    && echo "deb [signed-by=/usr/share/postgresql-common/pgdg/apt.postgresql.org.asc] https://apt.postgresql.org/pub/repos/apt noble-pgdg main" \
       > /etc/apt/sources.list.d/pgdg.list \
    && rm -rf /var/lib/apt/lists/*

FROM pgdg AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential libpq-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY engine engine
COPY src src
COPY Makefile .
RUN make

FROM pgdg
RUN apt-get update && apt-get install -y --no-install-recommends libpq5 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /app/tfb-cexpress /usr/local/bin/tfb-cexpress
ENV CEXPRESS_DB=1
# A/B baseline, temporary: flush after every request, as with libpq 16.
ENV CEXPRESS_PG_FLUSH_EACH=1
EXPOSE 8080
# Handlers no longer block on Postgres (each worker keeps many requests' queries in flight on one pipelined
# connection), so one worker per core. CEXPRESS_WORKERS overrides it for a sweep.
CMD ["sh", "-c", "CEXPRESS_WORKERS=${CEXPRESS_WORKERS:-$(nproc)} exec tfb-cexpress"]
