use axum::{
    extract::{Query, State},
    http::{header, StatusCode},
    response::Response,
    routing::get,
    Router,
};
use serde::Deserialize;
use sqlx::{postgres::PgPoolOptions, PgPool};
use std::sync::Arc;

#[derive(Deserialize)]
struct Pagination {
    limit: Option<i32>,
    offset: Option<i32>,
}

// Byte-identical to the query in ../../db_pg.c so all three servers do the same DB work.
const ARTICLES_SQL: &str = r#"SELECT json_build_object(
  'articles', COALESCE((
    SELECT json_agg(json_build_object(
      'slug', a.slug,
      'title', a.title,
      'description', a.description,
      'body', a.body,
      'createdAt', to_char(a.created_at, 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'),
      'updatedAt', to_char(a.updated_at, 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'),
      'favorited', EXISTS(SELECT 1 FROM favorites f WHERE f.article_id = a.id AND f.user_id = $1),
      'favoritesCount', (SELECT COUNT(*) FROM favorites f WHERE f.article_id = a.id),
      'author', json_build_object(
        'username', u.username,
        'bio', COALESCE(u.bio, ''),
        'image', COALESCE(u.image, ''),
        'following', EXISTS(SELECT 1 FROM follows fw WHERE fw.follower_id = $1 AND fw.followed_id = u.id)
      ),
      'tagList', COALESCE((
        SELECT json_agg(t.name)
        FROM article_tags at
        JOIN tags t ON t.id = at.tag_id
        WHERE at.article_id = a.id
      ), '[]'::json)
    ))
    FROM (
      SELECT * FROM articles
      ORDER BY created_at DESC
      LIMIT $2 OFFSET $3
    ) a
    JOIN users u ON u.id = a.author_id
  ), '[]'::json),
  'articlesCount', (SELECT article_count FROM article_stats)
)::text;"#;

async fn get_articles(
    State(pool): State<Arc<PgPool>>,
    Query(pagination): Query<Pagination>,
) -> Result<Response, StatusCode> {
    let limit = pagination.limit.unwrap_or(20);
    let offset = pagination.offset.unwrap_or(0);

    let result: (String,) = sqlx::query_as(ARTICLES_SQL)
        .bind(0)
        .bind(limit)
        .bind(offset)
        .fetch_one(&*pool)
        .await
        .map_err(|e| {
            eprintln!("DB Error: {}", e);
            StatusCode::INTERNAL_SERVER_ERROR
        })?;

    Ok(
        Response::builder()
            .header(header::CONTENT_TYPE, "application/json")
            .body(result.0.into())
            .unwrap()
    )
}

// Static baseline: measures the framework alone, no DB.
async fn ping() -> &'static str {
    "pong"
}

#[tokio::main]
async fn main() {
    let pool = PgPoolOptions::new()
        .max_connections(4) // Match CExpress 4 workers
        .connect("postgres://postgres:postgres@localhost:5432/realworld")
        .await
        .expect("Failed to connect to DB");

    let app = Router::new()
        .route("/api/ping", get(ping))
        .route("/api/articles", get(get_articles))
        .with_state(Arc::new(pool));

    let listener = tokio::net::TcpListener::bind("0.0.0.0:8083").await.unwrap();
    println!("Rust/Axum listening on :8083");
    axum::serve(listener, app).await.unwrap();
}
