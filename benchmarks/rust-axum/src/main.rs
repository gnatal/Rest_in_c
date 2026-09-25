use axum::{
    extract::{Query, State},
    http::{header, StatusCode},
    response::{IntoResponse, Response},
    routing::get,
    Router,
};
use serde::Deserialize;
use sqlx::{sqlite::SqlitePoolOptions, SqlitePool};
use std::sync::Arc;

#[derive(Deserialize)]
struct Pagination {
    limit: Option<i32>,
    offset: Option<i32>,
}

async fn get_articles(
    State(pool): State<Arc<SqlitePool>>,
    Query(pagination): Query<Pagination>,
) -> Result<Response, StatusCode> {
    let limit = pagination.limit.unwrap_or(20);
    let offset = pagination.offset.unwrap_or(0);

    let sql = r#"
        SELECT json_object(
          'articles', COALESCE((
            SELECT json_group_array(
              json_object(
                'slug', a.slug,
                'title', a.title,
                'description', a.description,
                'body', a.body,
                'createdAt', a.created_at,
                'updatedAt', a.updated_at,
                'favorited', EXISTS(SELECT 1 FROM favorites f WHERE f.article_id = a.id AND f.user_id = ?1),
                'favoritesCount', (SELECT COUNT(*) FROM favorites f WHERE f.article_id = a.id),
                'author', json_object(
                  'username', u.username,
                  'bio', u.bio,
                  'image', u.image,
                  'following', EXISTS(SELECT 1 FROM follows fw WHERE fw.follower_id = ?1 AND fw.followed_id = u.id)
                ),
                'tagList', COALESCE((
                  SELECT json_group_array(t.name)
                  FROM article_tags at
                  JOIN tags t ON t.id = at.tag_id
                  WHERE at.article_id = a.id
                ), json_array())
              )
            )
            FROM (
              SELECT * FROM articles
              ORDER BY created_at DESC
              LIMIT ?2 OFFSET ?3
            ) a
            JOIN users u ON u.id = a.author_id
          ), json_array()),
          'articlesCount', (SELECT COUNT(*) FROM articles)
        ) AS json_result;
    "#;

    let result: (String,) = sqlx::query_as(sql)
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

#[tokio::main]
async fn main() {
    let pool = SqlitePoolOptions::new()
        .max_connections(4) // Match CExpress 4 workers
        .connect("sqlite://../../realworld.db")
        .await
        .expect("Failed to connect to DB");

    let app = Router::new()
        .route("/api/articles", get(get_articles))
        .with_state(Arc::new(pool));

    let listener = tokio::net::TcpListener::bind("0.0.0.0:8083").await.unwrap();
    println!("Rust/Axum listening on :8083");
    axum::serve(listener, app).await.unwrap();
}
